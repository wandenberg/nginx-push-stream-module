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
 * ngx_http_push_stream_module_utils.c
 *
 * Created: Oct 26, 2010
 * Authors: Wandenberg Peixoto <wandenberg@gmail.com>, Rogério Carvalho Schneider <stockrt@gmail.com>
 */

#include <ngx_http_push_stream_module_utils.h>

static void            nxg_http_push_stream_free_channel_memory(ngx_slab_pool_t *shpool, ngx_http_push_stream_channel_t *channel);
static void            ngx_http_push_stream_run_cleanup_pool_handler(ngx_pool_t *p, ngx_pool_cleanup_pt handler);
static void            ngx_http_push_stream_cleanup_request_context(ngx_http_request_t *r);
static ngx_int_t       ngx_http_push_stream_send_response_padding(ngx_http_request_t *r, size_t len, ngx_flag_t sending_header);
void                   ngx_http_push_stream_delete_channels_data(ngx_http_push_stream_shm_data_t *data);
void                   ngx_http_push_stream_collect_expired_messages_and_empty_channels_data(ngx_http_push_stream_shm_data_t *data, ngx_flag_t force);
void                   ngx_http_push_stream_free_memory_of_expired_messages_and_channels_data(ngx_http_push_stream_shm_data_t *data, ngx_flag_t force);
static ngx_inline void ngx_http_push_stream_cleanup_shutting_down_worker_data(ngx_http_push_stream_shm_data_t *data);
static void            ngx_http_push_stream_flush_pending_output(ngx_http_request_t *r);


ngx_uint_t
ngx_http_push_stream_ensure_qtd_of_messages(ngx_http_push_stream_shm_data_t *data, ngx_http_push_stream_channel_t *channel, ngx_uint_t max_messages, ngx_flag_t expired)
{
    ngx_http_push_stream_msg_t             *msg;
    ngx_queue_t                            *q;
    ngx_uint_t                              qtd_removed = 0;

    if (max_messages == NGX_CONF_UNSET_UINT) {
        return qtd_removed;
    }

    ngx_shmtx_lock(channel->mutex);
    while (!ngx_queue_empty(&channel->message_queue) && ((channel->stored_messages > max_messages) || expired)) {
        q = ngx_queue_head(&channel->message_queue);
        msg = ngx_queue_data(q, ngx_http_push_stream_msg_t, queue);

        if (expired && (msg->deleted || (msg->expires == 0) || (msg->expires > ngx_time()) || (msg->workers_ref_count > 0))) {
            break;
        }

        qtd_removed++;
        NGX_HTTP_PUSH_STREAM_DECREMENT_COUNTER(channel->stored_messages);
        ngx_queue_remove(&msg->queue);
        ngx_http_push_stream_throw_the_message_away(msg, data);
    }
    ngx_shmtx_unlock(channel->mutex);

    return qtd_removed;
}


static void
ngx_http_push_stream_delete_channels(void)
{
    ngx_http_push_stream_global_shm_data_t *global_data = (ngx_http_push_stream_global_shm_data_t *) ngx_http_push_stream_global_shm_zone->data;
    ngx_queue_t                            *q;

    for (q = ngx_queue_head(&global_data->shm_datas_queue); q != ngx_queue_sentinel(&global_data->shm_datas_queue); q = ngx_queue_next(q)) {
        ngx_http_push_stream_shm_data_t *data = ngx_queue_data(q, ngx_http_push_stream_shm_data_t, shm_data_queue);
        ngx_http_push_stream_delete_channels_data(data);
    }
}

void
ngx_http_push_stream_delete_channels_data(ngx_http_push_stream_shm_data_t *data)
{
    ngx_http_push_stream_main_conf_t            *mcf = data->mcf;
    ngx_http_push_stream_channel_t              *channel;
    ngx_http_push_stream_pid_queue_t            *worker, *channel_worker;
    ngx_queue_t                                 *cur_worker, *cur;
    ngx_queue_t                                 *q;

    ngx_shmtx_lock(&data->channels_to_delete_mutex);
    for (q = ngx_queue_head(&data->channels_to_delete); q != ngx_queue_sentinel(&data->channels_to_delete); q = ngx_queue_next(q)) {
        channel = ngx_queue_data(q, ngx_http_push_stream_channel_t, queue);
        worker = NULL;

        // remove subscribers if any
        if (channel->subscribers > 0) {
            ngx_shmtx_lock(channel->mutex);
            // find the current worker
            for (cur_worker = ngx_queue_head(&channel->workers_with_subscribers); cur_worker != ngx_queue_sentinel(&channel->workers_with_subscribers); cur_worker = ngx_queue_next(cur_worker)) {
                channel_worker = ngx_queue_data(cur_worker, ngx_http_push_stream_pid_queue_t, queue);
                if (channel_worker->pid == ngx_pid) {
                    worker = channel_worker;
                    break;
                }
            }
            ngx_shmtx_unlock(channel->mutex);
        }

        if (worker != NULL) {
            // to each subscription of this channel in this worker
            while (!ngx_queue_empty(&worker->subscriptions)) {
                cur = ngx_queue_head(&worker->subscriptions);
                ngx_http_push_stream_subscription_t *subscription = ngx_queue_data(cur, ngx_http_push_stream_subscription_t, channel_worker_queue);
                ngx_http_push_stream_subscriber_t *subscriber = subscription->subscriber;

                ngx_shmtx_lock(channel->mutex);
                NGX_HTTP_PUSH_STREAM_DECREMENT_COUNTER(channel->subscribers);
                NGX_HTTP_PUSH_STREAM_DECREMENT_COUNTER(worker->subscribers);
                // remove the subscription for the channel from subscriber
                ngx_queue_remove(&subscription->queue);
                // remove the subscription for the channel from worker
                ngx_queue_remove(&subscription->channel_worker_queue);
                ngx_shmtx_unlock(channel->mutex);

                ngx_http_push_stream_send_event(mcf, ngx_cycle->log, subscription->channel, &NGX_HTTP_PUSH_STREAM_EVENT_TYPE_CLIENT_UNSUBSCRIBED, subscriber->request->pool);

                if (subscriber->longpolling) {
                    ngx_http_push_stream_add_polling_headers(subscriber->request, ngx_time(), 0, subscriber->request->pool);
                    ngx_http_send_header(subscriber->request);

                    ngx_http_push_stream_send_response_content_header(subscriber->request, ngx_http_get_module_loc_conf(subscriber->request, ngx_http_push_stream_module));
                }

                ngx_http_push_stream_send_response_message(subscriber->request, channel, channel->channel_deleted_message, 1, 0);


                // subscriber does not have any other subscription, the connection may be closed
                if (subscriber->longpolling || ngx_queue_empty(&subscriber->subscriptions)) {
                    ngx_http_push_stream_send_response_finalize(subscriber->request);
                }
            }
        }
    }
    ngx_shmtx_unlock(&data->channels_to_delete_mutex);
}

void
ngx_http_push_stream_collect_deleted_channels_data(ngx_http_push_stream_shm_data_t *data)
{
    ngx_http_push_stream_main_conf_t            *mcf = data->mcf;
    ngx_http_push_stream_channel_t              *channel;
    ngx_queue_t                                 *q;
    ngx_uint_t                                   qtd_removed;
    ngx_pool_t                                  *temp_pool = NULL;

    if (mcf->events_channel_id.len > 0) {
        temp_pool = ngx_create_pool(4096, ngx_cycle->log);
    }

    ngx_shmtx_lock(&data->channels_to_delete_mutex);
    for (q = ngx_queue_head(&data->channels_to_delete); q != ngx_queue_sentinel(&data->channels_to_delete);) {
        channel = ngx_queue_data(q, ngx_http_push_stream_channel_t, queue);
        q = ngx_queue_next(q);

        // remove all messages
        qtd_removed = ngx_http_push_stream_ensure_qtd_of_messages(data, channel, 0, 0);
        if (qtd_removed > 0) {
            ngx_shmtx_lock(&data->channels_queue_mutex);
            NGX_HTTP_PUSH_STREAM_DECREMENT_COUNTER_BY(data->stored_messages, qtd_removed);
            ngx_shmtx_unlock(&data->channels_queue_mutex);
        }

        // channel has no subscribers and can be released
        if (channel->subscribers == 0) {
            channel->expires = ngx_time() + NGX_HTTP_PUSH_STREAM_DEFAULT_SHM_MEMORY_CLEANUP_OBJECTS_TTL;

            // move the channel to trash queue
            ngx_queue_remove(&channel->queue);
            NGX_HTTP_PUSH_STREAM_DECREMENT_COUNTER(data->channels_in_delete);
            ngx_shmtx_lock(&data->channels_trash_mutex);
            ngx_queue_insert_tail(&data->channels_trash, &channel->queue);
            data->channels_in_trash++;
            ngx_shmtx_unlock(&data->channels_trash_mutex);

            ngx_http_push_stream_send_event(mcf, ngx_cycle->log, channel, &NGX_HTTP_PUSH_STREAM_EVENT_TYPE_CHANNEL_DESTROYED, temp_pool);
        }
    }
    ngx_shmtx_unlock(&data->channels_to_delete_mutex);

    if (temp_pool != NULL) {
        ngx_destroy_pool(temp_pool);
    }
}


static ngx_inline void
ngx_http_push_stream_delete_worker_channel(void)
{
    ngx_http_push_stream_delete_channels();
}


static ngx_inline void
ngx_http_push_stream_cleanup_shutting_down_worker(void)
{
    ngx_http_push_stream_global_shm_data_t *global_data = (ngx_http_push_stream_global_shm_data_t *) ngx_http_push_stream_global_shm_zone->data;
    ngx_queue_t                            *q;

    for (q = ngx_queue_head(&global_data->shm_datas_queue); q != ngx_queue_sentinel(&global_data->shm_datas_queue); q = ngx_queue_next(q)) {
        ngx_http_push_stream_shm_data_t *data = ngx_queue_data(q, ngx_http_push_stream_shm_data_t, shm_data_queue);
        ngx_http_push_stream_cleanup_shutting_down_worker_data(data);
    }
    global_data->pid[ngx_process_slot] = -1;
}


static ngx_inline void
ngx_http_push_stream_cleanup_shutting_down_worker_data(ngx_http_push_stream_shm_data_t *data)
{
    ngx_http_push_stream_worker_data_t          *thisworker_data = data->ipc + ngx_process_slot;
    ngx_queue_t                                 *q;

    while (!ngx_queue_empty(&thisworker_data->subscribers_queue)) {
        q = ngx_queue_head(&thisworker_data->subscribers_queue);
        ngx_http_push_stream_subscriber_t *subscriber = ngx_queue_data(q, ngx_http_push_stream_subscriber_t, worker_queue);
        if (subscriber->longpolling) {
            ngx_http_push_stream_send_response_finalize_for_longpolling_by_timeout(subscriber->request);
        } else {
            ngx_http_push_stream_send_response_finalize(subscriber->request);
        }
    }

    if (ngx_http_push_stream_memory_cleanup_event.timer_set) {
        ngx_del_timer(&ngx_http_push_stream_memory_cleanup_event);
    }

    if (ngx_http_push_stream_buffer_cleanup_event.timer_set) {
        ngx_del_timer(&ngx_http_push_stream_buffer_cleanup_event);
    }

    ngx_http_push_stream_clean_worker_data(data);
}

ngx_uint_t
ngx_http_push_stream_apply_text_template(ngx_str_t **dst_value, ngx_str_t **dst_message, ngx_str_t *text, const ngx_str_t *template, const ngx_str_t *token, ngx_slab_pool_t *shpool, ngx_pool_t *temp_pool)
{
    if (text != NULL) {
        if ((*dst_value = ngx_slab_alloc(shpool, sizeof(ngx_str_t) + text->len + 1)) == NULL) {
            return NGX_ERROR;
        }

        (*dst_value)->len = text->len;
        (*dst_value)->data = (u_char *) ((*dst_value) + 1);
        ngx_memcpy((*dst_value)->data, text->data, text->len);
        (*dst_value)->data[(*dst_value)->len] = '\0';

        ngx_str_t *aux = ngx_http_push_stream_str_replace(template, token, text, 0, temp_pool);
        if (aux == NULL) {
            return NGX_ERROR;
        }

        if (((*dst_message) = ngx_slab_alloc(shpool, sizeof(ngx_str_t) + aux->len)) == NULL) {
            return NGX_ERROR;
        }

        (*dst_message)->len = aux->len;
        (*dst_message)->data = (u_char *) ((*dst_message) + 1);
        ngx_memcpy((*dst_message)->data, aux->data, (*dst_message)->len);
    }

    return NGX_OK;
}

ngx_http_push_stream_msg_t *
ngx_http_push_stream_convert_char_to_msg_on_shared(ngx_http_push_stream_main_conf_t *mcf, u_char *data, size_t len, ngx_http_push_stream_channel_t *channel, ngx_int_t id, ngx_str_t *event_id, ngx_str_t *event_type, time_t time, ngx_int_t tag, ngx_pool_t *temp_pool)
{
    ngx_slab_pool_t                           *shpool = mcf->shpool;
    ngx_queue_t                               *q;
    ngx_http_push_stream_msg_t                *msg;
    int                                        i = 0;

    if ((msg = ngx_slab_alloc(shpool, sizeof(ngx_http_push_stream_msg_t))) == NULL) {
        return NULL;
    }

    msg->event_id = NULL;
    msg->event_type = NULL;
    msg->event_id_message = NULL;
    msg->event_type_message = NULL;
    msg->formatted_messages = NULL;
    msg->deleted = 0;
    msg->expires = 0;
    msg->id = id;
    msg->workers_ref_count = 0;
    msg->time = time;
    msg->tag = tag;
    msg->qtd_templates = mcf->qtd_templates;
    ngx_queue_init(&msg->queue);

    if ((msg->raw.data = ngx_slab_alloc(shpool, len + 1)) == NULL) {
        ngx_http_push_stream_free_message_memory(shpool, msg);
        return NULL;
    }

    msg->raw.len = len;
    // copy the message to shared memory
    ngx_memcpy(msg->raw.data, data, len);
    msg->raw.data[msg->raw.len] = '\0';


    if (ngx_http_push_stream_apply_text_template(&msg->event_id, &msg->event_id_message, event_id, &NGX_HTTP_PUSH_STREAM_EVENTSOURCE_ID_TEMPLATE, &NGX_HTTP_PUSH_STREAM_TOKEN_MESSAGE_EVENT_ID, shpool, temp_pool) != NGX_OK) {
        ngx_http_push_stream_free_message_memory(shpool, msg);
        return NULL;
    }

    if (ngx_http_push_stream_apply_text_template(&msg->event_type, &msg->event_type_message, event_type, &NGX_HTTP_PUSH_STREAM_EVENTSOURCE_EVENT_TEMPLATE, &NGX_HTTP_PUSH_STREAM_TOKEN_MESSAGE_EVENT_TYPE, shpool, temp_pool) != NGX_OK) {
        ngx_http_push_stream_free_message_memory(shpool, msg);
        return NULL;
    }

    if ((msg->formatted_messages = ngx_slab_alloc(shpool, sizeof(ngx_str_t) * msg->qtd_templates)) == NULL) {
        ngx_http_push_stream_free_message_memory(shpool, msg);
        return NULL;
    }
    ngx_memzero(msg->formatted_messages, sizeof(ngx_str_t) * msg->qtd_templates);

    for (q = ngx_queue_head(&mcf->msg_templates); q != ngx_queue_sentinel(&mcf->msg_templates); q = ngx_queue_next(q)) {
        ngx_http_push_stream_template_t *cur = ngx_queue_data(q, ngx_http_push_stream_template_t, queue);
        ngx_str_t *aux = NULL;
        if (cur->eventsource) {
            ngx_http_push_stream_line_t     *cur_line;
            ngx_queue_t                     *lines, *q_line;

            if ((lines = ngx_http_push_stream_split_by_crlf(&msg->raw, temp_pool)) == NULL) {
                ngx_http_push_stream_free_message_memory(shpool, msg);
                return NULL;
            }

            for (q_line = ngx_queue_head(lines); q_line != ngx_queue_sentinel(lines); q_line = ngx_queue_next(q_line )) {
                cur_line = ngx_queue_data(q_line , ngx_http_push_stream_line_t, queue);
                if ((cur_line->line = ngx_http_push_stream_format_message(channel, msg, cur_line->line, cur, temp_pool)) == NULL) {
                    break;
                }
            }

            ngx_str_t *tmp = ngx_http_push_stream_join_with_crlf(lines, temp_pool);
            if ((aux = ngx_http_push_stream_create_str(temp_pool, tmp->len + 1)) != NULL) {
                ngx_sprintf(aux->data, "%V\n", tmp);
            }
        } else {
            aux = ngx_http_push_stream_format_message(channel, msg, &msg->raw, cur, temp_pool);
        }

        if (aux == NULL) {
            ngx_http_push_stream_free_message_memory(shpool, msg);
            return NULL;
        }

        ngx_str_t *text = aux;
        if (cur->websocket) {
            text = ngx_http_push_stream_get_formatted_websocket_frame(&NGX_HTTP_PUSH_STREAM_WEBSOCKET_TEXT_LAST_FRAME_BYTE, sizeof(NGX_HTTP_PUSH_STREAM_WEBSOCKET_TEXT_LAST_FRAME_BYTE), aux->data, aux->len, temp_pool);
        }

        ngx_str_t *formmated = (msg->formatted_messages + i);
        if ((text == NULL) || ((formmated->data = ngx_slab_alloc(shpool, text->len)) == NULL)) {
            ngx_http_push_stream_free_message_memory(shpool, msg);
            return NULL;
        }

        formmated->len = text->len;
        ngx_memcpy(formmated->data, text->data, formmated->len);

        i++;
    }

    return msg;
}


ngx_int_t
ngx_http_push_stream_add_msg_to_channel(ngx_http_push_stream_main_conf_t *mcf, ngx_log_t *log, ngx_http_push_stream_channel_t *channel, u_char *text, size_t len, ngx_str_t *event_id, ngx_str_t *event_type, ngx_flag_t store_messages, ngx_pool_t *temp_pool)
{
    ngx_http_push_stream_shm_data_t        *data = mcf->shm_data;
    ngx_http_push_stream_msg_t             *msg;
    ngx_uint_t                              qtd_removed;
    ngx_int_t                               id;
    time_t                                  time;
    ngx_int_t                               tag;

    ngx_shmtx_lock(channel->mutex);

    ngx_shmtx_lock(&data->shpool->mutex);

    id = channel->last_message_id + 1;
    time = ngx_time();
    tag = ((time == data->last_message_time) ? (data->last_message_tag + 1) : 1);

    data->last_message_time = time;
    data->last_message_tag = tag;

    ngx_shmtx_unlock(&data->shpool->mutex);

    // create a buffer copy in shared mem
    msg = ngx_http_push_stream_convert_char_to_msg_on_shared(mcf, text, len, channel, id, event_id, event_type, time, tag, temp_pool);
    if (msg == NULL) {
        ngx_shmtx_unlock(channel->mutex);
        ngx_log_error(NGX_LOG_ERR, log, 0, "push stream module: unable to allocate message in shared memory");
        return NGX_ERROR;
    }

    channel->last_message_id++;

    // tag message with time stamp and a sequence tag
    channel->last_message_time = msg->time;
    channel->last_message_tag = msg->tag;
    // set message expiration time
    msg->expires = msg->time + mcf->message_ttl;
    channel->expires = ngx_time() + mcf->channel_inactivity_time;

    // put messages on the queue
    if (store_messages) {
        ngx_queue_insert_tail(&channel->message_queue, &msg->queue);
        channel->stored_messages++;
    }
    ngx_shmtx_unlock(channel->mutex);

    // now see if the queue is too big
    qtd_removed = ngx_http_push_stream_ensure_qtd_of_messages(data, channel, mcf->max_messages_stored_per_channel, 0);

    if (!channel->for_events) {
        ngx_shmtx_lock(&data->channels_queue_mutex);
        data->published_messages++;

        NGX_HTTP_PUSH_STREAM_DECREMENT_COUNTER_BY(data->stored_messages, qtd_removed);

        if (store_messages) {
            data->stored_messages++;
        }
        ngx_shmtx_unlock(&data->channels_queue_mutex);
    }

    // send an alert to workers
    ngx_http_push_stream_broadcast(channel, msg, log, mcf);

    // turn on timer to cleanup buffer of old messages
    ngx_http_push_stream_buffer_cleanup_timer_set();

    return NGX_OK;
}


ngx_int_t
ngx_http_push_stream_send_event(ngx_http_push_stream_main_conf_t *mcf, ngx_log_t *log, ngx_http_push_stream_channel_t *channel, ngx_str_t *event_type, ngx_pool_t *received_temp_pool)
{
    ngx_http_push_stream_shm_data_t        *data = mcf->shm_data;
    ngx_pool_t                             *temp_pool = received_temp_pool;

    if ((mcf->events_channel_id.len > 0) && !channel->for_events) {
        if ((temp_pool == NULL) && ((temp_pool = ngx_create_pool(4096, log)) == NULL)) {
            return NGX_ERROR;
        }

        size_t len = ngx_strlen(NGX_HTTP_PUSH_STREAM_EVENT_TEMPLATE) + event_type->len + channel->id.len;
        ngx_str_t *event = ngx_http_push_stream_create_str(temp_pool, len);
        if (event != NULL) {
            ngx_sprintf(event->data, NGX_HTTP_PUSH_STREAM_EVENT_TEMPLATE, event_type, &channel->id);
            ngx_http_push_stream_add_msg_to_channel(mcf, log, data->events_channel, event->data, ngx_strlen(event->data), NULL, event_type, 1, temp_pool);
        }

        if ((received_temp_pool == NULL) && (temp_pool != NULL)) {
            ngx_destroy_pool(temp_pool);
        }
    }

    return NGX_OK;
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


static ngx_int_t
ngx_http_push_stream_send_only_header_response_and_finalize(ngx_http_request_t *r, ngx_int_t status_code, const ngx_str_t *explain_error_message)
{
    ngx_int_t rc;
    rc = ngx_http_push_stream_send_only_header_response(r, status_code, explain_error_message);
    ngx_http_finalize_request(r, (rc == NGX_ERROR) ? NGX_DONE : NGX_OK);
    return NGX_DONE;
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
    ngx_http_push_stream_module_ctx_t     *ctx = ngx_http_get_module_ctx(r, ngx_http_push_stream_module);
    ngx_flag_t                             use_jsonp = (ctx != NULL) && (ctx->callback != NULL);
    ngx_int_t rc = NGX_OK;

    if (pslcf->location_type == NGX_HTTP_PUSH_STREAM_SUBSCRIBER_MODE_EVENTSOURCE) {
        if (msg->event_id_message != NULL) {
            rc = ngx_http_push_stream_send_response_text(r, msg->event_id_message->data, msg->event_id_message->len, 0);
        }

        if ((rc == NGX_OK) && (msg->event_type_message != NULL)) {
            rc = ngx_http_push_stream_send_response_text(r, msg->event_type_message->data, msg->event_type_message->len, 0);
        }
    }

    if (rc == NGX_OK) {
        ngx_str_t *str = ngx_http_push_stream_get_formatted_message(r, channel, msg);
        if (str != NULL) {
            if ((rc == NGX_OK) && use_jsonp && send_callback) {
                rc = ngx_http_push_stream_send_response_text(r, ctx->callback->data, ctx->callback->len, 0);
                if (rc == NGX_OK) {
                    rc = ngx_http_push_stream_send_response_text(r, NGX_HTTP_PUSH_STREAM_CALLBACK_INIT_CHUNK.data, NGX_HTTP_PUSH_STREAM_CALLBACK_INIT_CHUNK.len, 0);
                }
            }

            if ((rc == NGX_OK) && use_jsonp && send_separator) {
                rc = ngx_http_push_stream_send_response_text(r, NGX_HTTP_PUSH_STREAM_CALLBACK_MID_CHUNK.data, NGX_HTTP_PUSH_STREAM_CALLBACK_MID_CHUNK.len, 0);
            }

            if (rc == NGX_OK) {
                rc = ngx_http_push_stream_send_response_text(r, str->data, str->len, 0);
                if (rc == NGX_OK) {
                    ctx->message_sent = 1;
                }
            }

            if ((rc == NGX_OK) && use_jsonp && send_callback) {
                rc = ngx_http_push_stream_send_response_text(r, NGX_HTTP_PUSH_STREAM_CALLBACK_END_CHUNK.data, NGX_HTTP_PUSH_STREAM_CALLBACK_END_CHUNK.len, 0);
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
    ngx_http_push_stream_module_ctx_t      *ctx = NULL;
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
    ngx_http_core_loc_conf_t               *clcf;
    ngx_http_push_stream_module_ctx_t      *ctx = NULL;
    ngx_int_t                               rc;
    ngx_event_t                            *wev;
    ngx_connection_t                       *c;

    c = r->connection;
    wev = c->write;

    rc = ngx_http_output_filter(r, in);

    if ((rc == NGX_OK) && (ctx = ngx_http_get_module_ctx(r, ngx_http_push_stream_module)) != NULL) {
        ngx_chain_update_chains(r->pool, &ctx->free, &ctx->busy, &in, (ngx_buf_tag_t) &ngx_http_push_stream_module);
    }

    if (c->buffered & NGX_HTTP_LOWLEVEL_BUFFERED) {

        clcf = ngx_http_get_module_loc_conf(r->main, ngx_http_core_module);

        r->write_event_handler = ngx_http_push_stream_flush_pending_output;

        if (!wev->delayed) {
            ngx_add_timer(wev, clcf->send_timeout);
        }

        if (ngx_handle_write_event(wev, clcf->send_lowat) != NGX_OK) {
            return NGX_ERROR;
        }

        return NGX_OK;

    } else {
        if (wev->timer_set) {
            ngx_del_timer(wev);
        }
    }

    return rc;
}


static void
ngx_http_push_stream_flush_pending_output(ngx_http_request_t *r)
{
    int                        rc;
    ngx_event_t               *wev;
    ngx_connection_t          *c;
    ngx_http_core_loc_conf_t  *clcf;

    c = r->connection;
    wev = c->write;

    ngx_log_debug2(NGX_LOG_DEBUG_HTTP, wev->log, 0, "push stream module http writer handler: \"%V?%V\"", &r->uri, &r->args);

    clcf = ngx_http_get_module_loc_conf(r->main, ngx_http_core_module);

    if (wev->timedout) {
        if (!wev->delayed) {
            ngx_log_error(NGX_LOG_INFO, c->log, NGX_ETIMEDOUT, "push stream module: client timed out");
            c->timedout = 1;

            ngx_http_finalize_request(r, NGX_HTTP_REQUEST_TIME_OUT);
            return;
        }

        wev->timedout = 0;
        wev->delayed = 0;

        if (!wev->ready) {
            ngx_add_timer(wev, clcf->send_timeout);

            if (ngx_handle_write_event(wev, clcf->send_lowat) != NGX_OK) {
                ngx_http_finalize_request(r, 0);
            }

            return;
        }

    }

    if (wev->delayed || r->aio) {
        ngx_log_debug0(NGX_LOG_DEBUG_HTTP, wev->log, 0, "push stream module http writer delayed");

        if (ngx_handle_write_event(wev, clcf->send_lowat) != NGX_OK) {
            ngx_http_finalize_request(r, 0);
        }

        return;
    }

    rc = ngx_http_push_stream_output_filter(r, NULL);

    ngx_log_debug3(NGX_LOG_DEBUG_HTTP, c->log, 0, "push stream module http writer output filter: %d, \"%V?%V\"", rc, &r->uri, &r->args);

    if (rc == NGX_ERROR) {
        ngx_http_finalize_request(r, rc);
        return;
    }

    if (r->buffered || r->postponed || (r == r->main && c->buffered)) {

        if (!wev->delayed) {
            ngx_add_timer(wev, clcf->send_timeout);
        }

        if (ngx_handle_write_event(wev, clcf->send_lowat) != NGX_OK) {
            ngx_http_finalize_request(r, 0);
        }

        return;
    }

    ngx_log_debug2(NGX_LOG_DEBUG_HTTP, wev->log, 0, "push stream module http writer done: \"%V?%V\"", &r->uri, &r->args);

    r->write_event_handler = ngx_http_request_empty_handler;
}


static ngx_int_t
ngx_http_push_stream_send_response(ngx_http_request_t *r, ngx_str_t *text, const ngx_str_t *content_type, ngx_int_t status_code)
{
    ngx_int_t                rc;

    if ((r == NULL) || (text == NULL) || (content_type == NULL)) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    r->headers_out.content_type_len = content_type->len;
    r->headers_out.content_type = *content_type;
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
    b->temporary = 0;
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
    ngx_http_push_stream_module_ctx_t *ctx = ngx_http_get_module_ctx(r, ngx_http_push_stream_module);
    ngx_http_push_stream_loc_conf_t   *pslcf = ngx_http_get_module_loc_conf(r, ngx_http_push_stream_module);
    ngx_flag_t eventsource = (pslcf->location_type == NGX_HTTP_PUSH_STREAM_SUBSCRIBER_MODE_EVENTSOURCE);

    if (ctx->padding != NULL) {
        ngx_int_t diff = ((sending_header) ? ctx->padding->header_min_len : ctx->padding->message_min_len) - len;
        if (diff > 0) {
            ngx_int_t padding_index = diff / 100;
            ngx_str_t *padding = eventsource ? ngx_http_push_stream_module_paddings_chunks_for_eventsource[padding_index] : ngx_http_push_stream_module_paddings_chunks[padding_index];
            ngx_http_push_stream_send_response_text(r, padding->data, padding->len, 0);
        }
    }

    return NGX_OK;
}



static void
ngx_http_push_stream_run_cleanup_pool_handler(ngx_pool_t *p, ngx_pool_cleanup_pt handler)
{
    ngx_pool_cleanup_t       *c;

    if (p == NULL) {
        return;
    }

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
        if (pslcf->location_type == NGX_HTTP_PUSH_STREAM_SUBSCRIBER_MODE_WEBSOCKET) {
            rc = ngx_http_push_stream_send_response_text(r, NGX_HTTP_PUSH_STREAM_WEBSOCKET_CLOSE_LAST_FRAME_BYTE, sizeof(NGX_HTTP_PUSH_STREAM_WEBSOCKET_CLOSE_LAST_FRAME_BYTE), 1);
        } else {
            rc = ngx_http_send_special(r, NGX_HTTP_LAST | NGX_HTTP_FLUSH);
        }
    }

    ngx_http_finalize_request(r, (rc == NGX_ERROR) ? NGX_DONE : NGX_OK);
}

static void
ngx_http_push_stream_send_response_finalize_for_longpolling_by_timeout(ngx_http_request_t *r)
{
    ngx_http_push_stream_main_conf_t   *mcf = ngx_http_get_module_main_conf(r, ngx_http_push_stream_module);

    ngx_http_push_stream_run_cleanup_pool_handler(r->pool, (ngx_pool_cleanup_pt) ngx_http_push_stream_cleanup_request_context);

    ngx_http_push_stream_add_polling_headers(r, ngx_time(), 0, r->pool);

    if (mcf->timeout_with_body && (mcf->longpooling_timeout_msg == NULL)) {
        // create longpooling timeout message
        if ((mcf->longpooling_timeout_msg == NULL) && (mcf->longpooling_timeout_msg = ngx_http_push_stream_convert_char_to_msg_on_shared(mcf, (u_char *) NGX_HTTP_PUSH_STREAM_LONGPOOLING_TIMEOUT_MESSAGE_TEXT, ngx_strlen(NGX_HTTP_PUSH_STREAM_LONGPOOLING_TIMEOUT_MESSAGE_TEXT), NULL, NGX_HTTP_PUSH_STREAM_LONGPOOLING_TIMEOUT_MESSAGE_ID, NULL, NULL, 0, 0, r->pool)) == NULL) {
            ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "push stream module: unable to allocate long pooling timeout message in shared memory");
        }
    }

    if (mcf->timeout_with_body && (mcf->longpooling_timeout_msg != NULL)) {
        ngx_http_send_header(r);

        ngx_http_push_stream_send_response_content_header(r, ngx_http_get_module_loc_conf(r, ngx_http_push_stream_module));
        ngx_http_push_stream_send_response_message(r, NULL, mcf->longpooling_timeout_msg, 1, 0);
        ngx_http_push_stream_send_response_finalize(r);
    } else {
        ngx_http_push_stream_send_only_header_response_and_finalize(r, NGX_HTTP_NOT_MODIFIED, NULL);
    }
}

static ngx_int_t
ngx_http_push_stream_send_websocket_close_frame(ngx_http_request_t *r, ngx_uint_t http_status, const ngx_str_t *reason)
{
    ngx_int_t rc;
    ngx_str_t *text = ngx_http_push_stream_create_str(r->pool, reason->len + NGX_INT_T_LEN + NGX_HTTP_PUSH_STREAM_WEBSOCKET_CLOSE_REASON.len);
    if (text == NULL) {
        rc = ngx_http_push_stream_send_response_text(r, NGX_HTTP_PUSH_STREAM_WEBSOCKET_CLOSE_LAST_FRAME_BYTE, sizeof(NGX_HTTP_PUSH_STREAM_WEBSOCKET_CLOSE_LAST_FRAME_BYTE), 1);
    } else {
        u_char *last = ngx_sprintf(text->data, (char *) NGX_HTTP_PUSH_STREAM_WEBSOCKET_CLOSE_REASON.data, http_status, reason);
        text->len = last - text->data;
        ngx_str_t *frame = ngx_http_push_stream_get_formatted_websocket_frame(NGX_HTTP_PUSH_STREAM_WEBSOCKET_CLOSE_LAST_FRAME_BYTE, 1, text->data, text->len, r->pool);
        rc = ngx_http_push_stream_send_response_text(r, (const u_char *) frame->data, frame->len, 1);
    }
    return (rc == NGX_ERROR) ? NGX_DONE : NGX_OK;
}

static ngx_int_t
ngx_http_push_stream_delete_channel(ngx_http_push_stream_main_conf_t *mcf, ngx_http_push_stream_channel_t *channel, u_char *text, size_t len, ngx_pool_t *temp_pool)
{
    ngx_http_push_stream_shm_data_t        *data = mcf->shm_data;
    ngx_http_push_stream_pid_queue_t       *worker;
    ngx_queue_t                            *q;
    ngx_flag_t                              deleted = 0;

    ngx_shmtx_lock(&data->channels_queue_mutex);
    if ((channel != NULL) && !channel->deleted) {
        // apply channel deleted message text to message template
        if ((channel->channel_deleted_message = ngx_http_push_stream_convert_char_to_msg_on_shared(mcf, text, len, channel, NGX_HTTP_PUSH_STREAM_CHANNEL_DELETED_MESSAGE_ID, NULL, NULL, 0, 0, temp_pool)) == NULL) {
            ngx_shmtx_unlock(&data->channels_queue_mutex);

            ngx_log_error(NGX_LOG_ERR, temp_pool->log, 0, "push stream module: unable to allocate memory to channel deleted message");
            return -1;
        }

        deleted = 1;
        channel->deleted = 1;
        (channel->wildcard) ? NGX_HTTP_PUSH_STREAM_DECREMENT_COUNTER(data->wildcard_channels) : NGX_HTTP_PUSH_STREAM_DECREMENT_COUNTER(data->channels);

        // remove channel from active tree and queue
        ngx_rbtree_delete(&data->tree, &channel->node);
        ngx_queue_remove(&channel->queue);
    }
    ngx_shmtx_unlock(&data->channels_queue_mutex);

    if (deleted) {
        // move the channel to unrecoverable queue
        ngx_shmtx_lock(&data->channels_to_delete_mutex);
        ngx_queue_insert_tail(&data->channels_to_delete, &channel->queue);
        data->channels_in_delete++;
        ngx_shmtx_unlock(&data->channels_to_delete_mutex);

        // send signal to each worker with subscriber to this channel
        if (ngx_queue_empty(&channel->workers_with_subscribers)) {
            ngx_http_push_stream_alert_worker_delete_channel(ngx_pid, ngx_process_slot, ngx_cycle->log);
        } else {
            for (q = ngx_queue_head(&channel->workers_with_subscribers); q != ngx_queue_sentinel(&channel->workers_with_subscribers); q = ngx_queue_next(q)) {
                worker = ngx_queue_data(q, ngx_http_push_stream_pid_queue_t, queue);
                ngx_http_push_stream_alert_worker_delete_channel(worker->pid, worker->slot, ngx_cycle->log);
            }
        }
    }

    return deleted;
}


static void
ngx_http_push_stream_collect_expired_messages_and_empty_channels(ngx_flag_t force)
{
    ngx_http_push_stream_global_shm_data_t *global_data = (ngx_http_push_stream_global_shm_data_t *) ngx_http_push_stream_global_shm_zone->data;
    ngx_queue_t                            *q;

    for (q = ngx_queue_head(&global_data->shm_datas_queue); q != ngx_queue_sentinel(&global_data->shm_datas_queue); q = ngx_queue_next(q)) {
        ngx_http_push_stream_shm_data_t *data = ngx_queue_data(q, ngx_http_push_stream_shm_data_t, shm_data_queue);
        ngx_http_push_stream_collect_expired_messages_and_empty_channels_data(data, force);
    }
}


void
ngx_http_push_stream_collect_expired_messages_and_empty_channels_data(ngx_http_push_stream_shm_data_t *data, ngx_flag_t force)
{
    ngx_http_push_stream_main_conf_t   *mcf = data->mcf;
    ngx_http_push_stream_channel_t     *channel;
    ngx_queue_t                        *q;
    ngx_pool_t                         *temp_pool = NULL;

    if (mcf->events_channel_id.len > 0) {
        if ((temp_pool = ngx_create_pool(4096, ngx_cycle->log)) == NULL) {
            ngx_log_error(NGX_LOG_ERR, ngx_cycle->log, 0, "push stream module: unable to allocate memory to temporary pool");
            return;
        }
    }

    ngx_http_push_stream_collect_expired_messages_data(data, force);

    ngx_shmtx_lock(&data->channels_queue_mutex);
    for (q = ngx_queue_head(&data->channels_queue); q != ngx_queue_sentinel(&data->channels_queue);) {
        channel = ngx_queue_data(q, ngx_http_push_stream_channel_t, queue);
        q = ngx_queue_next(q);

        if ((channel->stored_messages == 0) && (channel->subscribers == 0) && (channel->expires < ngx_time()) && !channel->for_events) {
            channel->deleted = 1;
            channel->expires = ngx_time() + NGX_HTTP_PUSH_STREAM_DEFAULT_SHM_MEMORY_CLEANUP_OBJECTS_TTL;
            (channel->wildcard) ? NGX_HTTP_PUSH_STREAM_DECREMENT_COUNTER(data->wildcard_channels) : NGX_HTTP_PUSH_STREAM_DECREMENT_COUNTER(data->channels);

            // move the channel to trash queue
            ngx_rbtree_delete(&data->tree, &channel->node);
            ngx_queue_remove(&channel->queue);
            ngx_shmtx_lock(&data->channels_trash_mutex);
            ngx_queue_insert_tail(&data->channels_trash, &channel->queue);
            data->channels_in_trash++;
            ngx_shmtx_unlock(&data->channels_trash_mutex);

            ngx_http_push_stream_send_event(mcf, ngx_cycle->log, channel, &NGX_HTTP_PUSH_STREAM_EVENT_TYPE_CHANNEL_DESTROYED, temp_pool);
        }
    }
    ngx_shmtx_unlock(&data->channels_queue_mutex);

    if (temp_pool != NULL) {
        ngx_destroy_pool(temp_pool);
    }
}


static void
ngx_http_push_stream_collect_expired_messages_data(ngx_http_push_stream_shm_data_t *data, ngx_flag_t force)
{
    ngx_http_push_stream_channel_t         *channel;
    ngx_queue_t                            *q;
    ngx_uint_t                              qtd_removed;

    ngx_shmtx_lock(&data->channels_queue_mutex);

    for (q = ngx_queue_head(&data->channels_queue); q != ngx_queue_sentinel(&data->channels_queue); q = ngx_queue_next(q)) {
        channel = ngx_queue_data(q, ngx_http_push_stream_channel_t, queue);

        qtd_removed = ngx_http_push_stream_ensure_qtd_of_messages(data, channel, (force) ? 0 : channel->stored_messages, 1);
        NGX_HTTP_PUSH_STREAM_DECREMENT_COUNTER_BY(data->stored_messages, qtd_removed);
    }

    ngx_shmtx_unlock(&data->channels_queue_mutex);
}


static void
ngx_http_push_stream_free_memory_of_expired_channels(ngx_http_push_stream_shm_data_t *data, ngx_slab_pool_t *shpool, ngx_flag_t force)
{
    ngx_http_push_stream_channel_t         *channel;
    ngx_queue_t                            *cur;

    ngx_shmtx_lock(&data->channels_trash_mutex);
    while (!ngx_queue_empty(&data->channels_trash)) {
        cur = ngx_queue_head(&data->channels_trash);
        channel = ngx_queue_data(cur, ngx_http_push_stream_channel_t, queue);

        if ((ngx_time() > channel->expires) || force) {
            ngx_queue_remove(&channel->queue);
            nxg_http_push_stream_free_channel_memory(shpool, channel);
            NGX_HTTP_PUSH_STREAM_DECREMENT_COUNTER(data->channels_in_trash);
        } else {
            break;
        }
    }
    ngx_shmtx_unlock(&data->channels_trash_mutex);
}


static void
nxg_http_push_stream_free_channel_memory(ngx_slab_pool_t *shpool, ngx_http_push_stream_channel_t *channel)
{
    // delete the worker-subscriber queue
    ngx_http_push_stream_pid_queue_t     *worker;
    ngx_queue_t                          *cur;
    ngx_shmtx_t                          *mutex = channel->mutex;

    if (channel->channel_deleted_message != NULL) ngx_http_push_stream_free_message_memory(shpool, channel->channel_deleted_message);
    ngx_shmtx_lock(mutex);
    while (!ngx_queue_empty(&channel->workers_with_subscribers)) {
        cur = ngx_queue_head(&channel->workers_with_subscribers);
        worker = ngx_queue_data(cur, ngx_http_push_stream_pid_queue_t, queue);
        ngx_queue_remove(&worker->queue);
        ngx_slab_free(shpool, worker);
    }

    ngx_slab_free(shpool, channel->id.data);
    ngx_slab_free(shpool, channel);
    ngx_shmtx_unlock(mutex);
}


static ngx_int_t
ngx_http_push_stream_memory_cleanup(void)
{
    ngx_http_push_stream_global_shm_data_t *global_data = (ngx_http_push_stream_global_shm_data_t *) ngx_http_push_stream_global_shm_zone->data;
    ngx_queue_t                            *q;

    for (q = ngx_queue_head(&global_data->shm_datas_queue); q != ngx_queue_sentinel(&global_data->shm_datas_queue); q = ngx_queue_next(q)) {
        ngx_http_push_stream_shm_data_t *data = ngx_queue_data(q, ngx_http_push_stream_shm_data_t, shm_data_queue);
        ngx_http_push_stream_delete_channels_data(data);
        if (ngx_shmtx_trylock(&data->cleanup_mutex)) {
            ngx_http_push_stream_collect_deleted_channels_data(data);
            ngx_http_push_stream_collect_expired_messages_and_empty_channels_data(data, 0);
            ngx_http_push_stream_free_memory_of_expired_messages_and_channels_data(data, 0);
            ngx_shmtx_unlock(&data->cleanup_mutex);
        }
    }

    return NGX_OK;
}


static ngx_int_t
ngx_http_push_stream_buffer_cleanup(void)
{
    ngx_http_push_stream_global_shm_data_t *global_data = (ngx_http_push_stream_global_shm_data_t *) ngx_http_push_stream_global_shm_zone->data;
    ngx_queue_t                            *q;


    for (q = ngx_queue_head(&global_data->shm_datas_queue); q != ngx_queue_sentinel(&global_data->shm_datas_queue); q = ngx_queue_next(q)) {
        ngx_http_push_stream_shm_data_t *data = ngx_queue_data(q, ngx_http_push_stream_shm_data_t, shm_data_queue);
        if (ngx_shmtx_trylock(&data->cleanup_mutex)) {
            ngx_http_push_stream_collect_expired_messages_data(data, 0);
            ngx_shmtx_unlock(&data->cleanup_mutex);
        }
    }

    return NGX_OK;
}


static ngx_int_t
ngx_http_push_stream_free_memory_of_expired_messages_and_channels(ngx_flag_t force)
{
    ngx_http_push_stream_global_shm_data_t *global_data = (ngx_http_push_stream_global_shm_data_t *) ngx_http_push_stream_global_shm_zone->data;
    ngx_queue_t                            *q;

    for (q = ngx_queue_head(&global_data->shm_datas_queue); q != ngx_queue_sentinel(&global_data->shm_datas_queue); q = ngx_queue_next(q)) {
        ngx_http_push_stream_shm_data_t *data = ngx_queue_data(q, ngx_http_push_stream_shm_data_t, shm_data_queue);
        ngx_http_push_stream_free_memory_of_expired_messages_and_channels_data(data, 0);
    }

    return NGX_OK;
}


void
ngx_http_push_stream_free_memory_of_expired_messages_and_channels_data(ngx_http_push_stream_shm_data_t *data, ngx_flag_t force)
{
    ngx_slab_pool_t                        *shpool = data->shpool;
    ngx_http_push_stream_msg_t             *message;
    ngx_queue_t                            *cur;

    ngx_shmtx_lock(&data->messages_trash_mutex);
    while (!ngx_queue_empty(&data->messages_trash)) {
        cur = ngx_queue_head(&data->messages_trash);
        message = ngx_queue_data(cur, ngx_http_push_stream_msg_t, queue);

        if (force || ((message->workers_ref_count <= 0) && (ngx_time() > message->expires))) {
            ngx_queue_remove(&message->queue);
            ngx_http_push_stream_free_message_memory(shpool, message);
            NGX_HTTP_PUSH_STREAM_DECREMENT_COUNTER(data->messages_in_trash);
        } else {
            break;
        }
    }
    ngx_shmtx_unlock(&data->messages_trash_mutex);
    ngx_http_push_stream_free_memory_of_expired_channels(data, shpool, force);
}


static void
ngx_http_push_stream_free_message_memory(ngx_slab_pool_t *shpool, ngx_http_push_stream_msg_t *msg)
{
    u_int i;

    if (msg == NULL) {
        return;
    }

    ngx_shmtx_lock(&shpool->mutex);
    if (msg->formatted_messages != NULL) {
        for (i = 0; i < msg->qtd_templates; i++) {
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
    ngx_shmtx_unlock(&shpool->mutex);
}


static void
ngx_http_push_stream_free_worker_message_memory(ngx_slab_pool_t *shpool, ngx_http_push_stream_worker_msg_t *worker_msg)
{
    ngx_shmtx_lock(&shpool->mutex);
    worker_msg->msg->workers_ref_count--;
    if ((worker_msg->msg->workers_ref_count <= 0) && worker_msg->msg->deleted) {
        worker_msg->msg->expires = ngx_time() + NGX_HTTP_PUSH_STREAM_DEFAULT_SHM_MEMORY_CLEANUP_OBJECTS_TTL;
    }
    ngx_queue_remove(&worker_msg->queue);
    ngx_slab_free_locked(shpool, worker_msg);
    ngx_shmtx_unlock(&shpool->mutex);
}


static void
ngx_http_push_stream_throw_the_message_away(ngx_http_push_stream_msg_t *msg, ngx_http_push_stream_shm_data_t *data)
{
    ngx_shmtx_lock(&data->messages_trash_mutex);
    msg->deleted = 1;
    msg->expires = ngx_time() + NGX_HTTP_PUSH_STREAM_DEFAULT_SHM_MEMORY_CLEANUP_OBJECTS_TTL;
    ngx_queue_insert_tail(&data->messages_trash, &msg->queue);
    data->messages_in_trash++;
    ngx_shmtx_unlock(&data->messages_trash_mutex);
}


static void
ngx_http_push_stream_timer_set(ngx_msec_t timer_interval, ngx_event_t *event, ngx_event_handler_pt event_handler, ngx_flag_t start_timer)
{
    if ((timer_interval != NGX_CONF_UNSET_MSEC) && start_timer) {
        if (event->handler == NULL) {
            event->handler = event_handler;
            event->data = event; //set event as data to avoid error when running on debug mode (on log event)
            event->log = ngx_cycle->log;
            ngx_http_push_stream_timer_reset(timer_interval, event);
        }
    }
}


static void
ngx_http_push_stream_timer_reset(ngx_msec_t timer_interval, ngx_event_t *timer_event)
{
    if (!ngx_exiting && (timer_interval != NGX_CONF_UNSET_MSEC) && (timer_event != NULL)) {
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
    ngx_http_push_stream_main_conf_t   *mcf = ngx_http_get_module_main_conf(r, ngx_http_push_stream_module);
    ngx_http_push_stream_loc_conf_t    *pslcf = ngx_http_get_module_loc_conf(r, ngx_http_push_stream_module);
    ngx_http_push_stream_module_ctx_t  *ctx = ngx_http_get_module_ctx(r, ngx_http_push_stream_module);
    ngx_int_t                           rc = NGX_OK;

    if ((ctx == NULL) || (ctx->ping_timer == NULL)) {
        return;
    }

    if (pslcf->location_type == NGX_HTTP_PUSH_STREAM_SUBSCRIBER_MODE_EVENTSOURCE) {
        rc = ngx_http_push_stream_send_response_text(r, NGX_HTTP_PUSH_STREAM_EVENTSOURCE_PING_MESSAGE_CHUNK.data, NGX_HTTP_PUSH_STREAM_EVENTSOURCE_PING_MESSAGE_CHUNK.len, 0);
    } else if (pslcf->location_type == NGX_HTTP_PUSH_STREAM_SUBSCRIBER_MODE_WEBSOCKET) {
        rc = ngx_http_push_stream_send_response_text(r, NGX_HTTP_PUSH_STREAM_WEBSOCKET_PING_LAST_FRAME_BYTE, sizeof(NGX_HTTP_PUSH_STREAM_WEBSOCKET_PING_LAST_FRAME_BYTE), 0);
    } else {
        if (mcf->ping_msg == NULL) {
            // create ping message
            if ((mcf->ping_msg == NULL) && (mcf->ping_msg = ngx_http_push_stream_convert_char_to_msg_on_shared(mcf, mcf->ping_message_text.data, mcf->ping_message_text.len, NULL, NGX_HTTP_PUSH_STREAM_PING_MESSAGE_ID, NULL, NULL, 0, 0, r->pool)) == NULL) {
                ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "push stream module: unable to allocate ping message in shared memory");
            }
        }

        if (mcf->ping_msg != NULL) {
            rc = ngx_http_push_stream_send_response_message(r, NULL, mcf->ping_msg, 1, 0);
        }
    }

    if (rc != NGX_OK) {
        ngx_http_push_stream_send_response_finalize(r);
    } else {
        ngx_http_push_stream_timer_reset(pslcf->ping_message_interval, ctx->ping_timer);
    }
}

static void
ngx_http_push_stream_disconnect_timer_wake_handler(ngx_event_t *ev)
{
    ngx_http_request_t                    *r = (ngx_http_request_t *) ev->data;
    ngx_http_push_stream_module_ctx_t     *ctx = ngx_http_get_module_ctx(r, ngx_http_push_stream_module);

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
    ngx_http_push_stream_timer_reset(NGX_HTTP_PUSH_STREAM_DEFAULT_SHM_MEMORY_CLEANUP_INTERVAL, &ngx_http_push_stream_memory_cleanup_event);
}

static void
ngx_http_push_stream_buffer_timer_wake_handler(ngx_event_t *ev)
{
    ngx_http_push_stream_buffer_cleanup();
    ngx_http_push_stream_timer_reset(NGX_HTTP_PUSH_STREAM_MESSAGE_BUFFER_CLEANUP_INTERVAL, &ngx_http_push_stream_buffer_cleanup_event);
}

static ngx_str_t *
ngx_http_push_stream_str_replace(const ngx_str_t *org, const ngx_str_t *find, const ngx_str_t *replace, off_t offset, ngx_pool_t *pool)
{
    if (org == NULL) {
        return NULL;
    }

    ngx_str_t *result = (ngx_str_t *) org;

    if (find->len > 0) {
        u_char *ret = (u_char *) ngx_strnstr(org->data + offset, (char *) find->data, org->len - offset);
        if (ret != NULL) {
            ngx_str_t *tmp = ngx_http_push_stream_create_str(pool, org->len + replace->len - find->len);
            if (tmp == NULL) {
                ngx_log_error(NGX_LOG_ERR, pool->log, 0, "push stream module: unable to allocate memory to apply text replace");
                return NULL;
            }

            off_t offset_found = ret - org->data;
            ngx_memcpy(tmp->data, org->data, offset_found);
            ngx_memcpy(tmp->data + offset_found, replace->data, replace->len);
            ngx_memcpy(tmp->data + offset_found + replace->len, org->data + offset_found + find->len, org->len - offset_found - find->len);

            result = ngx_http_push_stream_str_replace(tmp, find, replace, offset_found + replace->len, pool);
        }
    }

    return result;
}


static ngx_str_t *
ngx_http_push_stream_get_formatted_message(ngx_http_request_t *r, ngx_http_push_stream_channel_t *channel, ngx_http_push_stream_msg_t *message)
{
    ngx_http_push_stream_loc_conf_t        *pslcf = ngx_http_get_module_loc_conf(r, ngx_http_push_stream_module);
    if (pslcf->message_template_index > 0) {
        return message->formatted_messages + pslcf->message_template_index - 1;
    }
    return &message->raw;
}


static ngx_str_t *
ngx_http_push_stream_format_message(ngx_http_push_stream_channel_t *channel, ngx_http_push_stream_msg_t *message, ngx_str_t *text, ngx_http_push_stream_template_t *template, ngx_pool_t *temp_pool)
{
    u_char                    *last;
    ngx_str_t                 *txt = NULL;
    size_t                     len = 0;
    ngx_queue_t               *q;
    u_char                     id[NGX_INT_T_LEN + 1];
    u_char                     tag[NGX_INT_T_LEN + 1];
    u_char                     size[NGX_INT_T_LEN + 1];
    u_char                     time[NGX_HTTP_PUSH_STREAM_TIME_FMT_LEN + 1];
    size_t                     id_len, tag_len, time_len, size_len;

    ngx_str_t *channel_id = (channel != NULL) ? &channel->id : &NGX_HTTP_PUSH_STREAM_EMPTY;
    ngx_str_t *event_id = (message->event_id != NULL) ? message->event_id : &NGX_HTTP_PUSH_STREAM_EMPTY;
    ngx_str_t *event_type = (message->event_type != NULL) ? message->event_type : &NGX_HTTP_PUSH_STREAM_EMPTY;

    ngx_sprintf(id, "%d%Z", message->id);
    id_len = ngx_strlen(id);

    last = ngx_http_time(time, message->time);
    time_len = last - time;

    ngx_sprintf(tag, "%d%Z", message->tag);
    tag_len = ngx_strlen(tag);

    ngx_sprintf(size, "%d%Z", text->len);
    size_len = ngx_strlen(size);

    len += template->qtd_channel * channel_id->len;
    len += template->qtd_event_id * event_id->len;
    len += template->qtd_event_type * event_type->len;
    len += template->qtd_message_id * id_len;
    len += template->qtd_time * time_len;
    len += template->qtd_tag * tag_len;
    len += template->qtd_text * text->len;
    len += template->qtd_size * size_len;
    len += template->literal_len;

    txt = ngx_http_push_stream_create_str(temp_pool, len);
    if (txt == NULL) {
        ngx_log_error(NGX_LOG_ERR, temp_pool->log, 0, "push stream module: unable to allocate memory to format message");
        return NULL;
    }

    last = txt->data;
    for (q = ngx_queue_head(&template->parts); q != ngx_queue_sentinel(&template->parts); q = ngx_queue_next(q)) {
        ngx_http_push_stream_template_parts_t *cur = ngx_queue_data(q, ngx_http_push_stream_template_parts_t, queue);
        switch (cur->kind) {
            case PUSH_STREAM_TEMPLATE_PART_TYPE_CHANNEL:
                last = ngx_cpymem(last, channel_id->data, channel_id->len);
                break;
            case PUSH_STREAM_TEMPLATE_PART_TYPE_EVENT_ID:
                last = ngx_cpymem(last, event_id->data, event_id->len);
                break;
            case PUSH_STREAM_TEMPLATE_PART_TYPE_EVENT_TYPE:
                last = ngx_cpymem(last, event_type->data, event_type->len);
                break;
            case PUSH_STREAM_TEMPLATE_PART_TYPE_ID:
                last = ngx_cpymem(last, id, id_len);
                break;
            case PUSH_STREAM_TEMPLATE_PART_TYPE_LITERAL:
                last = ngx_cpymem(last, cur->text.data, cur->text.len);
                break;
            case PUSH_STREAM_TEMPLATE_PART_TYPE_TAG:
                last = ngx_cpymem(last, tag, tag_len);
                break;
            case PUSH_STREAM_TEMPLATE_PART_TYPE_TEXT:
                last = ngx_cpymem(last, text->data, text->len);
                break;
            case PUSH_STREAM_TEMPLATE_PART_TYPE_SIZE:
                last = ngx_cpymem(last, size, size_len);
                break;
            case PUSH_STREAM_TEMPLATE_PART_TYPE_TIME:
                last = ngx_cpymem(last, time, time_len);
                break;
            default:
                break;
        }
    }

    return txt;
}


static ngx_http_push_stream_module_ctx_t *
ngx_http_push_stream_add_request_context(ngx_http_request_t *r)
{
    ngx_pool_cleanup_t                      *cln;
    ngx_http_push_stream_module_ctx_t       *ctx = ngx_http_get_module_ctx(r, ngx_http_push_stream_module);

    if (ctx != NULL) {
        return ctx;
    }

    if ((ctx = ngx_pcalloc(r->pool, sizeof(ngx_http_push_stream_module_ctx_t))) == NULL) {
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
    ctx->message_sent = 0;
    ctx->padding = NULL;
    ctx->callback = NULL;
    ctx->requested_channels = NULL;

    // set a cleaner to request
    cln->handler = (ngx_pool_cleanup_pt) ngx_http_push_stream_cleanup_request_context;
    cln->data = r;

    ngx_http_set_ctx(r, ctx, ngx_http_push_stream_module);

    return ctx;
}


static void
ngx_http_push_stream_cleanup_request_context(ngx_http_request_t *r)
{
    ngx_http_push_stream_module_ctx_t       *ctx = ngx_http_get_module_ctx(r, ngx_http_push_stream_module);

    r->read_event_handler = ngx_http_request_empty_handler;

    if (ctx != NULL) {
        if ((ctx->disconnect_timer != NULL) && ctx->disconnect_timer->timer_set) {
            ngx_del_timer(ctx->disconnect_timer);
        }

        if ((ctx->ping_timer != NULL) && ctx->ping_timer->timer_set) {
            ngx_del_timer(ctx->ping_timer);
        }

        if (ctx->subscriber != NULL) {
            ngx_http_push_stream_worker_subscriber_cleanup(ctx->subscriber);
        }

        if (ctx->temp_pool != NULL) {
            ngx_destroy_pool(ctx->temp_pool);
        }

        ctx->temp_pool = NULL;
        ctx->disconnect_timer = NULL;
        ctx->ping_timer = NULL;
        ctx->subscriber = NULL;
    }
}


static void
ngx_http_push_stream_worker_subscriber_cleanup(ngx_http_push_stream_subscriber_t *worker_subscriber)
{
    ngx_http_push_stream_main_conf_t        *mcf = ngx_http_get_module_main_conf(worker_subscriber->request, ngx_http_push_stream_module);
    ngx_http_push_stream_shm_data_t         *data = mcf->shm_data;
    ngx_slab_pool_t                         *shpool = mcf->shpool;
    ngx_queue_t                             *cur;

    while (!ngx_queue_empty(&worker_subscriber->subscriptions)) {
        cur = ngx_queue_head(&worker_subscriber->subscriptions);
        ngx_http_push_stream_subscription_t *subscription = ngx_queue_data(cur, ngx_http_push_stream_subscription_t, queue);
        ngx_shmtx_lock(subscription->channel->mutex);
        NGX_HTTP_PUSH_STREAM_DECREMENT_COUNTER(subscription->channel->subscribers);
        NGX_HTTP_PUSH_STREAM_DECREMENT_COUNTER(subscription->channel_worker_sentinel->subscribers);
        ngx_queue_remove(&subscription->channel_worker_queue);
        ngx_queue_remove(&subscription->queue);
        ngx_shmtx_unlock(subscription->channel->mutex);

        ngx_http_push_stream_send_event(mcf, ngx_cycle->log, subscription->channel, &NGX_HTTP_PUSH_STREAM_EVENT_TYPE_CLIENT_UNSUBSCRIBED, worker_subscriber->request->pool);
    }

    ngx_shmtx_lock(&shpool->mutex);
    ngx_queue_remove(&worker_subscriber->worker_queue);
    NGX_HTTP_PUSH_STREAM_DECREMENT_COUNTER(data->subscribers);
    NGX_HTTP_PUSH_STREAM_DECREMENT_COUNTER(data->ipc[ngx_process_slot].subscribers);
    ngx_shmtx_unlock(&shpool->mutex);
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
ngx_http_push_stream_get_formatted_websocket_frame(const u_char *opcode, off_t opcode_len, const u_char *text, off_t len, ngx_pool_t *temp_pool)
{
    ngx_str_t            *frame;
    u_char               *last;

    frame = ngx_http_push_stream_create_str(temp_pool, NGX_HTTP_PUSH_STREAM_WEBSOCKET_FRAME_HEADER_MAX_LENGTH + len);
    if (frame != NULL) {
        last = ngx_copy(frame->data, opcode, opcode_len);

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
ngx_http_push_stream_add_line_to_queue(ngx_queue_t *lines, u_char *text, u_int len, ngx_pool_t *temp_pool)
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
        ngx_queue_insert_tail(lines, &cur->queue);
    }
    return cur;
}

static ngx_queue_t *
ngx_http_push_stream_split_by_crlf(ngx_str_t *msg, ngx_pool_t *temp_pool)
{
    ngx_queue_t                        *lines = NULL;
    u_char                             *pos = NULL, *start = NULL, *crlf_pos, *cr_pos, *lf_pos;
    u_int                               step = 0, len = 0;

    if ((lines = ngx_pcalloc(temp_pool, sizeof(ngx_queue_t))) == NULL) {
        return NULL;
    }

    ngx_queue_init(lines);

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
            if ((len > 0) && (ngx_http_push_stream_add_line_to_queue(lines, start, len, temp_pool) == NULL)) {
                return NULL;
            }
            start = pos + step;
        }

    } while (pos != NULL);

    len = (msg->data + msg->len) - start;
    if ((len > 0) && (ngx_http_push_stream_add_line_to_queue(lines, start, len, temp_pool) == NULL)) {
        return NULL;
    }

    return lines;
}


static ngx_str_t *
ngx_http_push_stream_join_with_crlf(ngx_queue_t *lines, ngx_pool_t *temp_pool)
{
    ngx_http_push_stream_line_t     *cur;
    ngx_str_t                       *result = NULL, *tmp = &NGX_HTTP_PUSH_STREAM_EMPTY;
    ngx_queue_t                     *q;

    if (ngx_queue_empty(lines)) {
        return &NGX_HTTP_PUSH_STREAM_EMPTY;
    }

    for (q = ngx_queue_head(lines); q != ngx_queue_sentinel(lines); q = ngx_queue_next(q)) {
        cur = ngx_queue_data(q, ngx_http_push_stream_line_t, queue);

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
    ngx_http_push_stream_line_t     *cur;
    ngx_str_t                       *result = NULL;
    ngx_queue_t                     *lines, *q;

    lines = ngx_http_push_stream_split_by_crlf(text, temp_pool);
    if (lines != NULL) {
        for (q = ngx_queue_head(lines); q != ngx_queue_sentinel(lines); q = ngx_queue_next(q)) {
            cur = ngx_queue_data(q, ngx_http_push_stream_line_t, queue);
            cur->line = ngx_http_push_stream_str_replace(message_template, &NGX_HTTP_PUSH_STREAM_TOKEN_MESSAGE_TEXT, cur->line, 0, temp_pool);
            if (cur->line == NULL) {
                return NULL;
            }
        }
        result = ngx_http_push_stream_join_with_crlf(lines, temp_pool);
    }

    return result;
}

static void
ngx_http_push_stream_add_polling_headers(ngx_http_request_t *r, time_t last_modified_time, ngx_int_t tag, ngx_pool_t *temp_pool)
{
    ngx_http_push_stream_module_ctx_t          *ctx = ngx_http_get_module_ctx(r, ngx_http_push_stream_module);

    if (ctx->callback != NULL) {
        r->headers_out.content_type_len = NGX_HTTP_PUSH_STREAM_CALLBACK_CONTENT_TYPE.len;
        r->headers_out.content_type = NGX_HTTP_PUSH_STREAM_CALLBACK_CONTENT_TYPE;
    } else {
        ngx_http_set_content_type(r);
    }

    if (last_modified_time > 0) {
        r->headers_out.last_modified_time = last_modified_time;
    }

    if (tag >= 0) {
        ngx_str_t *etag = ngx_http_push_stream_create_str(temp_pool, NGX_INT_T_LEN + 3);
        if (etag != NULL) {
            ngx_sprintf(etag->data, "W/%ui%Z", tag);
            etag->len = ngx_strlen(etag->data);
            r->headers_out.etag = ngx_http_push_stream_add_response_header(r, &NGX_HTTP_PUSH_STREAM_HEADER_ETAG, etag);
        }
    }
}


static void
ngx_http_push_stream_get_last_received_message_values(ngx_http_request_t *r, time_t *if_modified_since, ngx_int_t *tag, ngx_str_t **last_event_id)
{
    ngx_http_push_stream_module_ctx_t              *ctx = ngx_http_get_module_ctx(r, ngx_http_push_stream_module);
    ngx_http_push_stream_loc_conf_t                *cf = ngx_http_get_module_loc_conf(r, ngx_http_push_stream_module);
    ngx_str_t                                      *etag = NULL, vv_etag = ngx_null_string;
    ngx_str_t                                       vv_event_id = ngx_null_string, vv_time = ngx_null_string;

    if (cf->last_received_message_time != NULL) {
        ngx_http_push_stream_complex_value(r, cf->last_received_message_time, &vv_time);
    } else if (r->headers_in.if_modified_since != NULL) {
        vv_time = r->headers_in.if_modified_since->value;
    }

    if (cf->last_received_message_tag != NULL) {
        ngx_http_push_stream_complex_value(r, cf->last_received_message_tag, &vv_etag);
        etag = vv_etag.len ? &vv_etag : NULL;
    } else {
        etag = ngx_http_push_stream_get_header(r, &NGX_HTTP_PUSH_STREAM_HEADER_IF_NONE_MATCH);
    }

    if ((etag != NULL) && (etag->len > 2) && (etag->data[0] == 'W') && (etag->data[1] == '/')) {
        etag->len -= 2;
        etag->data = etag->data + 2;
    }

    if (cf->last_event_id != NULL) {
        ngx_http_push_stream_complex_value(r, cf->last_event_id, &vv_event_id);
        if (vv_event_id.len) {
            *last_event_id = ngx_http_push_stream_create_str(ctx->temp_pool, vv_event_id.len);
            ngx_memcpy(((ngx_str_t *)*last_event_id)->data, vv_event_id.data, vv_event_id.len);
        }
    } else {
        *last_event_id = ngx_http_push_stream_get_header(r, &NGX_HTTP_PUSH_STREAM_HEADER_LAST_EVENT_ID);
    }

    *if_modified_since = vv_time.len ? ngx_http_parse_time(vv_time.data, vv_time.len) : -1;
    *tag = ((etag != NULL) && ((*tag = ngx_atoi(etag->data, etag->len)) != NGX_ERROR)) ? ngx_abs(*tag) : -1;
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
    b->memory = 1;
    b->temporary = 0;

    return ngx_http_write_filter(r, &out);
}


static ngx_queue_t *
ngx_http_push_stream_parse_paddings(ngx_conf_t *cf,  ngx_str_t *paddings_by_user_agent)
{
    ngx_int_t                           rc;
    u_char                              errstr[NGX_MAX_CONF_ERRSTR];
    ngx_regex_compile_t                 padding_rc, *agent_rc;
    int                                 captures[12];
    ngx_queue_t                        *paddings;
    ngx_http_push_stream_padding_t     *padding;
    ngx_str_t                           aux, *agent;


    if ((paddings = ngx_pcalloc(cf->pool, sizeof(ngx_queue_t))) == NULL) {
        ngx_conf_log_error(NGX_LOG_ERR, cf, 0, "push stream module: unable to allocate memory to save padding info");
        return NULL;
    }
    ngx_queue_init(paddings);

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

        if ((padding = ngx_pcalloc(cf->pool, sizeof(ngx_http_push_stream_padding_t))) == NULL) {
            ngx_conf_log_error(NGX_LOG_ERR, cf, 0, "push stream module: unable to allocate memory to save padding info");
            return NULL;
        }

        padding->agent = agent_rc->regex;
        padding->header_min_len = ngx_atoi(aux.data + captures[4], captures[5] - captures[4]);
        padding->message_min_len = ngx_atoi(aux.data + captures[6], captures[7] - captures[6]);

        ngx_queue_insert_tail(paddings, &padding->queue);

        ngx_conf_log_error(NGX_LOG_INFO, cf, 0, "push stream module: padding detected %V, header_min_len %d, message_min_len %d", &agent_rc->pattern, padding->header_min_len, padding->message_min_len);

        aux.data = aux.data + (captures[1] - captures[0] + 1);
        aux.len  = aux.len - (captures[1] - captures[0] + 1);

    } while (aux.data < (paddings_by_user_agent->data + paddings_by_user_agent->len));

    return paddings;
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


/**
 * borrowed from Nginx core files
 */
static ngx_int_t
ngx_http_push_stream_set_expires(ngx_http_request_t *r, ngx_http_push_stream_expires_t expires, time_t expires_time)
{
    size_t            len;
    time_t            now, expires_header_time, max_age;
#if (nginx_version >= 1023000)
    ngx_table_elt_t  *expires_header, *cc;
#else
    ngx_uint_t        i;
    ngx_table_elt_t  *expires_header, *cc, **ccp;
#endif

    expires_header = r->headers_out.expires;

    if (expires_header == NULL) {

        expires_header = ngx_list_push(&r->headers_out.headers);
        if (expires_header == NULL) {
            return NGX_ERROR;
        }

        r->headers_out.expires = expires_header;

        expires_header->hash = 1;
        ngx_str_set(&expires_header->key, "Expires");
    }

    len = sizeof("Mon, 28 Sep 1970 06:00:00 GMT");
    expires_header->value.len = len - 1;

#if (nginx_version >= 1023000)
    cc = r->headers_out.cache_control;

    if (cc == NULL) {

        cc = ngx_list_push(&r->headers_out.headers);
        if (cc == NULL) {
            expires_header->hash = 0;
            return NGX_ERROR;
        }

        r->headers_out.cache_control = cc;
        cc->next = NULL;

        cc->hash = 1;
        ngx_str_set(&cc->key, "Cache-Control");

    } else {
        for (cc = cc->next; cc; cc = cc->next) {
            cc->hash = 0;
        }

        cc = r->headers_out.cache_control;
        cc->next = NULL;
    }
#else
    ccp = r->headers_out.cache_control.elts;

    if (ccp == NULL) {

        if (ngx_array_init(&r->headers_out.cache_control, r->pool, 1, sizeof(ngx_table_elt_t *)) != NGX_OK) {
            return NGX_ERROR;
        }

        ccp = ngx_array_push(&r->headers_out.cache_control);
        if (ccp == NULL) {
            return NGX_ERROR;
        }

        cc = ngx_list_push(&r->headers_out.headers);
        if (cc == NULL) {
            return NGX_ERROR;
        }

        cc->hash = 1;
        ngx_str_set(&cc->key, "Cache-Control");
        *ccp = cc;

    } else {
        for (i = 1; i < r->headers_out.cache_control.nelts; i++) {
            ccp[i]->hash = 0;
        }

        cc = ccp[0];
    }
#endif


    if (expires == NGX_HTTP_PUSH_STREAM_EXPIRES_EPOCH) {
        expires_header->value.data = (u_char *) "Thu, 01 Jan 1970 00:00:01 GMT";
        ngx_str_set(&cc->value, "no-cache, no-store, must-revalidate");
        return NGX_OK;
    }

    if (expires == NGX_HTTP_PUSH_STREAM_EXPIRES_MAX) {
        expires_header->value.data = (u_char *) "Thu, 31 Dec 2037 23:55:55 GMT";
        /* 10 years */
        ngx_str_set(&cc->value, "max-age=315360000");
        return NGX_OK;
    }

    expires_header->value.data = ngx_pnalloc(r->pool, len);
    if (expires_header->value.data == NULL) {
        return NGX_ERROR;
    }

    if (expires_time == 0 && expires != NGX_HTTP_PUSH_STREAM_EXPIRES_DAILY) {
        ngx_memcpy(expires_header->value.data, ngx_cached_http_time.data, ngx_cached_http_time.len + 1);
        ngx_str_set(&cc->value, "max-age=0");
        return NGX_OK;
    }

    now = ngx_time();

    if (expires == NGX_HTTP_PUSH_STREAM_EXPIRES_DAILY) {
        expires_header_time = ngx_next_time(expires_time);
        max_age = expires_header_time - now;

    } else if (expires == NGX_HTTP_PUSH_STREAM_EXPIRES_ACCESS || r->headers_out.last_modified_time == -1) {
        expires_header_time = now + expires_time;
        max_age = expires_time;

    } else {
        expires_header_time = r->headers_out.last_modified_time + expires_time;
        max_age = expires_header_time - now;
    }

    ngx_http_time(expires_header->value.data, expires_header_time);

    if (expires_time < 0 || max_age < 0) {
        ngx_str_set(&cc->value, "no-cache, no-store, must-revalidate");
        return NGX_OK;
    }

    cc->value.data = ngx_pnalloc(r->pool, sizeof("max-age=") + NGX_TIME_T_LEN + 1);
    if (cc->value.data == NULL) {
        return NGX_ERROR;
    }

    cc->value.len = ngx_sprintf(cc->value.data, "max-age=%T", max_age) - cc->value.data;

    return NGX_OK;
}


ngx_http_push_stream_requested_channel_t *
ngx_http_push_stream_parse_channels_ids_from_path(ngx_http_request_t *r, ngx_pool_t *pool) {
    ngx_http_push_stream_main_conf_t               *mcf = ngx_http_get_module_main_conf(r, ngx_http_push_stream_module);
    ngx_http_push_stream_loc_conf_t                *cf = ngx_http_get_module_loc_conf(r, ngx_http_push_stream_module);
    ngx_str_t                                       vv_channels_path = ngx_null_string;
    ngx_http_push_stream_requested_channel_t       *requested_channels, *requested_channel;
    ngx_str_t                                       aux;
    int                                             captures[15];
    ngx_int_t                                       n;

    ngx_http_push_stream_complex_value(r, cf->channels_path, &vv_channels_path);
    if (vv_channels_path.len == 0) {
        return NULL;
    }

    if ((requested_channels = ngx_pcalloc(pool, sizeof(ngx_http_push_stream_requested_channel_t))) == NULL) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "push stream module: unable to allocate memory for requested_channels queue");
        return NULL;
    }

    ngx_queue_init(&requested_channels->queue);

    // doing the parser of given channel path
    aux.data = vv_channels_path.data;
    do {
        aux.len = vv_channels_path.len - (aux.data - vv_channels_path.data);
        if ((n = ngx_regex_exec(mcf->backtrack_parser_regex, &aux, captures, 15)) >= 0) {
            if ((requested_channel = ngx_pcalloc(pool, sizeof(ngx_http_push_stream_requested_channel_t))) == NULL) {
                ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "push stream module: unable to allocate memory for channel_id item");
                return NULL;
            }

            if ((requested_channel->id = ngx_http_push_stream_create_str(pool, captures[0])) == NULL) {
                ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "push stream module: unable to allocate memory for channel_id string");
                return NULL;
            }
            ngx_memcpy(requested_channel->id->data, aux.data, captures[0]);
            requested_channel->backtrack_messages = 0;
            if (captures[7] > captures[6]) {
                requested_channel->backtrack_messages = ngx_atoi(aux.data + captures[6], captures[7] - captures[6]);
            }

            ngx_queue_insert_tail(&requested_channels->queue, &requested_channel->queue);

            aux.data = aux.data + captures[1];
        }
    } while ((n != NGX_REGEX_NO_MATCHED) && (aux.data < (vv_channels_path.data + vv_channels_path.len)));

    return requested_channels;
}


ngx_int_t
ngx_http_push_stream_create_shmtx(ngx_shmtx_t *mtx, ngx_shmtx_sh_t *addr, u_char *name)
{
    u_char           *file;

#if (NGX_HAVE_ATOMIC_OPS)

    file = NULL;

#else

    ngx_str_t        logs_dir = ngx_string("logs/");

    if (ngx_conf_full_name((ngx_cycle_t  *) ngx_cycle, &logs_dir, 0) != NGX_OK) {
        return NGX_ERROR;
    }

    file = ngx_pnalloc(ngx_cycle->pool, logs_dir.len + ngx_strlen(name));
    if (file == NULL) {
        return NGX_ERROR;
    }

    (void) ngx_sprintf(file, "%V%s%Z", &logs_dir, name);

#endif

    if (ngx_shmtx_create(mtx, addr, file) != NGX_OK) {
        return NGX_ERROR;
    }

    return NGX_OK;
}


ngx_flag_t
ngx_http_push_stream_is_utf8(u_char *p, size_t n)
{
    u_char  c, *last;
    size_t  len;

    last = p + n;

    for (len = 0; p < last; len++) {

        c = *p;

        if (c < 0x80) {
            p++;
            continue;
        }

        if (ngx_utf8_decode(&p, n) > 0x10ffff) {
            /* invalid UTF-8 */
            return 0;
        }
    }

    return 1;
}
