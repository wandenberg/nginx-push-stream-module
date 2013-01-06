/*
 * Copyright (C) 2010-2011 Wandenberg Peixoto <wandenberg@gmail.com>, Rogério Carvalho Schneider <stockrt@gmail.com>
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
 * ngx_http_push_stream_module_utils.c
 *
 * Created: Oct 26, 2010
 * Authors: Wandenberg Peixoto <wandenberg@gmail.com>, Rogério Carvalho Schneider <stockrt@gmail.com>
 */

#include <ngx_http_push_stream_module_utils.h>

static void            nxg_http_push_stream_free_channel_memory_locked(ngx_slab_pool_t *shpool, ngx_http_push_stream_channel_t *channel);
static void            ngx_http_push_stream_run_cleanup_pool_handler(ngx_pool_t *p, ngx_pool_cleanup_pt handler);
static void            ngx_http_push_stream_cleanup_request_context(ngx_http_request_t *r);
static ngx_int_t       ngx_http_push_stream_send_response_padding(ngx_http_request_t *r, size_t len, ngx_flag_t sending_header);

static ngx_inline void
ngx_http_push_stream_ensure_qtd_of_messages_locked(ngx_http_push_stream_channel_t *channel, ngx_uint_t max_messages, ngx_flag_t expired)
{
    ngx_http_push_stream_msg_t             *sentinel, *msg;

    if (max_messages == NGX_CONF_UNSET_UINT) {
        return;
    }

    sentinel = &channel->message_queue;

    while (!ngx_queue_empty(&sentinel->queue) && ((channel->stored_messages > max_messages) || expired)) {
        msg = (ngx_http_push_stream_msg_t *)ngx_queue_next(&sentinel->queue);

        if (expired && ((msg->expires == 0) || (msg->expires > ngx_time()) || (msg->workers_ref_count > 0))) {
            break;
        }

        NGX_HTTP_PUSH_STREAM_DECREMENT_COUNTER(channel->stored_messages);
        channel->last_activity_time = ngx_time();
        ngx_queue_remove(&msg->queue);
        ngx_http_push_stream_mark_message_to_delete_locked(msg);
    }

}


static void
ngx_http_push_stream_delete_channels(ngx_http_push_stream_shm_data_t *data, ngx_slab_pool_t *shpool)
{
    ngx_http_push_stream_channel_t              *channel;
    ngx_http_push_stream_pid_queue_t            *cur_worker;
    ngx_http_push_stream_queue_elem_t           *cur;
    ngx_http_push_stream_subscription_t         *cur_subscription;

    ngx_queue_t                                 *prev_channel, *cur_channel = &data->channels_to_delete;

    while ((cur_channel = ngx_queue_next(cur_channel)) != &data->channels_to_delete) {
        channel = ngx_queue_data(cur_channel, ngx_http_push_stream_channel_t, queue);

        // remove subscribers if any
        if (channel->subscribers > 0) {
            cur_worker = &channel->workers_with_subscribers;

            // find the current worker
            while ((cur_worker = (ngx_http_push_stream_pid_queue_t *) ngx_queue_next(&cur_worker->queue)) != &channel->workers_with_subscribers) {
                if (cur_worker->pid == ngx_pid) {

                    // to each subscriber of this channel in this worker
                    while (!ngx_queue_empty(&cur_worker->subscribers_sentinel.queue)) {
                        cur = (ngx_http_push_stream_queue_elem_t *) ngx_queue_next(&cur_worker->subscribers_sentinel.queue);
                        ngx_http_push_stream_subscriber_t *subscriber = (ngx_http_push_stream_subscriber_t *) cur->value;

                        // find the subscription for the channel being deleted
                        cur_subscription = &subscriber->subscriptions_sentinel;
                        while ((cur_subscription = (ngx_http_push_stream_subscription_t *) ngx_queue_next(&cur_subscription->queue)) != &subscriber->subscriptions_sentinel) {
                            if (cur_subscription->channel == channel) {

                                ngx_shmtx_lock(&shpool->mutex);
                                NGX_HTTP_PUSH_STREAM_DECREMENT_COUNTER(channel->subscribers);
                                // remove the reference from subscription for channel
                                ngx_queue_remove(&cur_subscription->queue);
                                // remove the reference from channel for subscriber
                                ngx_queue_remove(&cur->queue);
                                ngx_shmtx_unlock(&shpool->mutex);

                                if (subscriber->longpolling) {
                                    ngx_http_push_stream_add_response_header(subscriber->request, &NGX_HTTP_PUSH_STREAM_HEADER_TRANSFER_ENCODING, &NGX_HTTP_PUSH_STREAM_HEADER_CHUNCKED);
                                    ngx_http_push_stream_add_polling_headers(subscriber->request, ngx_time(), 0, subscriber->request->pool);
                                    ngx_http_send_header(subscriber->request);

                                    ngx_http_push_stream_send_response_content_header(subscriber->request, ngx_http_get_module_loc_conf(subscriber->request, ngx_http_push_stream_module));
                                }

                                ngx_http_push_stream_send_response_message(subscriber->request, channel, channel->channel_deleted_message, 1, 1);

                                break;
                            }
                        }

                        // subscriber does not have any other subscription, the connection may be closed
                        if (subscriber->longpolling || ngx_queue_empty(&subscriber->subscriptions_sentinel.queue)) {
                            ngx_http_push_stream_send_response_finalize(subscriber->request);
                        }
                    }
                }
            }
        }
    }

    ngx_shmtx_lock(&shpool->mutex);
    while (((cur_channel = ngx_queue_next(cur_channel)) != &data->channels_to_delete) && (prev_channel = ngx_queue_prev(cur_channel))) {
        channel = ngx_queue_data(cur_channel, ngx_http_push_stream_channel_t, queue);

        // channel has not subscribers and can be released
        if (channel->subscribers == 0) {
            // go back one node on queue, since the current node will be removed
            cur_channel = prev_channel;

            channel->expires = ngx_time() + ngx_http_push_stream_module_main_conf->shm_cleanup_objects_ttl;

            // move the channel to trash queue
            ngx_queue_remove(&channel->queue);
            ngx_queue_insert_tail(&data->channels_trash, &channel->queue);
            channel->queue_sentinel = &data->channels_trash;
        }
    }
    ngx_shmtx_unlock(&shpool->mutex);
}


static ngx_inline void
ngx_http_push_stream_delete_worker_channel(void)
{
    ngx_slab_pool_t                             *shpool = (ngx_slab_pool_t *) ngx_http_push_stream_shm_zone->shm.addr;
    ngx_http_push_stream_shm_data_t             *data = (ngx_http_push_stream_shm_data_t *) ngx_http_push_stream_shm_zone->data;

    ngx_http_push_stream_delete_channels(data, shpool);
}

ngx_uint_t
ngx_http_push_stream_apply_text_template(ngx_str_t **dst_value, ngx_str_t **dst_message, ngx_str_t *text, const ngx_str_t *template, const ngx_str_t *token, ngx_slab_pool_t *shpool, ngx_pool_t *temp_pool)
{
    if (text != NULL) {
        if ((*dst_value = ngx_slab_alloc_locked(shpool, sizeof(ngx_str_t) + text->len + 1)) == NULL) {
            return NGX_ERROR;
        }

        (*dst_value)->len = text->len;
        (*dst_value)->data = (u_char *) ((*dst_value) + 1);
        ngx_memcpy((*dst_value)->data, text->data, text->len);
        (*dst_value)->data[(*dst_value)->len] = '\0';

        u_char *aux = ngx_http_push_stream_str_replace(template->data, token->data, text->data, 0, temp_pool);
        if (aux == NULL) {
            return NGX_ERROR;
        }

        ngx_str_t *chunk = ngx_http_push_stream_get_formatted_chunk(aux, ngx_strlen(aux), temp_pool);
        if ((chunk == NULL) || ((*dst_message) = ngx_slab_alloc_locked(shpool, sizeof(ngx_str_t) + chunk->len + 1)) == NULL) {
            return NGX_ERROR;
        }

        (*dst_message)->len = chunk->len;
        (*dst_message)->data = (u_char *) ((*dst_message) + 1);
        ngx_memcpy((*dst_message)->data, chunk->data, (*dst_message)->len);
        (*dst_message)->data[(*dst_message)->len] = '\0';
    }

    return NGX_OK;
}

ngx_http_push_stream_msg_t *
ngx_http_push_stream_convert_char_to_msg_on_shared_locked(u_char *data, size_t len, ngx_http_push_stream_channel_t *channel, ngx_int_t id, ngx_str_t *event_id, ngx_str_t *event_type, ngx_pool_t *temp_pool)
{
    ngx_slab_pool_t                           *shpool = (ngx_slab_pool_t *) ngx_http_push_stream_shm_zone->shm.addr;
    ngx_http_push_stream_shm_data_t           *shm_data = (ngx_http_push_stream_shm_data_t *) ngx_http_push_stream_shm_zone->data;
    ngx_http_push_stream_template_queue_t     *sentinel = &ngx_http_push_stream_module_main_conf->msg_templates;
    ngx_http_push_stream_template_queue_t     *cur = sentinel;
    ngx_http_push_stream_msg_t                *msg;
    int                                        i = 0;

    if ((msg = ngx_slab_alloc_locked(shpool, sizeof(ngx_http_push_stream_msg_t))) == NULL) {
        return NULL;
    }

    msg->event_id = NULL;
    msg->event_type = NULL;
    msg->event_id_message = NULL;
    msg->event_type_message = NULL;
    msg->formatted_messages = NULL;
    msg->deleted = 0;
    msg->expires = 0;
    msg->queue.prev = NULL;
    msg->queue.next = NULL;
    msg->id = id;
    msg->workers_ref_count = 0;
    msg->time = (id == -1) ? 0 : ngx_time();
    msg->tag = (msg->time == shm_data->last_message_time) ? (shm_data->last_message_tag + 1) : 0;

    if ((msg->raw.data = ngx_slab_alloc_locked(shpool, len + 1)) == NULL) {
        ngx_http_push_stream_free_message_memory_locked(shpool, msg);
        return NULL;
    }

    msg->raw.len = len;
    // copy the message to shared memory
    ngx_memcpy(msg->raw.data, data, len);
    msg->raw.data[msg->raw.len] = '\0';


    if (ngx_http_push_stream_apply_text_template(&msg->event_id, &msg->event_id_message, event_id, &NGX_HTTP_PUSH_STREAM_EVENTSOURCE_ID_TEMPLATE, &NGX_HTTP_PUSH_STREAM_TOKEN_MESSAGE_EVENT_ID, shpool, temp_pool) != NGX_OK) {
        ngx_http_push_stream_free_message_memory_locked(shpool, msg);
        return NULL;
    }

    if (ngx_http_push_stream_apply_text_template(&msg->event_type, &msg->event_type_message, event_type, &NGX_HTTP_PUSH_STREAM_EVENTSOURCE_EVENT_TEMPLATE, &NGX_HTTP_PUSH_STREAM_TOKEN_MESSAGE_EVENT_TYPE, shpool, temp_pool) != NGX_OK) {
        ngx_http_push_stream_free_message_memory_locked(shpool, msg);
        return NULL;
    }

    if ((msg->formatted_messages = ngx_slab_alloc_locked(shpool, sizeof(ngx_str_t)*ngx_http_push_stream_module_main_conf->qtd_templates)) == NULL) {
        ngx_http_push_stream_free_message_memory_locked(shpool, msg);
        return NULL;
    }

    while ((cur = (ngx_http_push_stream_template_queue_t *) ngx_queue_next(&cur->queue)) != sentinel) {
        ngx_str_t *aux = NULL;
        if (cur->eventsource) {
            ngx_http_push_stream_line_t     *lines, *cur_line;

            if ((lines = ngx_http_push_stream_split_by_crlf(&msg->raw, temp_pool)) == NULL) {
                return NULL;
            }

            cur_line = lines;
            while ((cur_line = (ngx_http_push_stream_line_t *) ngx_queue_next(&cur_line->queue)) != lines) {
                if ((cur_line->line = ngx_http_push_stream_format_message(channel, msg, cur_line->line, cur->template, temp_pool)) == NULL) {
                    break;
                }
            }
            aux = ngx_http_push_stream_join_with_crlf(lines, temp_pool);
        } else {
            aux = ngx_http_push_stream_format_message(channel, msg, &msg->raw, cur->template, temp_pool);
        }

        if (aux == NULL) {
            ngx_http_push_stream_free_message_memory_locked(shpool, msg);
            return NULL;
        }

        ngx_str_t *text = NULL;
        if (cur->websocket) {
            text = ngx_http_push_stream_get_formatted_websocket_frame(aux->data, aux->len, temp_pool);
        } else {
            text = ngx_http_push_stream_get_formatted_chunk(aux->data, aux->len, temp_pool);
        }

        ngx_str_t *formmated = (msg->formatted_messages + i);
        if ((text == NULL) || ((formmated->data = ngx_slab_alloc_locked(shpool, text->len + 1)) == NULL)) {
            ngx_http_push_stream_free_message_memory_locked(shpool, msg);
            return NULL;
        }

        formmated->len = text->len;
        ngx_memcpy(formmated->data, text->data, formmated->len);
        formmated->data[formmated->len] = '\0';

        i++;
    }

    return msg;
}


ngx_http_push_stream_channel_t *
ngx_http_push_stream_add_msg_to_channel(ngx_http_request_t *r, ngx_str_t *id, u_char *text, size_t len, ngx_str_t *event_id, ngx_str_t *event_type, ngx_pool_t *temp_pool)
{
    ngx_http_push_stream_shm_data_t        *data = (ngx_http_push_stream_shm_data_t *) ngx_http_push_stream_shm_zone->data;
    ngx_http_push_stream_loc_conf_t        *cf = ngx_http_get_module_loc_conf(r, ngx_http_push_stream_module);
    ngx_slab_pool_t                        *shpool = (ngx_slab_pool_t *) ngx_http_push_stream_shm_zone->shm.addr;
    ngx_http_push_stream_channel_t         *channel;
    ngx_http_push_stream_msg_t             *msg;

    ngx_shmtx_lock(&shpool->mutex);

    // just find the channel. if it's not there, NULL and return error.
    channel = ngx_http_push_stream_find_channel(id, r->connection->log);
    if (channel == NULL) {
        ngx_shmtx_unlock(&(shpool)->mutex);
        ngx_log_error(NGX_LOG_ERR, (r)->connection->log, 0, "push stream module: something goes very wrong, arrived on ngx_http_push_stream_publisher_body_handler without created channel %s", id->data);
        return NULL;
    }

    // create a buffer copy in shared mem
    msg = ngx_http_push_stream_convert_char_to_msg_on_shared_locked(text, len, channel, channel->last_message_id + 1, event_id, event_type, temp_pool);
    if (msg == NULL) {
        ngx_shmtx_unlock(&(shpool)->mutex);
        ngx_log_error(NGX_LOG_ERR, (r)->connection->log, 0, "push stream module: unable to allocate message in shared memory");
        return NULL;
    }

    channel->last_message_id++;
    data->published_messages++;

    // tag message with time stamp and a sequence tag
    channel->last_message_time = data->last_message_time = msg->time;
    channel->last_message_tag = data->last_message_tag = msg->tag;
    // set message expiration time
    msg->expires = msg->time + ngx_http_push_stream_module_main_conf->message_ttl;
    channel->last_activity_time = ngx_time();

    // put messages on the queue
    if (cf->store_messages) {
        ngx_queue_insert_tail(&channel->message_queue.queue, &msg->queue);
        channel->stored_messages++;

        // now see if the queue is too big
        ngx_http_push_stream_ensure_qtd_of_messages_locked(channel, ngx_http_push_stream_module_main_conf->max_messages_stored_per_channel, 0);
    }

    ngx_shmtx_unlock(&shpool->mutex);

    // send an alert to workers
    ngx_http_push_stream_broadcast(channel, msg, r->connection->log);

    // turn on timer to cleanup buffer of old messages
    ngx_http_push_stream_buffer_cleanup_timer_set(cf);

    return channel;
}


static ngx_int_t
ngx_http_push_stream_send_only_header_response(ngx_http_request_t *r, ngx_int_t status_code, const ngx_str_t *explain_error_message)
{
    ngx_int_t rc;

    r->header_only = 1;
    r->headers_out.content_length_n = 0;
    r->headers_out.status = status_code;
    if (explain_error_message != NULL) {
        ngx_http_push_stream_add_response_header(r, &NGX_HTTP_PUSH_STREAM_HEADER_EXPLAIN, explain_error_message);
    }

    rc = ngx_http_send_header(r);

    if (rc > NGX_HTTP_SPECIAL_RESPONSE) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    return rc;
}

static ngx_table_elt_t *
ngx_http_push_stream_add_response_header(ngx_http_request_t *r, const ngx_str_t *header_name, const ngx_str_t *header_value)
{
    ngx_table_elt_t     *h = ngx_list_push(&r->headers_out.headers);


    if (h == NULL) {
        return NULL;
    }
    h->hash = 1;
    h->key.len = header_name->len;
    h->key.data = header_name->data;
    h->value.len = header_value->len;
    h->value.data = header_value->data;

    return h;
}

static ngx_str_t *
ngx_http_push_stream_get_header(ngx_http_request_t *r, const ngx_str_t *header_name)
{
    ngx_table_elt_t             *h;
    ngx_list_part_t             *part;
    ngx_uint_t                   i;
    ngx_str_t                   *aux = NULL;

    part = &r->headers_in.headers.part;
    h = part->elts;

    for (i = 0; /* void */; i++) {

        if (i >= part->nelts) {
            if (part->next == NULL) {
                break;
            }

            part = part->next;
            h = part->elts;
            i = 0;
        }

        if ((h[i].key.len == header_name->len) && (ngx_strncasecmp(h[i].key.data, header_name->data, header_name->len) == 0)) {
            aux = ngx_http_push_stream_create_str(r->pool, h[i].value.len);
            if (aux != NULL) {
                ngx_memcpy(aux->data, h[i].value.data, h[i].value.len);
            }
            break;
        }
    }

    return aux;
}

static ngx_int_t
ngx_http_push_stream_send_response_content_header(ngx_http_request_t *r, ngx_http_push_stream_loc_conf_t *pslcf)
{
    ngx_int_t rc = NGX_OK;

    if (pslcf->header_template.len > 0) {
        rc = ngx_http_push_stream_send_response_text(r, pslcf->header_template.data, pslcf->header_template.len, 0);
        if (rc == NGX_OK) {
            rc = ngx_http_push_stream_send_response_padding(r, pslcf->header_template.len, 1);
        }
    }

    return rc;
}

static ngx_int_t
ngx_http_push_stream_send_response_message(ngx_http_request_t *r, ngx_http_push_stream_channel_t *channel, ngx_http_push_stream_msg_t *msg, ngx_flag_t send_callback, ngx_flag_t send_separator)
{
    ngx_http_push_stream_loc_conf_t       *pslcf = ngx_http_get_module_loc_conf(r, ngx_http_push_stream_module);
    ngx_http_push_stream_subscriber_ctx_t *ctx = ngx_http_get_module_ctx(r, ngx_http_push_stream_module);
    ngx_flag_t                             use_jsonp = (ctx != NULL) && (ctx->callback != NULL);
    ngx_int_t rc = NGX_OK;

    if (pslcf->eventsource_support) {
        if (msg->event_id_message != NULL) {
            rc = ngx_http_push_stream_send_response_text(r, msg->event_id_message->data, msg->event_id_message->len, 0);
        }

        if ((rc == NGX_OK) && (msg->event_type_message != NULL)) {
            rc = ngx_http_push_stream_send_response_text(r, msg->event_type_message->data, msg->event_type_message->len, 0);
        }
    }

    if (rc == NGX_OK) {
        ngx_str_t *str = ngx_http_push_stream_get_formatted_message(r, channel, msg, r->pool);
        if (str != NULL) {
            if ((rc == NGX_OK) && use_jsonp && send_callback) {
                rc = ngx_http_push_stream_send_response_text(r, ctx->callback->data, ctx->callback->len, 0);
                if (rc == NGX_OK) {
                    rc = ngx_http_push_stream_send_response_text(r, NGX_HTTP_PUSH_STREAM_CALLBACK_INIT_CHUNK.data, NGX_HTTP_PUSH_STREAM_CALLBACK_INIT_CHUNK.len, 0);
                }
            }

            if (rc == NGX_OK) {
                rc = ngx_http_push_stream_send_response_text(r, str->data, str->len, 0);
            }

            if ((rc == NGX_OK) && use_jsonp) {
                if (send_separator) {
                    rc = ngx_http_push_stream_send_response_text(r, NGX_HTTP_PUSH_STREAM_CALLBACK_MID_CHUNK.data, NGX_HTTP_PUSH_STREAM_CALLBACK_MID_CHUNK.len, 0);
                }

                if (send_callback) {
                    rc = ngx_http_push_stream_send_response_text(r, NGX_HTTP_PUSH_STREAM_CALLBACK_END_CHUNK.data, NGX_HTTP_PUSH_STREAM_CALLBACK_END_CHUNK.len, 0);
                }
            }

            if (rc == NGX_OK) {
                rc = ngx_http_push_stream_send_response_padding(r, str->len, 0);
            }
        }
    }

    return rc;
}


ngx_chain_t *
ngx_http_push_stream_get_buf(ngx_http_request_t *r)
{
    ngx_http_push_stream_subscriber_ctx_t  *ctx = NULL;
    ngx_chain_t                            *out = NULL;

    if ((ctx = ngx_http_get_module_ctx(r, ngx_http_push_stream_module)) != NULL) {
        out = ngx_chain_get_free_buf(r->pool, &ctx->free);
        if (out != NULL) {
            out->buf->tag = (ngx_buf_tag_t) &ngx_http_push_stream_module;
        }
    } else {
        out = (ngx_chain_t *) ngx_pcalloc(r->pool, sizeof(ngx_chain_t));
        if (out == NULL) {
            return NULL;
        }

        out->buf = ngx_calloc_buf(r->pool);
        if (out->buf == NULL) {
            return NULL;
        }
    }

    return out;
}


ngx_int_t
ngx_http_push_stream_output_filter(ngx_http_request_t *r, ngx_chain_t *in)
{
    ngx_http_push_stream_subscriber_ctx_t  *ctx = NULL;
    ngx_int_t                               rc;

    rc = ngx_http_output_filter(r, in);

    if ((ctx = ngx_http_get_module_ctx(r, ngx_http_push_stream_module)) != NULL) {
        #if defined nginx_version && nginx_version >= 1001004
            ngx_chain_update_chains(r->pool, &ctx->free, &ctx->busy, &in, (ngx_buf_tag_t) &ngx_http_push_stream_module);
        #else
            ngx_chain_update_chains(&ctx->free, &ctx->busy, &in, (ngx_buf_tag_t) &ngx_http_push_stream_module);
        #endif
    }

    return rc;
}


static ngx_int_t
ngx_http_push_stream_send_response(ngx_http_request_t *r, ngx_str_t *text, const ngx_str_t *content_type, ngx_int_t status_code)
{
    ngx_int_t                rc;

    if ((r == NULL) || (text == NULL) || (content_type == NULL)) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    r->headers_out.content_type.len = content_type->len;
    r->headers_out.content_type.data = content_type->data;
    r->headers_out.content_length_n = text->len;

    r->headers_out.status = status_code;

    rc = ngx_http_send_header(r);

    if (rc == NGX_ERROR || rc > NGX_OK || r->header_only) {
        return rc;
    }

    return ngx_http_push_stream_send_response_text(r, text->data, text->len, 1);
}

static ngx_int_t
ngx_http_push_stream_send_response_text(ngx_http_request_t *r, const u_char *text, uint len, ngx_flag_t last_buffer)
{
    ngx_buf_t     *b;
    ngx_chain_t   *out;

    if ((text == NULL) || (r->connection->error)) {
        return NGX_ERROR;
    }

    out = ngx_http_push_stream_get_buf(r);
    if (out == NULL) {
        return NGX_ERROR;
    }

    b = out->buf;

    b->last_buf = last_buffer;
    b->last_in_chain = 1;
    b->flush = 1;
    b->memory = 1;
    b->pos = (u_char *) text;
    b->start = b->pos;
    b->end = b->pos + len;
    b->last = b->end;

    out->next = NULL;

    return ngx_http_push_stream_output_filter(r, out);
}


static ngx_int_t
ngx_http_push_stream_send_response_padding(ngx_http_request_t *r, size_t len, ngx_flag_t sending_header)
{
    ngx_http_push_stream_subscriber_ctx_t *ctx = ngx_http_get_module_ctx(r, ngx_http_push_stream_module);

    if (ctx->padding != NULL) {
        ngx_int_t diff = ((sending_header) ? ctx->padding->header_min_len : ctx->padding->message_min_len) - len;
        if (diff > 0) {
            ngx_str_t *padding = *(ngx_http_push_stream_module_paddings_chunks + diff / 100);
            ngx_http_push_stream_send_response_text(r, padding->data, padding->len, 0);
        }
    }

    return NGX_OK;
}



static void
ngx_http_push_stream_run_cleanup_pool_handler(ngx_pool_t *p, ngx_pool_cleanup_pt handler)
{
    ngx_pool_cleanup_t       *c;

    for (c = p->cleanup; c; c = c->next) {
        if ((c->handler == handler) && (c->data != NULL)) {
            c->handler(c->data);
            return;
        }
    }
}

/**
 * Should never be called inside a locked block
 * */
static void
ngx_http_push_stream_send_response_finalize(ngx_http_request_t *r)
{
    ngx_http_push_stream_loc_conf_t *pslcf = ngx_http_get_module_loc_conf(r, ngx_http_push_stream_module);
    ngx_int_t                        rc = NGX_OK;

    ngx_http_push_stream_run_cleanup_pool_handler(r->pool, (ngx_pool_cleanup_pt) ngx_http_push_stream_cleanup_request_context);

    if (pslcf->footer_template.len > 0) {
        rc = ngx_http_push_stream_send_response_text(r, pslcf->footer_template.data, pslcf->footer_template.len, 0);
    }

    if (rc == NGX_OK) {
        if (pslcf->location_type == NGX_HTTP_PUSH_STREAM_WEBSOCKET_MODE) {
            rc = ngx_http_push_stream_send_response_text(r, NGX_HTTP_PUSH_STREAM_WEBSOCKET_CLOSE_LAST_FRAME_BYTE, sizeof(NGX_HTTP_PUSH_STREAM_WEBSOCKET_CLOSE_LAST_FRAME_BYTE), 1);
        } else {
            rc = ngx_http_push_stream_send_response_text(r, NGX_HTTP_PUSH_STREAM_LAST_CHUNK.data, NGX_HTTP_PUSH_STREAM_LAST_CHUNK.len, 1);
        }
    }

    ngx_http_finalize_request(r, (rc == NGX_ERROR) ? NGX_DONE : NGX_OK);
}

static void
ngx_http_push_stream_send_response_finalize_for_longpolling_by_timeout(ngx_http_request_t *r)
{
    ngx_http_push_stream_run_cleanup_pool_handler(r->pool, (ngx_pool_cleanup_pt) ngx_http_push_stream_cleanup_request_context);

    ngx_http_push_stream_add_polling_headers(r, ngx_time(), 0, r->pool);
    ngx_http_push_stream_send_only_header_response(r, NGX_HTTP_NOT_MODIFIED, NULL);
    ngx_http_finalize_request(r, NGX_DONE);
}

static void
ngx_http_push_stream_delete_channel(ngx_str_t *id, ngx_pool_t *temp_pool)
{
    ngx_http_push_stream_channel_t         *channel;
    ngx_slab_pool_t                        *shpool = (ngx_slab_pool_t *) ngx_http_push_stream_shm_zone->shm.addr;
    ngx_http_push_stream_shm_data_t        *data = (ngx_http_push_stream_shm_data_t *) ngx_http_push_stream_shm_zone->data;
    ngx_http_push_stream_pid_queue_t       *cur;

    ngx_shmtx_lock(&shpool->mutex);

    channel = ngx_http_push_stream_find_channel(id, ngx_cycle->log);
    if (channel != NULL) {
        // remove channel from tree
        channel->deleted = 1;
        (channel->broadcast) ? NGX_HTTP_PUSH_STREAM_DECREMENT_COUNTER(data->broadcast_channels) : NGX_HTTP_PUSH_STREAM_DECREMENT_COUNTER(data->channels);

        // move the channel to unrecoverable queue
        ngx_rbtree_delete(&data->tree, &channel->node);
        ngx_queue_remove(&channel->queue);
        ngx_queue_insert_tail(&data->channels_to_delete, &channel->queue);
        channel->queue_sentinel = &data->channels_to_delete;


        // remove all messages
        ngx_http_push_stream_ensure_qtd_of_messages_locked(channel, 0, 0);

        // apply channel deleted message text to message template
        if ((channel->channel_deleted_message = ngx_http_push_stream_convert_char_to_msg_on_shared_locked(ngx_http_push_stream_module_main_conf->channel_deleted_message_text.data, ngx_http_push_stream_module_main_conf->channel_deleted_message_text.len, channel, NGX_HTTP_PUSH_STREAM_CHANNEL_DELETED_MESSAGE_ID, NULL, NULL, temp_pool)) == NULL) {
            ngx_shmtx_unlock(&(shpool)->mutex);
            ngx_log_error(NGX_LOG_ERR, temp_pool->log, 0, "push stream module: unable to allocate memory to channel deleted message");
            return;
        }

        // send signal to each worker with subscriber to this channel
        cur = &channel->workers_with_subscribers;

        while ((cur = (ngx_http_push_stream_pid_queue_t *) ngx_queue_next(&cur->queue)) != &channel->workers_with_subscribers) {
            ngx_http_push_stream_alert_worker_delete_channel(cur->pid, cur->slot, ngx_cycle->log);
        }
    }

    ngx_shmtx_unlock(&(shpool)->mutex);
}


static void
ngx_http_push_stream_collect_expired_messages_and_empty_channels(ngx_http_push_stream_shm_data_t *data, ngx_slab_pool_t *shpool, ngx_flag_t force)
{
    ngx_http_push_stream_channel_t     *channel;
    ngx_queue_t                        *prev, *cur = &data->channels_queue;

    ngx_http_push_stream_collect_expired_messages(data, shpool, force);

    while (((cur = ngx_queue_next(cur)) != &data->channels_queue) && (prev = ngx_queue_prev(cur))) {
        channel = ngx_queue_data(cur, ngx_http_push_stream_channel_t, queue);

        if (channel->queue_sentinel != &data->channels_queue) {
            break;
        }

        if ((channel->stored_messages == 0) && (channel->subscribers == 0) && (channel->last_activity_time + 30 < ngx_time())) {
            // go back one node on queue, since the current node will be removed
            cur = prev;
            ngx_shmtx_lock(&shpool->mutex);

            if (!channel->deleted) {
                channel->deleted = 1;
                channel->expires = ngx_time() + ngx_http_push_stream_module_main_conf->shm_cleanup_objects_ttl;
                (channel->broadcast) ? NGX_HTTP_PUSH_STREAM_DECREMENT_COUNTER(data->broadcast_channels) : NGX_HTTP_PUSH_STREAM_DECREMENT_COUNTER(data->channels);

                // move the channel to trash queue
                ngx_rbtree_delete(&data->tree, &channel->node);
                ngx_queue_remove(&channel->queue);
                ngx_queue_insert_tail(&data->channels_trash, &channel->queue);
                channel->queue_sentinel = &data->channels_trash;
            }

            ngx_shmtx_unlock(&shpool->mutex);
        }
    }
}


static void
ngx_http_push_stream_collect_expired_messages(ngx_http_push_stream_shm_data_t *data, ngx_slab_pool_t *shpool, ngx_flag_t force)
{
    ngx_http_push_stream_channel_t         *channel;
    ngx_queue_t                            *cur = &data->channels_queue;

    ngx_shmtx_lock(&shpool->mutex);

    while ((cur = ngx_queue_next(cur)) != &data->channels_queue) {
        channel = ngx_queue_data(cur, ngx_http_push_stream_channel_t, queue);

        ngx_http_push_stream_ensure_qtd_of_messages_locked(channel, (force) ? 0 : channel->stored_messages, 1);
    }

    ngx_shmtx_unlock(&shpool->mutex);
}


static void
ngx_http_push_stream_free_memory_of_expired_channels_locked(ngx_http_push_stream_shm_data_t *data, ngx_slab_pool_t *shpool, ngx_flag_t force)
{
    ngx_http_push_stream_channel_t         *channel;
    ngx_queue_t                            *cur;

    while ((cur = ngx_queue_head(&data->channels_trash)) != &data->channels_trash) {
        channel = ngx_queue_data(cur, ngx_http_push_stream_channel_t, queue);

        if ((ngx_time() > channel->expires) || force) {
            ngx_queue_remove(&channel->queue);
            nxg_http_push_stream_free_channel_memory_locked(shpool, channel);
        } else {
            break;
        }
    }
}


static void
nxg_http_push_stream_free_channel_memory_locked(ngx_slab_pool_t *shpool, ngx_http_push_stream_channel_t *channel)
{
    // delete the worker-subscriber queue
    ngx_http_push_stream_pid_queue_t     *cur;

    while ((cur = (ngx_http_push_stream_pid_queue_t *)ngx_queue_next(&channel->workers_with_subscribers.queue)) != &channel->workers_with_subscribers) {
        ngx_queue_remove(&cur->queue);
        ngx_slab_free_locked(shpool, cur);
    }

    if (channel->channel_deleted_message != NULL) ngx_http_push_stream_free_message_memory_locked(shpool, channel->channel_deleted_message);
    ngx_slab_free_locked(shpool, channel->id.data);
    ngx_slab_free_locked(shpool, channel);
}


static ngx_int_t
ngx_http_push_stream_memory_cleanup()
{
    ngx_slab_pool_t                        *shpool = (ngx_slab_pool_t *) ngx_http_push_stream_shm_zone->shm.addr;
    ngx_http_push_stream_shm_data_t        *data = (ngx_http_push_stream_shm_data_t *) ngx_http_push_stream_shm_zone->data;

    ngx_http_push_stream_delete_channels(data, shpool);
    ngx_http_push_stream_collect_expired_messages_and_empty_channels(data, shpool, 0);
    ngx_http_push_stream_free_memory_of_expired_messages_and_channels(0);

    return NGX_OK;
}


static ngx_int_t
ngx_http_push_stream_buffer_cleanup()
{
    ngx_slab_pool_t                        *shpool = (ngx_slab_pool_t *) ngx_http_push_stream_shm_zone->shm.addr;
    ngx_http_push_stream_shm_data_t        *data = (ngx_http_push_stream_shm_data_t *) ngx_http_push_stream_shm_zone->data;

    ngx_http_push_stream_collect_expired_messages(data, shpool, 0);

    return NGX_OK;
}


static ngx_int_t
ngx_http_push_stream_free_memory_of_expired_messages_and_channels(ngx_flag_t force)
{
    ngx_slab_pool_t                        *shpool = (ngx_slab_pool_t *) ngx_http_push_stream_shm_zone->shm.addr;
    ngx_http_push_stream_shm_data_t        *data = (ngx_http_push_stream_shm_data_t *) ngx_http_push_stream_shm_zone->data;
    ngx_http_push_stream_msg_t             *cur, *prev;

    ngx_shmtx_lock(&shpool->mutex);
    cur = &data->messages_to_delete;
    while ((cur = (ngx_http_push_stream_msg_t *)ngx_queue_next(&cur->queue)) != &data->messages_to_delete) {
        if (force || ((cur->workers_ref_count <= 0) && (ngx_time() > cur->expires))) {
            prev = (ngx_http_push_stream_msg_t *)ngx_queue_prev(&cur->queue);
            ngx_queue_remove(&cur->queue);
            ngx_http_push_stream_free_message_memory_locked(shpool, cur);
            cur = prev;
        }
    }
    ngx_http_push_stream_free_memory_of_expired_channels_locked(data, shpool, force);
    ngx_shmtx_unlock(&shpool->mutex);

    return NGX_OK;
}


static void
ngx_http_push_stream_free_message_memory_locked(ngx_slab_pool_t *shpool, ngx_http_push_stream_msg_t *msg)
{
    u_int i;

    if (msg == NULL) {
        return;
    }

    if (msg->formatted_messages != NULL) {
        for (i = 0; i < ngx_http_push_stream_module_main_conf->qtd_templates; i++) {
            ngx_str_t *formmated = (msg->formatted_messages + i);
            if ((formmated != NULL) && (formmated->data != NULL)) {
                ngx_slab_free_locked(shpool, formmated->data);
            }
        }

        ngx_slab_free_locked(shpool, msg->formatted_messages);
    }

    if (msg->raw.data != NULL) ngx_slab_free_locked(shpool, msg->raw.data);
    if (msg->event_id != NULL) ngx_slab_free_locked(shpool, msg->event_id);
    if (msg->event_type != NULL) ngx_slab_free_locked(shpool, msg->event_type);
    if (msg->event_id_message != NULL) ngx_slab_free_locked(shpool, msg->event_id_message);
    if (msg->event_type_message != NULL) ngx_slab_free_locked(shpool, msg->event_type_message);
    ngx_slab_free_locked(shpool, msg);
}


static void
ngx_http_push_stream_free_worker_message_memory_locked(ngx_slab_pool_t *shpool, ngx_http_push_stream_worker_msg_t *worker_msg)
{
    worker_msg->msg->workers_ref_count--;
    if ((worker_msg->msg->workers_ref_count <= 0) && worker_msg->msg->deleted) {
        worker_msg->msg->expires = ngx_time() + ngx_http_push_stream_module_main_conf->shm_cleanup_objects_ttl;
    }
    ngx_queue_remove(&worker_msg->queue);
    ngx_slab_free_locked(shpool, worker_msg);
}


static void
ngx_http_push_stream_mark_message_to_delete_locked(ngx_http_push_stream_msg_t *msg)
{
    ngx_http_push_stream_shm_data_t        *data = (ngx_http_push_stream_shm_data_t *) ngx_http_push_stream_shm_zone->data;

    msg->deleted = 1;
    msg->expires = ngx_time() + ngx_http_push_stream_module_main_conf->shm_cleanup_objects_ttl;
    ngx_queue_insert_tail(&data->messages_to_delete.queue, &msg->queue);
}


static void
ngx_http_push_stream_timer_set(ngx_msec_t timer_interval, ngx_event_t *event, ngx_event_handler_pt event_handler, ngx_flag_t start_timer)
{
    if ((timer_interval != NGX_CONF_UNSET_MSEC) && start_timer) {
        ngx_slab_pool_t     *shpool = (ngx_slab_pool_t *) ngx_http_push_stream_shm_zone->shm.addr;

        if (event->handler == NULL) {
            ngx_shmtx_lock(&shpool->mutex);
            if (event->handler == NULL) {
                event->handler = event_handler;
                event->data = event; //set event as data to avoid error when running on debug mode (on log event)
                event->log = ngx_cycle->log;
                ngx_http_push_stream_timer_reset(timer_interval, event);
            }
            ngx_shmtx_unlock(&shpool->mutex);
        }
    }
}


static void
ngx_http_push_stream_timer_reset(ngx_msec_t timer_interval, ngx_event_t *timer_event)
{
    if (!ngx_exiting && (timer_interval != NGX_CONF_UNSET_MSEC)) {
        if (timer_event->timedout) {
            ngx_time_update();
        }
        ngx_add_timer(timer_event, timer_interval);
    }
}


static void
ngx_http_push_stream_ping_timer_wake_handler(ngx_event_t *ev)
{
    ngx_http_request_t                 *r = (ngx_http_request_t *) ev->data;
    ngx_http_push_stream_loc_conf_t    *pslcf = ngx_http_get_module_loc_conf(r, ngx_http_push_stream_module);
    ngx_int_t                           rc;

    if (pslcf->eventsource_support) {
        rc = ngx_http_push_stream_send_response_text(r, NGX_HTTP_PUSH_STREAM_EVENTSOURCE_PING_MESSAGE_CHUNK.data, NGX_HTTP_PUSH_STREAM_EVENTSOURCE_PING_MESSAGE_CHUNK.len, 0);
    } else if (pslcf->location_type == NGX_HTTP_PUSH_STREAM_WEBSOCKET_MODE) {
        rc = ngx_http_push_stream_send_response_text(r, NGX_HTTP_PUSH_STREAM_WEBSOCKET_PING_LAST_FRAME_BYTE, sizeof(NGX_HTTP_PUSH_STREAM_WEBSOCKET_PING_LAST_FRAME_BYTE), 1);
    } else {
        rc = ngx_http_push_stream_send_response_message(r, NULL, ngx_http_push_stream_ping_msg, 1, 1);
    }

    if (rc != NGX_OK) {
        ngx_http_push_stream_send_response_finalize(r);
    } else {
        ngx_http_push_stream_subscriber_ctx_t *ctx = ngx_http_get_module_ctx(r, ngx_http_push_stream_module);
        ngx_http_push_stream_timer_reset(pslcf->ping_message_interval, ctx->ping_timer);
    }
}

static void
ngx_http_push_stream_disconnect_timer_wake_handler(ngx_event_t *ev)
{
    ngx_http_request_t                    *r = (ngx_http_request_t *) ev->data;
    ngx_http_push_stream_subscriber_ctx_t *ctx = ngx_http_get_module_ctx(r, ngx_http_push_stream_module);

    if (ctx->longpolling) {
        ngx_http_push_stream_send_response_finalize_for_longpolling_by_timeout(r);
    } else {
        ngx_http_push_stream_send_response_finalize(r);
    }
}

static void
ngx_http_push_stream_memory_cleanup_timer_wake_handler(ngx_event_t *ev)
{
    ngx_http_push_stream_memory_cleanup();
    ngx_http_push_stream_timer_reset(ngx_http_push_stream_module_main_conf->memory_cleanup_interval, &ngx_http_push_stream_memory_cleanup_event);
}

static void
ngx_http_push_stream_buffer_timer_wake_handler(ngx_event_t *ev)
{
    ngx_http_push_stream_buffer_cleanup();
    ngx_http_push_stream_timer_reset(NGX_HTTP_PUSH_STREAM_MESSAGE_BUFFER_CLEANUP_INTERVAL, &ngx_http_push_stream_buffer_cleanup_event);
}

static u_char *
ngx_http_push_stream_str_replace(u_char *org, u_char *find, u_char *replace, ngx_uint_t offset, ngx_pool_t *pool)
{
    if (org == NULL) {
        return NULL;
    }

    ngx_uint_t len_org = ngx_strlen(org);
    ngx_uint_t len_find = ngx_strlen(find);
    ngx_uint_t len_replace = ngx_strlen(replace);

    u_char      *result = org, *last;

    if (len_find > 0) {
        u_char *ret = (u_char *) ngx_strstr(org + offset, find);
        if (ret != NULL) {
            u_char *tmp = ngx_pcalloc(pool, len_org + len_replace + len_find + 1);
            if (tmp == NULL) {
                ngx_log_error(NGX_LOG_ERR, pool->log, 0, "push stream module: unable to allocate memory to apply text replace");
                return NULL;
            }

            u_int len_found = ret-org;
            ngx_memcpy(tmp, org, len_found);
            ngx_memcpy(tmp + len_found, replace, len_replace);
            last = ngx_copy(tmp + len_found + len_replace, org + len_found + len_find, len_org - len_found - len_find);
            *last = '\0';

            result = ngx_http_push_stream_str_replace(tmp, find, replace, len_found + len_replace, pool);
        }
    }

    return result;
}


static ngx_str_t *
ngx_http_push_stream_get_formatted_message(ngx_http_request_t *r, ngx_http_push_stream_channel_t *channel, ngx_http_push_stream_msg_t *message, ngx_pool_t *pool)
{
    ngx_http_push_stream_loc_conf_t        *pslcf = ngx_http_get_module_loc_conf(r, ngx_http_push_stream_module);
    if (pslcf->message_template_index > 0) {
        return message->formatted_messages + pslcf->message_template_index - 1;
    }
    return &message->raw;
}


static ngx_str_t *
ngx_http_push_stream_format_message(ngx_http_push_stream_channel_t *channel, ngx_http_push_stream_msg_t *message, ngx_str_t *text, ngx_str_t *message_template, ngx_pool_t *temp_pool)
{
    u_char                    *txt = NULL, *last;
    ngx_str_t                 *str = NULL;

    u_char char_id[NGX_INT_T_LEN + 1];
    u_char tag[NGX_INT_T_LEN + 1];
    u_char time[NGX_HTTP_PUSH_STREAM_TIME_FMT_LEN];

    u_char *channel_id = (channel != NULL) ? channel->id.data : NGX_HTTP_PUSH_STREAM_EMPTY.data;
    u_char *event_id = (message->event_id != NULL) ? message->event_id->data : NGX_HTTP_PUSH_STREAM_EMPTY.data;
    u_char *event_type = (message->event_type != NULL) ? message->event_type->data : NGX_HTTP_PUSH_STREAM_EMPTY.data;

    last = ngx_sprintf(char_id, "%d", message->id);
    *last = '\0';

    last = ngx_http_time(time, message->time);
    *last = '\0';

    last = ngx_sprintf(tag, "%d", message->tag);
    *last = '\0';

    txt = ngx_http_push_stream_str_replace(message_template->data, NGX_HTTP_PUSH_STREAM_TOKEN_MESSAGE_ID.data, char_id, 0, temp_pool);
    txt = ngx_http_push_stream_str_replace(txt, NGX_HTTP_PUSH_STREAM_TOKEN_MESSAGE_EVENT_ID.data, event_id, 0, temp_pool);
    txt = ngx_http_push_stream_str_replace(txt, NGX_HTTP_PUSH_STREAM_TOKEN_MESSAGE_EVENT_TYPE.data, event_type, 0, temp_pool);
    txt = ngx_http_push_stream_str_replace(txt, NGX_HTTP_PUSH_STREAM_TOKEN_MESSAGE_CHANNEL.data, channel_id, 0, temp_pool);
    txt = ngx_http_push_stream_str_replace(txt, NGX_HTTP_PUSH_STREAM_TOKEN_MESSAGE_TEXT.data, text->data, 0, temp_pool);
    txt = ngx_http_push_stream_str_replace(txt, NGX_HTTP_PUSH_STREAM_TOKEN_MESSAGE_TIME.data, time, 0, temp_pool);
    txt = ngx_http_push_stream_str_replace(txt, NGX_HTTP_PUSH_STREAM_TOKEN_MESSAGE_TAG.data, tag, 0, temp_pool);

    if (txt == NULL) {
        ngx_log_error(NGX_LOG_ERR, temp_pool->log, 0, "push stream module: unable to allocate memory to replace message values on template");
        return NULL;
    }

    if ((str = ngx_pcalloc(temp_pool, sizeof(ngx_str_t))) == NULL) {
        ngx_log_error(NGX_LOG_ERR, temp_pool->log, 0, "push stream module: unable to allocate memory to return message applied to template");
        return NULL;
    }

    str->data = txt;
    str->len = ngx_strlen(txt);
    return str;
}


static ngx_http_push_stream_subscriber_ctx_t *
ngx_http_push_stream_add_request_context(ngx_http_request_t *r)
{
    ngx_pool_cleanup_t                      *cln;
    ngx_http_push_stream_subscriber_ctx_t   *ctx = ngx_http_get_module_ctx(r, ngx_http_push_stream_module);

    if (ctx != NULL) {
        return ctx;
    }

    if ((ctx = ngx_pcalloc(r->pool, sizeof(ngx_http_push_stream_subscriber_ctx_t))) == NULL) {
        return NULL;
    }

    if ((cln = ngx_pool_cleanup_add(r->pool, 0)) == NULL) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "push stream module: unable to allocate memory for cleanup");
        return NULL;
    }

    if ((ctx->temp_pool = ngx_create_pool(4096, r->connection->log)) == NULL) {
        return NULL;
    }

    ctx->busy = NULL;
    ctx->free = NULL;
    ctx->disconnect_timer = NULL;
    ctx->ping_timer = NULL;
    ctx->subscriber = NULL;
    ctx->longpolling = 0;
    ctx->padding = NULL;
    ctx->callback = NULL;

    // set a cleaner to request
    cln->handler = (ngx_pool_cleanup_pt) ngx_http_push_stream_cleanup_request_context;
    cln->data = r;

    ngx_http_set_ctx(r, ctx, ngx_http_push_stream_module);

    return ctx;
}


static void
ngx_http_push_stream_cleanup_request_context(ngx_http_request_t *r)
{
    ngx_slab_pool_t                         *shpool = (ngx_slab_pool_t *) ngx_http_push_stream_shm_zone->shm.addr;
    ngx_http_push_stream_subscriber_ctx_t   *ctx = ngx_http_get_module_ctx(r, ngx_http_push_stream_module);

    if (ctx != NULL) {
        if ((ctx->disconnect_timer != NULL) && ctx->disconnect_timer->timer_set) {
            ngx_del_timer(ctx->disconnect_timer);
        }

        if ((ctx->ping_timer != NULL) && ctx->ping_timer->timer_set) {
            ngx_del_timer(ctx->ping_timer);
        }

        if (ctx->temp_pool != NULL) {
            ngx_destroy_pool(ctx->temp_pool);
            ctx->temp_pool = NULL;
        }

        if (ctx->subscriber != NULL) {
            ngx_shmtx_lock(&shpool->mutex);
            ngx_http_push_stream_worker_subscriber_cleanup_locked(ctx->subscriber);
            ctx->subscriber = NULL;
            ngx_shmtx_unlock(&shpool->mutex);
        }
    }
}


static void
ngx_http_push_stream_worker_subscriber_cleanup_locked(ngx_http_push_stream_subscriber_t *worker_subscriber)
{
    ngx_http_push_stream_subscription_t     *cur, *sentinel;
    ngx_http_push_stream_shm_data_t         *data = (ngx_http_push_stream_shm_data_t *) ngx_http_push_stream_shm_zone->data;

    sentinel = &worker_subscriber->subscriptions_sentinel;

    while ((cur = (ngx_http_push_stream_subscription_t *) ngx_queue_next(&sentinel->queue)) != sentinel) {
        NGX_HTTP_PUSH_STREAM_DECREMENT_COUNTER(cur->channel->subscribers);
        cur->channel->last_activity_time = ngx_time();
        ngx_queue_remove(&cur->channel_subscriber_element_ref->queue);
        ngx_queue_remove(&cur->queue);
    }
    ngx_queue_init(&sentinel->queue);
    if (worker_subscriber->worker_subscriber_element_ref != NULL) {
        ngx_queue_remove(&worker_subscriber->worker_subscriber_element_ref->queue);
        ngx_queue_init(&worker_subscriber->worker_subscriber_element_ref->queue);
    }
    NGX_HTTP_PUSH_STREAM_DECREMENT_COUNTER(data->subscribers);
    NGX_HTTP_PUSH_STREAM_DECREMENT_COUNTER((data->ipc + ngx_process_slot)->subscribers);
}


static ngx_http_push_stream_content_subtype_t *
ngx_http_push_stream_match_channel_info_format_and_content_type(ngx_http_request_t *r, ngx_uint_t default_subtype)
{
    ngx_uint_t      i;
    ngx_http_push_stream_content_subtype_t *subtype = &subtypes[default_subtype];

    if (r->headers_in.accept) {
        u_char     *cur = r->headers_in.accept->value.data;
        size_t      rem = 0;

        while ((cur != NULL) && (cur = ngx_strnstr(cur, "/", r->headers_in.accept->value.len)) != NULL) {
            cur = cur + 1;
            rem = r->headers_in.accept->value.len - (r->headers_in.accept->value.data - cur);

            for(i=0; i<(sizeof(subtypes) / sizeof(ngx_http_push_stream_content_subtype_t)); i++) {
                if (ngx_strncmp(cur, subtypes[i].subtype, rem < subtypes[i].len ? rem : subtypes[i].len) == 0) {
                    subtype = &subtypes[i];
                    // force break while
                    cur = NULL;
                    break;
                }
            }
        }
    }

    return subtype;
}

static ngx_str_t *
ngx_http_push_stream_get_formatted_current_time(ngx_pool_t *pool)
{
    ngx_tm_t                            tm;
    ngx_str_t                          *currenttime;

    currenttime = ngx_http_push_stream_create_str(pool, 19); //ISO 8601 pattern
    if (currenttime != NULL) {
        ngx_gmtime(ngx_time(), &tm);
        ngx_sprintf(currenttime->data, (char *) NGX_HTTP_PUSH_STREAM_DATE_FORMAT_ISO_8601.data, tm.ngx_tm_year, tm.ngx_tm_mon, tm.ngx_tm_mday, tm.ngx_tm_hour, tm.ngx_tm_min, tm.ngx_tm_sec);
    } else {
        currenttime = &NGX_HTTP_PUSH_STREAM_EMPTY;
    }

    return currenttime;
}

static ngx_str_t *
ngx_http_push_stream_get_formatted_hostname(ngx_pool_t *pool)
{
    ngx_str_t                          *hostname;

    hostname = ngx_http_push_stream_create_str(pool, sizeof(ngx_str_t) + ngx_cycle->hostname.len);
    if (hostname != NULL) {
        ngx_memcpy(hostname->data, ngx_cycle->hostname.data, ngx_cycle->hostname.len);
    } else {
        hostname = &NGX_HTTP_PUSH_STREAM_EMPTY;
    }

    return hostname;
}


static ngx_str_t *
ngx_http_push_stream_get_formatted_chunk(const u_char *text, off_t len, ngx_pool_t *temp_pool)
{
    ngx_str_t            *chunk;

    /* the "0000000000000000" is 64-bit hexadimal string */
    chunk = ngx_http_push_stream_create_str(temp_pool, sizeof("0000000000000000" CRLF CRLF CRLF) + len);
    if (chunk != NULL) {
        ngx_sprintf(chunk->data, "%xO" CRLF "%*s" CRLF CRLF, len + sizeof(CRLF) - 1, (size_t) len, text);
        chunk->len = ngx_strlen(chunk->data);
    }
    return chunk;
}


uint64_t
ngx_http_push_stream_htonll(uint64_t value) {
    int num = 42;
    if (*(char *)&num == 42) {
        uint32_t high_part = htonl((uint32_t)(value >> 32));
        uint32_t low_part = htonl((uint32_t)(value & 0xFFFFFFFFLL));
        return (((uint64_t)low_part) << 32) | high_part;
    } else {
        return value;
    }
}


uint64_t
ngx_http_push_stream_ntohll(uint64_t value) {
    int num = 42;
    if (*(char *)&num == 42) {
        uint32_t high_part = ntohl((uint32_t)(value >> 32));
        uint32_t low_part = ntohl((uint32_t)(value & 0xFFFFFFFFLL));
        return (((uint64_t)low_part) << 32) | high_part;
    } else {
        return value;
    }
}


static ngx_str_t *
ngx_http_push_stream_get_formatted_websocket_frame(const u_char *text, off_t len, ngx_pool_t *temp_pool)
{
    ngx_str_t            *frame;
    u_char               *last;

    frame = ngx_http_push_stream_create_str(temp_pool, NGX_HTTP_PUSH_STREAM_WEBSOCKET_FRAME_HEADER_MAX_LENGTH + len);
    if (frame != NULL) {
        last = ngx_copy(frame->data, &NGX_HTTP_PUSH_STREAM_WEBSOCKET_TEXT_LAST_FRAME_BYTE, sizeof(NGX_HTTP_PUSH_STREAM_WEBSOCKET_TEXT_LAST_FRAME_BYTE));

        if (len <= 125) {
            last = ngx_copy(last, &len, 1);
        } else if (len < (1 << 16)) {
            last = ngx_copy(last, &NGX_HTTP_PUSH_STREAM_WEBSOCKET_PAYLOAD_LEN_16_BYTE, sizeof(NGX_HTTP_PUSH_STREAM_WEBSOCKET_PAYLOAD_LEN_16_BYTE));
            uint16_t len_net = htons(len);
            last = ngx_copy(last, &len_net, 2);
        } else {
            last = ngx_copy(last, &NGX_HTTP_PUSH_STREAM_WEBSOCKET_PAYLOAD_LEN_64_BYTE, sizeof(NGX_HTTP_PUSH_STREAM_WEBSOCKET_PAYLOAD_LEN_64_BYTE));
            uint64_t len_net = ngx_http_push_stream_htonll(len);
            last = ngx_copy(last, &len_net, 8);
        }
        last = ngx_copy(last, text, len);
        frame->len = last - frame->data;
    }
    return frame;
}


static ngx_str_t *
ngx_http_push_stream_create_str(ngx_pool_t *pool, uint len)
{
    ngx_str_t *aux = (ngx_str_t *) ngx_pcalloc(pool, sizeof(ngx_str_t) + len + 1);
    if (aux != NULL) {
        aux->data = (u_char *) (aux + 1);
        aux->len = len;
        ngx_memset(aux->data, '\0', len + 1);
    }
    return aux;
}


static ngx_http_push_stream_line_t *
ngx_http_push_stream_add_line_to_queue(ngx_http_push_stream_line_t *sentinel, u_char *text, u_int len, ngx_pool_t *temp_pool)
{
    ngx_http_push_stream_line_t        *cur = NULL;
    ngx_str_t                          *line;
    if (len > 0) {
        cur = ngx_pcalloc(temp_pool, sizeof(ngx_http_push_stream_line_t));
        line = ngx_http_push_stream_create_str(temp_pool, len);
        if ((cur == NULL) || (line == NULL)) {
            return NULL;
        }
        cur->line = line;
        ngx_memcpy(cur->line->data, text, len);
        ngx_queue_insert_tail(&sentinel->queue, &cur->queue);
    }
    return cur;
}

static ngx_http_push_stream_line_t *
ngx_http_push_stream_split_by_crlf(ngx_str_t *msg, ngx_pool_t *temp_pool)
{
    ngx_http_push_stream_line_t        *sentinel = NULL;
    u_char                             *pos = NULL, *start = NULL, *crlf_pos, *cr_pos, *lf_pos;
    u_int                               step = 0, len = 0;

    if ((sentinel = ngx_pcalloc(temp_pool, sizeof(ngx_http_push_stream_line_t))) == NULL) {
        return NULL;
    }

    ngx_queue_init(&sentinel->queue);

    start = msg->data;
    do {
        crlf_pos = (u_char *) ngx_strstr(start, CRLF);
        cr_pos = (u_char *) ngx_strstr(start, "\r");
        lf_pos = (u_char *) ngx_strstr(start, "\n");

        pos = crlf_pos;
        step = 2;
        if ((pos == NULL) || (cr_pos < pos)) {
            pos = cr_pos;
            step = 1;
        }

        if ((pos == NULL) || (lf_pos < pos)) {
            pos = lf_pos;
            step = 1;
        }

        if (pos != NULL) {
            len = pos - start;
            if ((len > 0) && (ngx_http_push_stream_add_line_to_queue(sentinel, start, len, temp_pool) == NULL)) {
                return NULL;
            }
            start = pos + step;
        }

    } while (pos != NULL);

    len = (msg->data + msg->len) - start;
    if ((len > 0) && (ngx_http_push_stream_add_line_to_queue(sentinel, start, len, temp_pool) == NULL)) {
        return NULL;
    }

    return sentinel;
}


static ngx_str_t *
ngx_http_push_stream_join_with_crlf(ngx_http_push_stream_line_t *lines, ngx_pool_t *temp_pool)
{
    ngx_http_push_stream_line_t     *cur;
    ngx_str_t                       *result = NULL, *tmp = &NGX_HTTP_PUSH_STREAM_EMPTY;

    if (ngx_queue_empty(&lines->queue)) {
        return &NGX_HTTP_PUSH_STREAM_EMPTY;
    }

    cur = lines;
    while ((cur = (ngx_http_push_stream_line_t *) ngx_queue_next(&cur->queue)) != lines) {
        if ((cur->line == NULL) || (result = ngx_http_push_stream_create_str(temp_pool, tmp->len + cur->line->len)) == NULL) {
            return NULL;
        }

        ngx_memcpy(result->data, tmp->data, tmp->len);
        ngx_memcpy((result->data + tmp->len), cur->line->data, cur->line->len);

        tmp = result;
    }

    return result;
}


static ngx_str_t *
ngx_http_push_stream_apply_template_to_each_line(ngx_str_t *text, const ngx_str_t *message_template, ngx_pool_t *temp_pool)
{
    ngx_http_push_stream_line_t     *lines, *cur;
    ngx_str_t                       *result = NULL;

    lines = ngx_http_push_stream_split_by_crlf(text, temp_pool);
    if (lines != NULL) {
        cur = lines;
        while ((cur = (ngx_http_push_stream_line_t *) ngx_queue_next(&cur->queue)) != lines) {
            cur->line->data = ngx_http_push_stream_str_replace(message_template->data, NGX_HTTP_PUSH_STREAM_TOKEN_MESSAGE_TEXT.data, cur->line->data, 0, temp_pool);
            if (cur->line->data == NULL) {
                return NULL;
            }
            cur->line->len = ngx_strlen(cur->line->data);
        }
        result = ngx_http_push_stream_join_with_crlf(lines, temp_pool);
    }

    return result;
}

static void
ngx_http_push_stream_add_polling_headers(ngx_http_request_t *r, time_t last_modified_time, ngx_int_t tag, ngx_pool_t *temp_pool)
{
    ngx_http_push_stream_loc_conf_t                *cf = ngx_http_get_module_loc_conf(r, ngx_http_push_stream_module);
    ngx_http_push_stream_subscriber_ctx_t          *ctx = ngx_http_get_module_ctx(r, ngx_http_push_stream_module);
    ngx_str_t                                       content_type = (ctx->callback != NULL) ? NGX_HTTP_PUSH_STREAM_CALLBACK_CONTENT_TYPE : cf->content_type;

    r->headers_out.content_type = content_type;

    if (last_modified_time > 0) {
        r->headers_out.last_modified_time = last_modified_time;
    }

    if (tag >= 0) {
        ngx_str_t *etag = ngx_http_push_stream_create_str(temp_pool, NGX_INT_T_LEN);
        if (etag != NULL) {
            ngx_sprintf(etag->data, "%ui", tag);
            etag->len = ngx_strlen(etag->data);
            r->headers_out.etag = ngx_http_push_stream_add_response_header(r, &NGX_HTTP_PUSH_STREAM_HEADER_ETAG, etag);
        }
    }
}

/**
 * Copied from nginx code to only send headers added on this module code
 * */
static ngx_int_t
ngx_http_push_stream_send_only_added_headers(ngx_http_request_t *r)
{
    size_t                     len;
    ngx_str_t                 *status_line = NULL;
    ngx_buf_t                 *b;
    ngx_uint_t                 i;
    ngx_chain_t                out;
    ngx_list_part_t           *part;
    ngx_table_elt_t           *header;

    if (r->header_sent) {
        return NGX_OK;
    }

    r->header_sent = 1;

    if (r != r->main) {
        return NGX_OK;
    }

    if (r->http_version < NGX_HTTP_VERSION_10) {
        return NGX_OK;
    }

    if (r->method == NGX_HTTP_HEAD) {
        r->header_only = 1;
    }

    if (r->headers_out.last_modified_time != -1) {
        if (r->headers_out.status != NGX_HTTP_OK
            && r->headers_out.status != NGX_HTTP_PARTIAL_CONTENT
            && r->headers_out.status != NGX_HTTP_NOT_MODIFIED)
        {
            r->headers_out.last_modified_time = -1;
            r->headers_out.last_modified = NULL;
        }
    }

    len = sizeof("HTTP/1.x ") - 1 + sizeof(CRLF) - 1
          /* the end of the header */
          + sizeof(CRLF) - 1;

    /* status line */

    if (r->headers_out.status_line.len) {
        len += r->headers_out.status_line.len;
        status_line = &r->headers_out.status_line;
    }

    part = &r->headers_out.headers.part;
    header = part->elts;

    for (i = 0; /* void */; i++) {

        if (i >= part->nelts) {
            if (part->next == NULL) {
                break;
            }

            part = part->next;
            header = part->elts;
            i = 0;
        }

        if (header[i].hash == 0) {
            continue;
        }

        len += header[i].key.len + sizeof(": ") - 1 + header[i].value.len + sizeof(CRLF) - 1;
    }

    b = ngx_create_temp_buf(r->pool, len);
    if (b == NULL) {
        return NGX_ERROR;
    }

    /* "HTTP/1.x " */
    b->last = ngx_cpymem(b->last, "HTTP/1.1 ", sizeof("HTTP/1.x ") - 1);

    /* status line */
    if (status_line) {
        b->last = ngx_copy(b->last, status_line->data, status_line->len);
    }
    *b->last++ = CR; *b->last++ = LF;

    part = &r->headers_out.headers.part;
    header = part->elts;

    for (i = 0; /* void */; i++) {

        if (i >= part->nelts) {
            if (part->next == NULL) {
                break;
            }

            part = part->next;
            header = part->elts;
            i = 0;
        }

        if (header[i].hash == 0) {
            continue;
        }

        b->last = ngx_copy(b->last, header[i].key.data, header[i].key.len);
        *b->last++ = ':'; *b->last++ = ' ';

        b->last = ngx_copy(b->last, header[i].value.data, header[i].value.len);
        *b->last++ = CR; *b->last++ = LF;
    }

    /* the end of HTTP header */
    *b->last++ = CR; *b->last++ = LF;

    r->header_size = b->last - b->pos;

    if (r->header_only) {
        b->last_buf = 1;
    }

    out.buf = b;
    out.next = NULL;
    b->flush = 1;

    return ngx_http_write_filter(r, &out);
}


static ngx_http_push_stream_padding_t *
ngx_http_push_stream_parse_paddings(ngx_conf_t *cf,  ngx_str_t *paddings_by_user_agent)
{
    ngx_int_t                           rc;
    u_char                              errstr[NGX_MAX_CONF_ERRSTR];
    ngx_regex_compile_t                 padding_rc, *agent_rc;
    int                                 captures[12];
    ngx_http_push_stream_padding_t     *sentinel, *padding;
    ngx_str_t                           aux, *agent;


    if ((sentinel = ngx_palloc(cf->pool, sizeof(ngx_http_push_stream_padding_t))) == NULL) {
        ngx_conf_log_error(NGX_LOG_ERR, cf, 0, "push stream module: unable to allocate memory to save padding info");
        return NULL;
    }
    ngx_queue_init(&sentinel->queue);

    ngx_memzero(&padding_rc, sizeof(ngx_regex_compile_t));

    padding_rc.pattern = NGX_HTTP_PUSH_STREAM_PADDING_BY_USER_AGENT_PATTERN;
    padding_rc.pool = cf->pool;
    padding_rc.err.len = NGX_MAX_CONF_ERRSTR;
    padding_rc.err.data = errstr;

    if (ngx_regex_compile(&padding_rc) != NGX_OK) {
        ngx_conf_log_error(NGX_LOG_ERR, cf, 0, "push stream module: unable to compile padding pattern %V", &NGX_HTTP_PUSH_STREAM_PADDING_BY_USER_AGENT_PATTERN);
        return NULL;
    }

    aux.data = paddings_by_user_agent->data;
    aux.len = paddings_by_user_agent->len;

    do {
        rc = ngx_regex_exec(padding_rc.regex, &aux, captures, 12);
        if (rc == NGX_REGEX_NO_MATCHED) {
            ngx_conf_log_error(NGX_LOG_ERR, cf, 0, "push stream module: padding pattern not match the value %V", &aux);
            return NULL;
        }

        if (rc < 0) {
            ngx_conf_log_error(NGX_LOG_ERR, cf, 0, "push stream module: error applying padding pattern to %V", &aux);
            return NULL;
        }

        if (captures[0] != 0) {
            ngx_conf_log_error(NGX_LOG_ERR, cf, 0, "push stream module: error applying padding pattern to %V", &aux);
            return NULL;
        }

        if ((agent = ngx_http_push_stream_create_str(cf->pool, captures[3] - captures[2])) == NULL) {
            ngx_conf_log_error(NGX_LOG_ERR, cf, 0, "video security module: unable to allocate memory to copy agent pattern");
            return NGX_CONF_ERROR;
        }
        ngx_memcpy(agent->data, aux.data + captures[2], agent->len);

        if ((agent_rc = ngx_pcalloc(cf->pool, sizeof(ngx_regex_compile_t))) == NULL) {
            ngx_conf_log_error(NGX_LOG_ERR, cf, 0, "video security module: unable to allocate memory to compile agent patterns");
            return NGX_CONF_ERROR;
        }

        agent_rc->pattern = *agent;
        agent_rc->pool = cf->pool;
        agent_rc->err.len = NGX_MAX_CONF_ERRSTR;
        agent_rc->err.data = errstr;

        if (ngx_regex_compile(agent_rc) != NGX_OK) {
            ngx_conf_log_error(NGX_LOG_ERR, cf, 0, "push stream module: unable to compile agent pattern %V", &agent);
            return NULL;
        }

        if ((padding = ngx_palloc(cf->pool, sizeof(ngx_http_push_stream_padding_t))) == NULL) {
            ngx_conf_log_error(NGX_LOG_ERR, cf, 0, "push stream module: unable to allocate memory to save padding info");
            return NULL;
        }

        padding->agent = agent_rc->regex;
        padding->header_min_len = ngx_atoi(aux.data + captures[4], captures[5] - captures[4]);
        padding->message_min_len = ngx_atoi(aux.data + captures[6], captures[7] - captures[6]);

        ngx_queue_insert_tail(&sentinel->queue, &padding->queue);

        ngx_conf_log_error(NGX_LOG_INFO, cf, 0, "push stream module: padding detected %V, header_min_len %d, message_min_len %d", &agent_rc->pattern, padding->header_min_len, padding->message_min_len);

        aux.data = aux.data + (captures[1] - captures[0] + 1);
        aux.len  = aux.len - (captures[1] - captures[0] + 1);

    } while (aux.data < (paddings_by_user_agent->data + paddings_by_user_agent->len));

    return sentinel;
}


static void
ngx_http_push_stream_complex_value(ngx_http_request_t *r, ngx_http_complex_value_t *val, ngx_str_t *value)
{
    ngx_http_complex_value(r, val, value);
    ngx_http_push_stream_unescape_uri(value);
}


static void
ngx_http_push_stream_unescape_uri(ngx_str_t *value)
{
    u_char                                         *dst, *src;

    if (value->len) {
        dst = value->data;
        src = value->data;
        ngx_unescape_uri(&dst, &src, value->len, NGX_UNESCAPE_URI);
        if (dst < src) {
            *dst = '\0';
            value->len = dst - value->data;
        }
    }
}
