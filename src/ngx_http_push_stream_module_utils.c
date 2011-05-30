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

static ngx_inline void
ngx_http_push_stream_ensure_qtd_of_messages_locked(ngx_http_push_stream_channel_t *channel, ngx_uint_t max_messages, ngx_flag_t expired) {
    ngx_http_push_stream_msg_t             *sentinel, *msg;

    sentinel = &channel->message_queue;

    while (!ngx_queue_empty(&sentinel->queue) && ((channel->stored_messages > max_messages) || expired)) {
        msg = (ngx_http_push_stream_msg_t *)ngx_queue_next(&sentinel->queue);

        if (expired && msg->expires > ngx_time()) {
            break;
        }

        channel->stored_messages--;
        ngx_queue_remove(&msg->queue);
        ngx_http_push_stream_mark_message_to_delete_locked(msg);
    }

}


ngx_http_push_stream_msg_t *
ngx_http_push_stream_convert_buffer_to_msg_on_shared_locked(ngx_buf_t *buf)
{
    ngx_slab_pool_t                           *shpool = (ngx_slab_pool_t *) ngx_http_push_stream_shm_zone->shm.addr;
    ngx_http_push_stream_msg_t                *msg;
    off_t                                      len;

    len = ngx_buf_size(buf);

    msg = ngx_slab_alloc_locked(shpool, sizeof(ngx_http_push_stream_msg_t));
    if (msg == NULL) {
        return NULL;
    }

    msg->buf = ngx_slab_alloc_locked(shpool, sizeof(ngx_buf_t));
    if (msg->buf == NULL) {
        ngx_slab_free_locked(shpool, msg);
        return NULL;
    }

    msg->buf->start = ngx_slab_alloc_locked(shpool, len + 1);
    if (msg->buf->start == NULL) {
        ngx_slab_free_locked(shpool, msg->buf);
        ngx_slab_free_locked(shpool, msg);
        return NULL;
    }

    ngx_memset(msg->buf->start, '\0', len + 1);
    // copy the message to shared memory
    msg->buf->last = ngx_copy(msg->buf->start, buf->pos, len);

    msg->buf->pos = msg->buf->start;
    msg->buf->end = msg->buf->last + len;
    msg->buf->temporary = 1;
    msg->buf->memory = 1;
    msg->deleted = 0;
    msg->expires = 0;
    msg->queue.prev = NULL;
    msg->queue.next = NULL;

    return msg;
}


static ngx_int_t
ngx_http_push_stream_send_only_header_response(ngx_http_request_t *r, ngx_int_t status_code, const ngx_str_t *explain_error_message)
{
    ngx_int_t rc;

    ngx_http_discard_request_body(r);
    r->discard_body = 1;
    r->keepalive = 0;
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

static ngx_int_t
ngx_http_push_stream_send_response_content_header(ngx_http_request_t *r, ngx_http_push_stream_loc_conf_t *pslcf)
{
    ngx_int_t rc = NGX_OK;

    if (pslcf->header_template.len > 0) {
        rc = ngx_http_push_stream_send_response_chunk(r, pslcf->header_template.data, pslcf->header_template.len, 0);
    }

    return rc;
}

static ngx_int_t
ngx_http_push_stream_send_response_chunk(ngx_http_request_t *r, const u_char *chunk_text, uint chunk_len, ngx_flag_t last_buffer)
{
    ngx_buf_t     *b;
    ngx_chain_t   *out;

    if (chunk_text == NULL) {
        return NGX_ERROR;
    }

    out = (ngx_chain_t *) ngx_pcalloc(r->pool, sizeof(ngx_chain_t));
    b = ngx_calloc_buf(r->pool);
    if ((out == NULL) || (b == NULL)) {
        return NGX_ERROR;
    }

    b->last_buf = last_buffer;
    b->flush = 1;
    b->memory = 1;
    b->pos = (u_char *)chunk_text;
    b->start = b->pos;
    b->end = b->pos + chunk_len;
    b->last = b->end;

    out->buf = b;
    out->next = NULL;

    return ngx_http_output_filter(r, out);
}

static ngx_int_t
ngx_http_push_stream_send_ping(ngx_log_t *log, ngx_http_push_stream_loc_conf_t *pslcf)
{
    if (pslcf->message_template.len > 0) {
        ngx_http_push_stream_alert_worker_send_ping(ngx_pid, ngx_process_slot, ngx_cycle->log);
    }

    return NGX_OK;
}


static void
ngx_http_push_stream_collect_expired_messages_and_empty_channels(ngx_http_push_stream_shm_data_t *data, ngx_slab_pool_t *shpool, ngx_rbtree_node_t *node, ngx_flag_t force)
{
    ngx_http_push_stream_channel_t     *channel;

    channel = (ngx_http_push_stream_channel_t *) node;
    if ((channel != NULL) && (channel->deleted == 0) && (&channel->node != data->tree.sentinel) && (&channel->node != data->channels_to_delete.sentinel)) {

        if ((channel != NULL) && (channel->deleted == 0) && (channel->node.left != NULL)) {
            ngx_http_push_stream_collect_expired_messages_and_empty_channels(data, shpool, node->left, force);
        }

        if ((channel != NULL) && (channel->deleted == 0) && (channel->node.right != NULL)) {
            ngx_http_push_stream_collect_expired_messages_and_empty_channels(data, shpool, node->right, force);
        }

        ngx_shmtx_lock(&shpool->mutex);

        if ((channel != NULL) && (channel->deleted == 0)) {

            ngx_http_push_stream_ensure_qtd_of_messages_locked(channel, (force) ? 0 : channel->stored_messages, 1);

            if ((channel->stored_messages == 0) && (channel->subscribers == 0)) {
                channel->deleted = 1;
                channel->expires = ngx_time() + ngx_http_push_stream_module_main_conf->memory_cleanup_timeout;
                (channel->broadcast) ? data->broadcast_channels-- : data->channels--;

                // move the channel to trash tree
                ngx_rbtree_delete(&data->tree, (ngx_rbtree_node_t *) channel);
                channel->node.key = ngx_crc32_short(channel->id.data, channel->id.len);
                ngx_rbtree_insert(&data->channels_to_delete, (ngx_rbtree_node_t *) channel);
            }
        }

        ngx_shmtx_unlock(&shpool->mutex);
    }
}


static void
ngx_http_push_stream_collect_expired_messages(ngx_http_push_stream_shm_data_t *data, ngx_slab_pool_t *shpool, ngx_rbtree_node_t *node, ngx_flag_t force)
{
    ngx_http_push_stream_channel_t     *channel;

    channel = (ngx_http_push_stream_channel_t *) node;
    if ((channel != NULL) && (channel->deleted == 0) && (&channel->node != data->tree.sentinel) && (&channel->node != data->channels_to_delete.sentinel)) {

        if ((channel != NULL) && (channel->deleted == 0) && (channel->node.left != NULL)) {
            ngx_http_push_stream_collect_expired_messages(data, shpool, node->left, force);
        }

        if ((channel != NULL) && (channel->deleted == 0) && (channel->node.right != NULL)) {
            ngx_http_push_stream_collect_expired_messages(data, shpool, node->right, force);
        }

        ngx_shmtx_lock(&shpool->mutex);

        if ((channel != NULL) && (channel->deleted == 0)) {
            ngx_http_push_stream_ensure_qtd_of_messages_locked(channel, (force) ? 0 : channel->stored_messages, 1);
        }

        ngx_shmtx_unlock(&shpool->mutex);
    }
}


static void
ngx_http_push_stream_free_memory_of_expired_channels_locked(ngx_rbtree_t *tree, ngx_slab_pool_t *shpool, ngx_rbtree_node_t *node, ngx_flag_t force)
{
    ngx_rbtree_node_t                  *sentinel;
    ngx_http_push_stream_channel_t     *channel;

    sentinel = tree->sentinel;


    if (node != sentinel) {

        if (node->left != NULL) {
            ngx_http_push_stream_free_memory_of_expired_channels_locked(tree, shpool, node->left, force);
        }

        if (node->right != NULL) {
            ngx_http_push_stream_free_memory_of_expired_channels_locked(tree, shpool, node->right, force);
        }

        channel = (ngx_http_push_stream_channel_t *) node;

        if ((ngx_time() > channel->expires) || force) {
            ngx_rbtree_delete(tree, node);
            // delete the worker-subscriber queue
            ngx_http_push_stream_pid_queue_t     *workers_sentinel, *cur, *next;

            workers_sentinel = &channel->workers_with_subscribers;
            cur = (ngx_http_push_stream_pid_queue_t *)ngx_queue_next(&workers_sentinel->queue);

            while (cur != workers_sentinel) {
                next = (ngx_http_push_stream_pid_queue_t *)ngx_queue_next(&cur->queue);
                ngx_queue_remove(&cur->queue);
                ngx_slab_free_locked(shpool, cur);
                cur = next;
            }

            ngx_slab_free_locked(shpool, channel->id.data);
            ngx_slab_free_locked(shpool, channel);
        }
    }
}


static ngx_int_t
ngx_http_push_stream_memory_cleanup(ngx_log_t *log, ngx_http_push_stream_main_conf_t *psmcf)
{
    ngx_slab_pool_t                        *shpool = (ngx_slab_pool_t *) ngx_http_push_stream_shm_zone->shm.addr;
    ngx_http_push_stream_shm_data_t        *data = (ngx_http_push_stream_shm_data_t *) ngx_http_push_stream_shm_zone->data;

    ngx_http_push_stream_collect_expired_messages_and_empty_channels(data, shpool, data->tree.root, 0);
    ngx_http_push_stream_free_memory_of_expired_messages_and_channels(0);

    return NGX_OK;
}


static ngx_int_t
ngx_http_push_stream_buffer_cleanup(ngx_log_t *log, ngx_http_push_stream_loc_conf_t *pslcf)
{
    ngx_slab_pool_t                        *shpool = (ngx_slab_pool_t *) ngx_http_push_stream_shm_zone->shm.addr;
    ngx_http_push_stream_shm_data_t        *data = (ngx_http_push_stream_shm_data_t *) ngx_http_push_stream_shm_zone->data;

    ngx_http_push_stream_collect_expired_messages(data, shpool, data->tree.root, 0);

    return NGX_OK;
}


static ngx_int_t
ngx_http_push_stream_free_memory_of_expired_messages_and_channels(ngx_flag_t force)
{
    ngx_slab_pool_t                        *shpool = (ngx_slab_pool_t *) ngx_http_push_stream_shm_zone->shm.addr;
    ngx_http_push_stream_shm_data_t        *data = (ngx_http_push_stream_shm_data_t *) ngx_http_push_stream_shm_zone->data;
    ngx_http_push_stream_msg_t             *sentinel, *cur, *next;

    sentinel = &data->messages_to_delete;

    ngx_shmtx_lock(&shpool->mutex);
    cur = (ngx_http_push_stream_msg_t *)ngx_queue_next(&sentinel->queue);
    while (cur != sentinel) {
        next = (ngx_http_push_stream_msg_t *)ngx_queue_next(&cur->queue);
        if ((ngx_time() > cur->expires) || force) {
            ngx_queue_remove(&cur->queue);
            ngx_http_push_stream_free_message_memory_locked(shpool, cur);
        }
        cur = next;
    }
    ngx_http_push_stream_free_memory_of_expired_channels_locked(&data->channels_to_delete, shpool, data->channels_to_delete.root, force);
    ngx_shmtx_unlock(&shpool->mutex);

    return NGX_OK;
}


static void
ngx_http_push_stream_free_message_memory_locked(ngx_slab_pool_t *shpool, ngx_http_push_stream_msg_t *msg) {
    ngx_slab_free_locked(shpool, msg->buf->start);
    ngx_slab_free_locked(shpool, msg->buf);
    ngx_slab_free_locked(shpool, msg);
}


static void
ngx_http_push_stream_mark_message_to_delete_locked(ngx_http_push_stream_msg_t *msg) {
    ngx_http_push_stream_shm_data_t        *data = (ngx_http_push_stream_shm_data_t *) ngx_http_push_stream_shm_zone->data;

    msg->deleted = 1;
    msg->expires = ngx_time() + ngx_http_push_stream_module_main_conf->memory_cleanup_timeout;
    ngx_queue_insert_tail(&data->messages_to_delete.queue, &msg->queue);
}


static void
ngx_http_push_stream_ping_timer_set(ngx_http_push_stream_loc_conf_t *pslcf)
{
    if (pslcf->ping_message_interval != NGX_CONF_UNSET_MSEC) {
        ngx_slab_pool_t     *shpool = (ngx_slab_pool_t *) ngx_http_push_stream_shm_zone->shm.addr;

        if (ngx_http_push_stream_ping_event.handler == NULL) {
            ngx_shmtx_lock(&shpool->mutex);
            if (ngx_http_push_stream_ping_event.handler == NULL) {
                ngx_http_push_stream_ping_event.handler = ngx_http_push_stream_ping_timer_wake_handler;
                ngx_http_push_stream_ping_event.data = pslcf;
                ngx_http_push_stream_ping_event.log = ngx_cycle->log;
                ngx_http_push_stream_timer_reset(pslcf->ping_message_interval, &ngx_http_push_stream_ping_event);
            }
            ngx_shmtx_unlock(&shpool->mutex);
        }
    }
}

static void
ngx_http_push_stream_disconnect_timer_set(ngx_http_push_stream_loc_conf_t *pslcf)
{
    if (pslcf->subscriber_disconnect_interval != NGX_CONF_UNSET_MSEC) {
        ngx_slab_pool_t     *shpool = (ngx_slab_pool_t *) ngx_http_push_stream_shm_zone->shm.addr;

        if (ngx_http_push_stream_disconnect_event.handler == NULL) {
            ngx_shmtx_lock(&shpool->mutex);
            if (ngx_http_push_stream_disconnect_event.handler == NULL) {
                ngx_http_push_stream_disconnect_event.handler = ngx_http_push_stream_disconnect_timer_wake_handler;
                ngx_http_push_stream_disconnect_event.data = pslcf;
                ngx_http_push_stream_disconnect_event.log = ngx_cycle->log;
                ngx_http_push_stream_timer_reset(pslcf->subscriber_disconnect_interval, &ngx_http_push_stream_disconnect_event);
            }
            ngx_shmtx_unlock(&shpool->mutex);
        }
    }
}


static void
ngx_http_push_stream_memory_cleanup_timer_set(ngx_http_push_stream_main_conf_t *psmcf)
{
    if (psmcf->memory_cleanup_interval != NGX_CONF_UNSET_MSEC) {
        ngx_slab_pool_t     *shpool = (ngx_slab_pool_t *) ngx_http_push_stream_shm_zone->shm.addr;

        if (ngx_http_push_stream_memory_cleanup_event.handler == NULL) {
            ngx_shmtx_lock(&shpool->mutex);
            if (ngx_http_push_stream_memory_cleanup_event.handler == NULL) {
                ngx_http_push_stream_memory_cleanup_event.handler = ngx_http_push_stream_memory_cleanup_timer_wake_handler;
                ngx_http_push_stream_memory_cleanup_event.data = psmcf;
                ngx_http_push_stream_memory_cleanup_event.log = ngx_cycle->log;
                ngx_http_push_stream_timer_reset(psmcf->memory_cleanup_interval, &ngx_http_push_stream_memory_cleanup_event);
            }
            ngx_shmtx_unlock(&shpool->mutex);
        }
    }
}

static void
ngx_http_push_stream_buffer_cleanup_timer_set(ngx_http_push_stream_loc_conf_t *pslcf)
{
    if ((pslcf->buffer_cleanup_interval != NGX_CONF_UNSET_MSEC) && pslcf->store_messages) {
        ngx_slab_pool_t     *shpool = (ngx_slab_pool_t *) ngx_http_push_stream_shm_zone->shm.addr;

        if (ngx_http_push_stream_buffer_cleanup_event.handler == NULL) {
            ngx_shmtx_lock(&shpool->mutex);
            if (ngx_http_push_stream_buffer_cleanup_event.handler == NULL) {
                ngx_http_push_stream_buffer_cleanup_event.handler = ngx_http_push_stream_buffer_timer_wake_handler;
                ngx_http_push_stream_buffer_cleanup_event.data = pslcf;
                ngx_http_push_stream_buffer_cleanup_event.log = ngx_cycle->log;
                ngx_http_push_stream_timer_reset(pslcf->buffer_cleanup_interval, &ngx_http_push_stream_buffer_cleanup_event);
            }
            ngx_shmtx_unlock(&shpool->mutex);
        }
    }
}


static void
ngx_http_push_stream_timer_reset(ngx_msec_t timer_interval, ngx_event_t *timer_event)
{
    if (timer_interval != NGX_CONF_UNSET_MSEC) {
        if (timer_event->timedout) {
            #if defined nginx_version && nginx_version >= 7066
                ngx_time_update();
            #else
                ngx_time_update(0, 0);
            #endif
        }
        ngx_add_timer(timer_event, timer_interval);
    }
}


static void
ngx_http_push_stream_ping_timer_wake_handler(ngx_event_t *ev)
{
    ngx_http_push_stream_loc_conf_t     *pslcf = ev->data;


    ngx_http_push_stream_send_ping(ev->log, pslcf);
    ngx_http_push_stream_timer_reset(pslcf->ping_message_interval, &ngx_http_push_stream_ping_event);
}

static void
ngx_http_push_stream_disconnect_timer_wake_handler(ngx_event_t *ev)
{
    ngx_http_push_stream_loc_conf_t     *pslcf = ev->data;

    ngx_http_push_stream_alert_worker_disconnect_subscribers(ngx_pid, ngx_process_slot, ngx_cycle->log);
    ngx_http_push_stream_timer_reset(pslcf->subscriber_disconnect_interval, &ngx_http_push_stream_disconnect_event);
}

static void
ngx_http_push_stream_memory_cleanup_timer_wake_handler(ngx_event_t *ev)
{
    ngx_http_push_stream_main_conf_t     *psmcf = ev->data;

    ngx_http_push_stream_memory_cleanup(ev->log, psmcf);
    ngx_http_push_stream_timer_reset(psmcf->memory_cleanup_interval, &ngx_http_push_stream_memory_cleanup_event);
}

static void
ngx_http_push_stream_buffer_timer_wake_handler(ngx_event_t *ev)
{
    ngx_http_push_stream_loc_conf_t     *pslcf = ev->data;

    ngx_http_push_stream_buffer_cleanup(ev->log, pslcf);
    ngx_http_push_stream_timer_reset(pslcf->buffer_cleanup_interval, &ngx_http_push_stream_buffer_cleanup_event);
}

static u_char *
ngx_http_push_stream_str_replace(u_char *org, u_char *find, u_char *replace, ngx_uint_t offset, ngx_pool_t *pool)
{
    ngx_uint_t len_org = ngx_strlen(org);
    ngx_uint_t len_find = ngx_strlen(find);
    ngx_uint_t len_replace = ngx_strlen(replace);

    u_char      *result = org;

    if (len_find > 0) {
        u_char      *ret = (u_char *) ngx_strstr(org + offset, find);
        if (ret != NULL) {
            u_char      *tmp = ngx_pcalloc(pool, len_org + len_replace + len_find + 1);
            ngx_memset(tmp, '\0', len_org + len_replace + len_find + 1);

            u_int len_found = ret-org;
            ngx_memcpy(tmp, org, len_found);
            ngx_memcpy(tmp + len_found, replace, len_replace);
            ngx_memcpy(tmp + len_found + len_replace, org + len_found + len_find, len_org - len_found - len_find);

            result = ngx_http_push_stream_str_replace(tmp, find, replace, len_found + len_replace, pool);
        }
    }

    return result;
}


static ngx_buf_t *
ngx_http_push_stream_get_formatted_message(ngx_http_push_stream_loc_conf_t *pslcf, ngx_http_push_stream_channel_t *channel, ngx_buf_t *buf, ngx_pool_t *pool)
{
    ngx_uint_t                 len = 0;
    u_char                    *txt = NULL;

    if (pslcf->message_template.len > 0) {
        u_char template[pslcf->message_template.len + 1];
        ngx_memset(template, '\0', pslcf->message_template.len + 1);
        ngx_memcpy(template, pslcf->message_template.data, pslcf->message_template.len);

        u_char char_id[NGX_INT_T_LEN];
        ngx_memset(char_id, '\0', NGX_INT_T_LEN);
        u_char *msg = NGX_PUSH_STREAM_PING_MESSAGE_TEXT.data;
        u_char *channel_id = NGX_PUSH_STREAM_PING_CHANNEL_ID.data;

        if ((channel != NULL) && (buf != NULL)) {
            ngx_sprintf(char_id, "%d", channel->last_message_id + 1);
            len = ngx_buf_size(buf);
            msg = ngx_pcalloc(pool, len + 1);
            ngx_memset(msg, '\0', len + 1);
            ngx_memcpy(msg, buf->pos, len);
            channel_id = channel->id.data;
        } else {
            ngx_memcpy(char_id, NGX_PUSH_STREAM_PING_MESSAGE_ID.data, NGX_PUSH_STREAM_PING_MESSAGE_ID.len + 1);
        }

        txt = ngx_http_push_stream_str_replace(template, NGX_PUSH_STREAM_TOKEN_MESSAGE_ID.data, char_id, 0, pool);
        txt = ngx_http_push_stream_str_replace(txt, NGX_PUSH_STREAM_TOKEN_MESSAGE_CHANNEL.data, channel_id, 0, pool);
        txt = ngx_http_push_stream_str_replace(txt, NGX_PUSH_STREAM_TOKEN_MESSAGE_TEXT.data, msg, 0, pool);

        len = ngx_strlen(txt);
        buf = ngx_calloc_buf(pool);
    } else if (buf != NULL) {
        ngx_str_t msg = ngx_string(buf->pos);
        msg.len = ngx_buf_size(buf);
        txt = ngx_http_push_stream_append_crlf(&msg, pool);
        len = ngx_strlen(txt);
    }

    // global adjusts
    if (buf != NULL) {
        buf->pos = txt;
        buf->last = buf->pos + len;
        buf->start = buf->pos;
        buf->end = buf->last;
        buf->temporary = 1;
        buf->memory = 1;
    }
    return buf;
}


static void
ngx_http_push_stream_worker_subscriber_cleanup(ngx_http_push_stream_worker_subscriber_t *worker_subscriber)
{
    ngx_http_push_stream_subscription_t     *cur, *sentinel;
    ngx_slab_pool_t                         *shpool = (ngx_slab_pool_t *) ngx_http_push_stream_shm_zone->shm.addr;
    ngx_http_push_stream_shm_data_t         *data = (ngx_http_push_stream_shm_data_t *) ngx_http_push_stream_shm_zone->data;

    ngx_shmtx_lock(&shpool->mutex);
    sentinel = &worker_subscriber->subscriptions_sentinel;

    while ((cur = (ngx_http_push_stream_subscription_t *) ngx_queue_next(&sentinel->queue)) != sentinel) {
        cur->channel->subscribers--;
        ngx_queue_remove(&cur->subscriber->queue);
        ngx_queue_remove(&cur->queue);
    }
    ngx_queue_init(&sentinel->queue);
    ngx_queue_remove(&worker_subscriber->queue);
    ngx_queue_init(&worker_subscriber->queue);
    worker_subscriber->clndata->worker_subscriber = NULL;
    data->subscribers--;
    (data->ipc + ngx_process_slot)->subscribers--;
    ngx_shmtx_unlock(&shpool->mutex);
}

u_char *
ngx_http_push_stream_append_crlf(const ngx_str_t *str, ngx_pool_t *pool)
{
    u_char *last, *result;
    ngx_str_t crlf = ngx_string(CRLF);
    result = ngx_pcalloc(pool, str->len + crlf.len + 1);
    ngx_memset(result, '\0', str->len + crlf.len + 1);
    last = ngx_copy(result, str->data, str->len);
    last = ngx_copy(last, crlf.data, crlf.len);

    return result;
}

static ngx_http_push_stream_content_subtype_t *
ngx_http_push_stream_match_channel_info_format_and_content_type(ngx_http_request_t *r, ngx_uint_t default_subtype)
{
    ngx_uint_t      i;
    ngx_http_push_stream_content_subtype_t *subtype = &subtypes[default_subtype];

    if (r->headers_in.accept) {
        u_char     *cur = r->headers_in.accept->value.data;
        size_t      rem = 0;

        while((cur != NULL) && (cur = ngx_strnstr(cur, "/", r->headers_in.accept->value.len)) != NULL) {
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

    currenttime = (ngx_str_t *) ngx_pcalloc(pool, sizeof(ngx_str_t) + 20); //ISO 8601 pattern plus 1
    if (currenttime != NULL) {
        currenttime->data = (u_char *) (currenttime + 1);
        ngx_memset(currenttime->data, '\0', 20);
        ngx_gmtime(ngx_time(), &tm);
        ngx_sprintf(currenttime->data, (char *) NGX_PUSH_STREAM_DATE_FORMAT_ISO_8601.data, tm.ngx_tm_year, tm.ngx_tm_mon, tm.ngx_tm_mday, tm.ngx_tm_hour, tm.ngx_tm_min, tm.ngx_tm_sec);
        currenttime->len = ngx_strlen(currenttime->data);
    } else {
        currenttime = &NGX_HTTP_PUSH_STREAM_EMPTY;
    }

    return currenttime;
}

static ngx_str_t *
ngx_http_push_stream_get_formatted_hostname(ngx_pool_t *pool)
{
    ngx_str_t                          *hostname;

    hostname = (ngx_str_t *) ngx_pcalloc(pool, sizeof(ngx_str_t) + ngx_cycle->hostname.len + 1); //hostname length plus 1
    if (hostname != NULL) {
        hostname->data = (u_char *) (hostname + 1);
        ngx_memset(hostname->data, '\0', ngx_cycle->hostname.len + 1);
        ngx_memcpy(hostname->data, ngx_cycle->hostname.data, ngx_cycle->hostname.len);
        hostname->len = ngx_strlen(hostname->data);
    } else {
        hostname = &NGX_HTTP_PUSH_STREAM_EMPTY;
    }

    return hostname;
}
