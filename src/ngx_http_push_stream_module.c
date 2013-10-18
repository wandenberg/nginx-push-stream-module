/*
 * Copyright (C) 2010-2013 Wandenberg Peixoto <wandenberg@gmail.com>, Rogério Carvalho Schneider <stockrt@gmail.com>
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

    len = 7*NGX_INT_T_LEN + subtype->format_summarized->len + hostname->len + currenttime->len + ngx_strlen(subscribers_by_workers) - 21;// minus 21 sprintf

    if ((text = ngx_http_push_stream_create_str(r->pool, len)) == NULL) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "Failed to allocate response buffer.");
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    ngx_sprintf(text->data, (char *) subtype->format_summarized->data, hostname->data, currenttime->data, data->channels, data->wildcard_channels, data->published_messages, data->stored_messages, data->messages_in_trash, data->channels_in_trash, data->subscribers, ngx_time() - data->startup, subscribers_by_workers);
    text->len = ngx_strlen(text->data);

    return ngx_http_push_stream_send_response(r, text, subtype->content_type, NGX_HTTP_OK);
}


static ngx_int_t
ngx_http_push_stream_send_response_channels_info(ngx_http_request_t *r, ngx_queue_t *queue_channel_info) {
    ngx_int_t                                 rc, content_len = 0;
    ngx_chain_t                              *chain, *first = NULL, *last = NULL;
    ngx_str_t                                *currenttime, *hostname, *text, *header_response;
    ngx_queue_t                              *cur, *next;
    ngx_http_push_stream_main_conf_t         *mcf = ngx_http_get_module_main_conf(r, ngx_http_push_stream_module);
    ngx_http_push_stream_shm_data_t          *data = mcf->shm_data;
    ngx_http_push_stream_content_subtype_t   *subtype = ngx_http_push_stream_match_channel_info_format_and_content_type(r, 1);

    const ngx_str_t *format;
    const ngx_str_t *head = subtype->format_group_head;
    const ngx_str_t *tail = subtype->format_group_tail;

    // format content body
    cur = ngx_queue_head(queue_channel_info);
    while (cur != queue_channel_info) {
        next = ngx_queue_next(cur);
        ngx_http_push_stream_channel_info_t *channel_info = (ngx_http_push_stream_channel_info_t *) cur;
        if ((chain = ngx_http_push_stream_get_buf(r)) == NULL) {
            ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "push stream module: unable to allocate memory for response channels info");
            return NGX_HTTP_INTERNAL_SERVER_ERROR;
        }

        format = (next != queue_channel_info) ? subtype->format_group_item : subtype->format_group_last_item;
        if ((text = ngx_http_push_stream_channel_info_formatted(r->pool, format, &channel_info->id, channel_info->published_messages, channel_info->stored_messages, channel_info->subscribers)) == NULL) {
            ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "push stream module: unable to allocate memory to format channel info");
            return NGX_HTTP_INTERNAL_SERVER_ERROR;
        }

        chain->buf->last_buf = 0;
        chain->buf->memory = 1;
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
        cur = next;
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
    ngx_slab_pool_t                          *shpool = mcf->shpool;
    ngx_queue_t                              *cur = &data->channels_queue;
    ngx_http_push_stream_channel_t           *channel;

    ngx_queue_init(&queue_channel_info);

    ngx_shmtx_lock(&shpool->mutex);
    while ((cur = ngx_queue_next(cur)) && (cur != NULL) && (cur != &data->channels_queue)) {
        channel = ngx_queue_data(cur, ngx_http_push_stream_channel_t, queue);

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
    ngx_shmtx_unlock(&shpool->mutex);

    return ngx_http_push_stream_send_response_channels_info(r, &queue_channel_info);
}

static ngx_int_t
ngx_http_push_stream_send_response_channels_info_detailed(ngx_http_request_t *r, ngx_http_push_stream_requested_channel_t *requested_channels) {
    ngx_str_t                                *text;
    ngx_queue_t                               queue_channel_info;
    ngx_http_push_stream_main_conf_t         *mcf = ngx_http_get_module_main_conf(r, ngx_http_push_stream_module);
    ngx_slab_pool_t                          *shpool = mcf->shpool;
    ngx_http_push_stream_content_subtype_t   *subtype = ngx_http_push_stream_match_channel_info_format_and_content_type(r, 1);
    ngx_http_push_stream_channel_info_t      *channel_info;
    ngx_http_push_stream_channel_t           *channel = NULL;
    ngx_http_push_stream_requested_channel_t *requested_channel;
    ngx_queue_t                              *cur = &requested_channels->queue;
    ngx_uint_t                                qtd_channels = 0;

    ngx_queue_init(&queue_channel_info);

    ngx_shmtx_lock(&shpool->mutex);
    while ((cur = ngx_queue_next(cur)) != &requested_channels->queue) {
        requested_channel = ngx_queue_data(cur, ngx_http_push_stream_requested_channel_t, queue);

        // search for a existing channel with this id
        channel = ngx_http_push_stream_find_channel(requested_channel->id, r->connection->log, mcf);
        if ((channel != NULL) && ((channel_info = ngx_pcalloc(r->pool, sizeof(ngx_http_push_stream_channel_info_t))) != NULL)) {
            channel_info->id.data = channel->id.data;
            channel_info->id.len = channel->id.len;
            channel_info->published_messages = channel->last_message_id;
            channel_info->stored_messages = channel->stored_messages;
            channel_info->subscribers = channel->subscribers;

            ngx_queue_insert_tail(&queue_channel_info, &channel_info->queue);
            qtd_channels++;
        }
    }
    ngx_shmtx_unlock(&shpool->mutex);

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
ngx_http_push_stream_find_or_add_template(ngx_conf_t *cf,  ngx_str_t template, ngx_flag_t eventsource, ngx_flag_t websocket) {
    ngx_http_push_stream_main_conf_t      *mcf = ngx_http_conf_get_module_main_conf(cf, ngx_http_push_stream_module);
    ngx_http_push_stream_template_queue_t *sentinel = &mcf->msg_templates;
    ngx_http_push_stream_template_queue_t *cur = sentinel;
    ngx_str_t                             *aux = NULL;

    while ((cur = (ngx_http_push_stream_template_queue_t *) ngx_queue_next(&cur->queue)) != sentinel) {
        if ((ngx_memn2cmp(cur->template->data, template.data, cur->template->len, template.len) == 0) &&
            (cur->eventsource == eventsource) && (cur->websocket == websocket)) {
            return cur->index;
        }
    }

    mcf->qtd_templates++;

    cur = ngx_pcalloc(cf->pool, sizeof(ngx_http_push_stream_template_queue_t));
    aux = ngx_http_push_stream_create_str(cf->pool, template.len);
    if ((cur == NULL) || (aux == NULL)) {
        ngx_log_error(NGX_LOG_ERR, cf->log, 0, "push stream module: unable to allocate memory for add template to main configuration");
        return -1;
    }
    cur->template = aux;
    cur->eventsource = eventsource;
    cur->websocket = websocket;
    cur->index = mcf->qtd_templates;
    ngx_memcpy(cur->template->data, template.data, template.len);
    ngx_queue_insert_tail(&mcf->msg_templates.queue, &cur->queue);
    return cur->index;
}
