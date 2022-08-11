/*
 * Copyright (C) 2010-2022 Wandenberg Peixoto <wandenberg@gmail.com>, Rogério Carvalho Schneider <stockrt@gmail.com>
 *
 * This file is part of Nginx Push Stream Module.
 *
 * Nginx Push Stream Module is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Nginx Push Stream Module is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Nginx Push Stream Module.  If not, see <http://www.gnu.org/licenses/>.
 *
 *
 * ngx_http_push_stream_module.c
 *
 * Created: Oct 26, 2010
 * Authors: Wandenberg Peixoto <wandenberg@gmail.com>, Rogério Carvalho Schneider <stockrt@gmail.com>
 */

#include <ngx_http_push_stream_module.h>
#include <ngx_http_push_stream_module_setup.c>
#include <ngx_http_push_stream_rbtree_util.c>
#include <ngx_http_push_stream_module_utils.c>
#include <ngx_http_push_stream_module_ipc.c>
#include <ngx_http_push_stream_module_publisher.c>
#include <ngx_http_push_stream_module_subscriber.c>
#include <ngx_http_push_stream_module_websocket.c>

static ngx_str_t *
ngx_http_push_stream_channel_info_formatted(ngx_pool_t *pool, const ngx_str_t *format, ngx_str_t *id, ngx_uint_t published_messages, ngx_uint_t stored_messages, ngx_uint_t subscribers)
{
    ngx_str_t      *text;
    ngx_uint_t      len;

    if ((format == NULL) || (id == NULL)) {
        return NULL;
    }

    len = 3*NGX_INT_T_LEN + format->len + id->len - 11;// minus 11 sprintf

    if ((text = ngx_http_push_stream_create_str(pool, len)) == NULL) {
        return NULL;
    }

    ngx_sprintf(text->data, (char *) format->data, id->data, published_messages, stored_messages, subscribers);
    text->len = ngx_strlen(text->data);

    return text;
}


static ngx_int_t
ngx_http_push_stream_send_response_all_channels_info_summarized(ngx_http_request_t *r)
{
    ngx_uint_t                                   len;
    ngx_str_t                                   *currenttime, *hostname, *format, *text;
    u_char                                      *subscribers_by_workers, *start;
    int                                          i, j, used_slots;
    ngx_http_push_stream_main_conf_t            *mcf = ngx_http_get_module_main_conf(r, ngx_http_push_stream_module);
    ngx_http_push_stream_shm_data_t             *data = mcf->shm_data;
    ngx_http_push_stream_worker_data_t          *worker_data;
    ngx_http_push_stream_content_subtype_t      *subtype;

    subtype = ngx_http_push_stream_match_channel_info_format_and_content_type(r, 1);
    currenttime = ngx_http_push_stream_get_formatted_current_time(r->pool);
    hostname = ngx_http_push_stream_get_formatted_hostname(r->pool);

    used_slots = 0;
    for(i = 0; i < NGX_MAX_PROCESSES; i++) {
        if (data->ipc[i].pid > 0) {
            used_slots++;
        }
    }

    len = (subtype->format_summarized_worker_item->len > subtype->format_summarized_worker_last_item->len) ? subtype->format_summarized_worker_item->len : subtype->format_summarized_worker_last_item->len;
    len = used_slots * (3*NGX_INT_T_LEN + len - 8); //minus 8 sprintf
    if ((subscribers_by_workers = ngx_pcalloc(r->pool, len)) == NULL) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "Failed to allocate memory to write workers statistics.");
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }
    start = subscribers_by_workers;
    for (i = 0, j = 0; (i < used_slots) && (j < NGX_MAX_PROCESSES); j++) {
        worker_data = data->ipc + j;
        if (worker_data->pid > 0) {
            format = (i < used_slots - 1) ? subtype->format_summarized_worker_item : subtype->format_summarized_worker_last_item;
            start = ngx_sprintf(start, (char *) format->data, worker_data->pid, worker_data->subscribers, ngx_time() - worker_data->startup);
            i++;
        }
    }
    *start = '\0';

    len = 8*NGX_INT_T_LEN + subtype->format_summarized->len + hostname->len + currenttime->len + ngx_strlen(subscribers_by_workers) - 24;// minus 24 sprintf

    if ((text = ngx_http_push_stream_create_str(r->pool, len)) == NULL) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "Failed to allocate response buffer.");
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    ngx_sprintf(text->data, (char *) subtype->format_summarized->data, hostname->data, currenttime->data, data->channels, data->wildcard_channels, data->published_messages, data->stored_messages, data->messages_in_trash, data->channels_in_delete, data->channels_in_trash, data->subscribers, ngx_time() - data->startup, subscribers_by_workers);
    text->len = ngx_strlen(text->data);

    return ngx_http_push_stream_send_response(r, text, subtype->content_type, NGX_HTTP_OK);
}


static ngx_int_t
ngx_http_push_stream_send_response_channels_info(ngx_http_request_t *r, ngx_queue_t *queue_channel_info) {
    ngx_int_t                                 rc, content_len = 0;
    ngx_chain_t                              *chain, *first = NULL, *last = NULL;
    ngx_str_t                                *currenttime, *hostname, *text, *header_response;
    ngx_queue_t                              *q;
    ngx_http_push_stream_main_conf_t         *mcf = ngx_http_get_module_main_conf(r, ngx_http_push_stream_module);
    ngx_http_push_stream_shm_data_t          *data = mcf->shm_data;
    ngx_http_push_stream_content_subtype_t   *subtype = ngx_http_push_stream_match_channel_info_format_and_content_type(r, 1);

    const ngx_str_t *format;
    const ngx_str_t *head = subtype->format_group_head;
    const ngx_str_t *tail = subtype->format_group_tail;

    // format content body
    for (q = ngx_queue_head(queue_channel_info); q != ngx_queue_sentinel(queue_channel_info); q = ngx_queue_next(q)) {
        ngx_http_push_stream_channel_info_t *channel_info = ngx_queue_data(q, ngx_http_push_stream_channel_info_t, queue);
        if ((chain = ngx_http_push_stream_get_buf(r)) == NULL) {
            ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "push stream module: unable to allocate memory for response channels info");
            return NGX_HTTP_INTERNAL_SERVER_ERROR;
        }

        format = (q != ngx_queue_last(queue_channel_info)) ? subtype->format_group_item : subtype->format_group_last_item;
        if ((text = ngx_http_push_stream_channel_info_formatted(r->pool, format, &channel_info->id, channel_info->published_messages, channel_info->stored_messages, channel_info->subscribers)) == NULL) {
            ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "push stream module: unable to allocate memory to format channel info");
            return NGX_HTTP_INTERNAL_SERVER_ERROR;
        }

        chain->buf->last_buf = 0;
        chain->buf->memory = 1;
        chain->buf->temporary = 0;
        chain->buf->pos = text->data;
        chain->buf->last = text->data + text->len;
        chain->buf->start = chain->buf->pos;
        chain->buf->end = chain->buf->last;

        content_len += text->len;

        if (first == NULL) {
            first = chain;
        }

        if (last != NULL) {
            last->next = chain;
        }

        last = chain;
    }

    // get formatted current time
    currenttime = ngx_http_push_stream_get_formatted_current_time(r->pool);

    // get formatted hostname
    hostname = ngx_http_push_stream_get_formatted_hostname(r->pool);

    // format content header
    if ((header_response = ngx_http_push_stream_create_str(r->pool, head->len + hostname->len + currenttime->len + NGX_INT_T_LEN)) == NULL) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "push stream module: unable to allocate memory for response channels info");
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    ngx_sprintf(header_response->data, (char *) head->data, hostname->data, currenttime->data, data->channels, data->wildcard_channels, ngx_time() - data->startup);
    header_response->len = ngx_strlen(header_response->data);

    content_len += header_response->len + tail->len;

    r->headers_out.content_type_len = subtype->content_type->len;
    r->headers_out.content_type     = *subtype->content_type;
    r->headers_out.content_length_n = content_len;
    r->headers_out.status = NGX_HTTP_OK;

    rc = ngx_http_send_header(r);
    if (rc == NGX_ERROR || rc > NGX_OK || r->header_only) {
        return rc;
    }

    // send content header
    ngx_http_push_stream_send_response_text(r, header_response->data, header_response->len,0);
    // send content body
    if (first != NULL) {
        ngx_http_push_stream_output_filter(r, first);
    }
    // send content footer
    return ngx_http_push_stream_send_response_text(r, tail->data, tail->len, 1);
}

static ngx_int_t
ngx_http_push_stream_send_response_all_channels_info_detailed(ngx_http_request_t *r, ngx_str_t *prefix)
{
    ngx_http_push_stream_main_conf_t         *mcf = ngx_http_get_module_main_conf(r, ngx_http_push_stream_module);
    ngx_queue_t                               queue_channel_info;
    ngx_http_push_stream_shm_data_t          *data = mcf->shm_data;
    ngx_queue_t                              *q;
    ngx_http_push_stream_channel_t           *channel;

    ngx_queue_init(&queue_channel_info);

    ngx_shmtx_lock(&data->channels_queue_mutex);
    for (q = ngx_queue_head(&data->channels_queue); q != ngx_queue_sentinel(&data->channels_queue); q = ngx_queue_next(q)) {
        channel = ngx_queue_data(q, ngx_http_push_stream_channel_t, queue);

        ngx_http_push_stream_channel_info_t *channel_info;

        if(!prefix || (ngx_strncmp(channel->id.data, prefix->data, prefix->len) == 0)) {

            if ((channel_info = ngx_pcalloc(r->pool, sizeof(ngx_http_push_stream_channel_info_t))) != NULL) {
                channel_info->id.data = channel->id.data;
                channel_info->id.len = channel->id.len;
                channel_info->published_messages = channel->last_message_id;
                channel_info->stored_messages = channel->stored_messages;
                channel_info->subscribers = channel->subscribers;

                ngx_queue_insert_tail(&queue_channel_info, &channel_info->queue);
            }

        }
    }
    ngx_shmtx_unlock(&data->channels_queue_mutex);

    return ngx_http_push_stream_send_response_channels_info(r, &queue_channel_info);
}

static ngx_int_t
ngx_http_push_stream_send_response_channels_info_detailed(ngx_http_request_t *r, ngx_http_push_stream_requested_channel_t *requested_channels) {
    ngx_str_t                                *text;
    ngx_queue_t                               queue_channel_info;
    ngx_http_push_stream_content_subtype_t   *subtype = ngx_http_push_stream_match_channel_info_format_and_content_type(r, 1);
    ngx_http_push_stream_channel_info_t      *channel_info;
    ngx_http_push_stream_requested_channel_t *requested_channel;
    ngx_queue_t                              *q;
    ngx_uint_t                                qtd_channels = 0;

    ngx_queue_init(&queue_channel_info);

    for (q = ngx_queue_head(&requested_channels->queue); q != ngx_queue_sentinel(&requested_channels->queue); q = ngx_queue_next(q)) {
        requested_channel = ngx_queue_data(q, ngx_http_push_stream_requested_channel_t, queue);

        if ((requested_channel->channel != NULL) && ((channel_info = ngx_pcalloc(r->pool, sizeof(ngx_http_push_stream_channel_info_t))) != NULL)) {
            channel_info->id.data = requested_channel->channel->id.data;
            channel_info->id.len = requested_channel->channel->id.len;
            channel_info->published_messages = requested_channel->channel->last_message_id;
            channel_info->stored_messages = requested_channel->channel->stored_messages;
            channel_info->subscribers = requested_channel->channel->subscribers;

            ngx_queue_insert_tail(&queue_channel_info, &channel_info->queue);
            qtd_channels++;
        }
    }

    if (qtd_channels == 0) {
        return ngx_http_push_stream_send_only_header_response(r, NGX_HTTP_NOT_FOUND, NULL);
    }

    if (qtd_channels == 1) {
        channel_info = ngx_queue_data(ngx_queue_head(&queue_channel_info), ngx_http_push_stream_channel_info_t, queue);
        text = ngx_http_push_stream_channel_info_formatted(r->pool, subtype->format_item, &channel_info->id, channel_info->published_messages, channel_info->stored_messages, channel_info->subscribers);
        if (text == NULL) {
            ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "Failed to allocate response buffer.");
            return NGX_HTTP_INTERNAL_SERVER_ERROR;
        }

        return ngx_http_push_stream_send_response(r, text, subtype->content_type, NGX_HTTP_OK);
    }

    return ngx_http_push_stream_send_response_channels_info(r, &queue_channel_info);
}

static ngx_int_t
ngx_http_push_stream_check_and_parse_template_pattern(ngx_conf_t *cf, ngx_http_push_stream_template_t *template, u_char *last, u_char *start, const ngx_str_t *token, ngx_http_push_stream_template_part_type part_type)
{
    ngx_http_push_stream_template_parts_t *part;

    if (ngx_strncasecmp(start, token->data, token->len) == 0) {
        if ((start - last) > 0) {
            part = ngx_pcalloc(cf->pool, sizeof(ngx_http_push_stream_template_parts_t));
            if (part == NULL) {
                ngx_log_error(NGX_LOG_ERR, cf->log, 0, "push stream module: unable to allocate memory for add template part");
                return NGX_ERROR;
            }
            part->kind = PUSH_STREAM_TEMPLATE_PART_TYPE_LITERAL;
            part->text.data = last;
            part->text.len = start - last;
            template->literal_len += part->text.len;
            ngx_queue_insert_tail(&template->parts, &part->queue);
        }

        part = ngx_pcalloc(cf->pool, sizeof(ngx_http_push_stream_template_parts_t));
        if (part == NULL) {
            ngx_log_error(NGX_LOG_ERR, cf->log, 0, "push stream module: unable to allocate memory for add template part");
            return NGX_ERROR;
        }
        part->kind = part_type;
        ngx_queue_insert_tail(&template->parts, &part->queue);

        return NGX_OK;
    }

    return NGX_DECLINED;
}

static ngx_int_t
ngx_http_push_stream_find_or_add_template(ngx_conf_t *cf, ngx_str_t template, ngx_flag_t eventsource, ngx_flag_t websocket)
{
    ngx_http_push_stream_main_conf_t      *mcf = ngx_http_conf_get_module_main_conf(cf, ngx_http_push_stream_module);
    ngx_queue_t                           *q;
    ngx_http_push_stream_template_t       *cur;
    ngx_str_t                             *aux = NULL;
    u_char                                *start = NULL, *last = NULL;
    size_t                                 len = 0;
    ngx_http_push_stream_template_parts_t *part;
    ngx_int_t                              rc;

    for (q = ngx_queue_head(&mcf->msg_templates); q != ngx_queue_sentinel(&mcf->msg_templates); q = ngx_queue_next(q)) {
        cur = ngx_queue_data(q, ngx_http_push_stream_template_t, queue);
        if ((ngx_memn2cmp(cur->template->data, template.data, cur->template->len, template.len) == 0) &&
            (cur->eventsource == eventsource) && (cur->websocket == websocket)) {
            return cur->index;
        }
    }

    mcf->qtd_templates++;

    cur = ngx_pcalloc(cf->pool, sizeof(ngx_http_push_stream_template_t));
    aux = ngx_http_push_stream_create_str(cf->pool, template.len);
    if ((cur == NULL) || (aux == NULL)) {
        ngx_log_error(NGX_LOG_ERR, cf->log, 0, "push stream module: unable to allocate memory for add template to main configuration");
        return -1;
    }
    cur->template = aux;
    cur->eventsource = eventsource;
    cur->websocket = websocket;
    cur->index = mcf->qtd_templates;
    cur->qtd_message_id = 0;
    cur->qtd_event_id = 0;
    cur->qtd_event_type = 0;
    cur->qtd_channel = 0;
    cur->qtd_text = 0;
    cur->qtd_tag = 0;
    cur->qtd_time = 0;
    cur->qtd_size = 0;
    cur->literal_len = 0;
    ngx_queue_init(&cur->parts);
    ngx_memcpy(cur->template->data, template.data, template.len);
    ngx_queue_insert_tail(&mcf->msg_templates, &cur->queue);

    len = cur->template->len;
    last = start = cur->template->data;
    while ((start = ngx_strnstr(start, "~", len)) != NULL) {
        if ((rc = ngx_http_push_stream_check_and_parse_template_pattern(cf, cur, last, start, &NGX_HTTP_PUSH_STREAM_TOKEN_MESSAGE_ID, PUSH_STREAM_TEMPLATE_PART_TYPE_ID)) == NGX_OK) {
            start += NGX_HTTP_PUSH_STREAM_TOKEN_MESSAGE_ID.len;
            last = start;
            cur->qtd_message_id++;
        } else if ((rc == NGX_DECLINED) && ((rc = ngx_http_push_stream_check_and_parse_template_pattern(cf, cur, last, start, &NGX_HTTP_PUSH_STREAM_TOKEN_MESSAGE_EVENT_ID, PUSH_STREAM_TEMPLATE_PART_TYPE_EVENT_ID)) == NGX_OK)) {
            start += NGX_HTTP_PUSH_STREAM_TOKEN_MESSAGE_EVENT_ID.len;
            last = start;
            cur->qtd_event_id++;
        } else if ((rc == NGX_DECLINED) && ((rc = ngx_http_push_stream_check_and_parse_template_pattern(cf, cur, last, start, &NGX_HTTP_PUSH_STREAM_TOKEN_MESSAGE_EVENT_TYPE, PUSH_STREAM_TEMPLATE_PART_TYPE_EVENT_TYPE)) == NGX_OK)) {
            start += NGX_HTTP_PUSH_STREAM_TOKEN_MESSAGE_EVENT_TYPE.len;
            last = start;
            cur->qtd_event_type++;
        } else if ((rc == NGX_DECLINED) && ((rc = ngx_http_push_stream_check_and_parse_template_pattern(cf, cur, last, start, &NGX_HTTP_PUSH_STREAM_TOKEN_MESSAGE_CHANNEL, PUSH_STREAM_TEMPLATE_PART_TYPE_CHANNEL)) == NGX_OK)) {
            start += NGX_HTTP_PUSH_STREAM_TOKEN_MESSAGE_CHANNEL.len;
            last = start;
            cur->qtd_channel++;
        } else if ((rc == NGX_DECLINED) && ((rc = ngx_http_push_stream_check_and_parse_template_pattern(cf, cur, last, start, &NGX_HTTP_PUSH_STREAM_TOKEN_MESSAGE_TEXT, PUSH_STREAM_TEMPLATE_PART_TYPE_TEXT)) == NGX_OK)) {
            start += NGX_HTTP_PUSH_STREAM_TOKEN_MESSAGE_TEXT.len;
            last = start;
            cur->qtd_text++;
        } else if ((rc == NGX_DECLINED) && ((rc = ngx_http_push_stream_check_and_parse_template_pattern(cf, cur, last, start, &NGX_HTTP_PUSH_STREAM_TOKEN_MESSAGE_TAG, PUSH_STREAM_TEMPLATE_PART_TYPE_TAG)) == NGX_OK)) {
            start += NGX_HTTP_PUSH_STREAM_TOKEN_MESSAGE_TAG.len;
            last = start;
            cur->qtd_tag++;
        } else if ((rc == NGX_DECLINED) && ((rc = ngx_http_push_stream_check_and_parse_template_pattern(cf, cur, last, start, &NGX_HTTP_PUSH_STREAM_TOKEN_MESSAGE_TIME, PUSH_STREAM_TEMPLATE_PART_TYPE_TIME)) == NGX_OK)) {
            start += NGX_HTTP_PUSH_STREAM_TOKEN_MESSAGE_TIME.len;
            last = start;
            cur->qtd_time++;
        } else if ((rc == NGX_DECLINED) && ((rc = ngx_http_push_stream_check_and_parse_template_pattern(cf, cur, last, start, &NGX_HTTP_PUSH_STREAM_TOKEN_MESSAGE_SIZE, PUSH_STREAM_TEMPLATE_PART_TYPE_SIZE)) == NGX_OK)) {
            start += NGX_HTTP_PUSH_STREAM_TOKEN_MESSAGE_SIZE.len;
            last = start;
            cur->qtd_size++;
        } else {
            start += 1;
        }

        if (rc == NGX_ERROR) {
            return -1;
        }
    }

    if (last < (cur->template->data + cur->template->len)) {
        part = ngx_pcalloc(cf->pool, sizeof(ngx_http_push_stream_template_parts_t));
        if (part == NULL) {
            ngx_log_error(NGX_LOG_ERR, cf->log, 0, "push stream module: unable to allocate memory for add template part");
            return -1;
        }
        part->kind = PUSH_STREAM_TEMPLATE_PART_TYPE_LITERAL;
        part->text.data = last;
        part->text.len = (cur->template->data + cur->template->len) - last;
        cur->literal_len += part->text.len;
        ngx_queue_insert_tail(&cur->parts, &part->queue);
    }

    return cur->index;
}
