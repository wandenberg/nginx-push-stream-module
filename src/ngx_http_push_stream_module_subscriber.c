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
 * ngx_http_push_stream_module_subscriber.c
 *
 * Created: Oct 26, 2010
 * Authors: Wandenberg Peixoto <wandenberg@gmail.com>, Rogério Carvalho Schneider <stockrt@gmail.com>
 */

#include <ngx_http_push_stream_module_subscriber.h>

static ngx_int_t
ngx_http_push_stream_subscriber_handler(ngx_http_request_t *r)
{
    ngx_slab_pool_t                                *shpool = (ngx_slab_pool_t *)ngx_http_push_stream_shm_zone->shm.addr;
    ngx_http_push_stream_loc_conf_t                *cf = ngx_http_get_module_loc_conf(r, ngx_http_push_stream_module);
    ngx_pool_cleanup_t                             *cln;
    ngx_http_push_stream_subscriber_cleanup_t      *clndata;
    ngx_http_push_stream_worker_subscriber_t       *worker_subscriber;
    ngx_http_push_stream_requested_channel_t       *channels_ids, *cur;
    ngx_pool_t                                     *temp_pool;
    ngx_uint_t                                      subscribed_channels_qtd = 0;
    ngx_uint_t                                      subscribed_broadcast_channels_qtd = 0;
    ngx_http_push_stream_shm_data_t                *data = (ngx_http_push_stream_shm_data_t *) ngx_http_push_stream_shm_zone->data;
    ngx_http_push_stream_worker_data_t             *thisworker_data = data->ipc + ngx_process_slot;
    ngx_flag_t                                      is_broadcast_channel;
    ngx_http_push_stream_channel_t                 *channel;

    // only accept GET method
    if (!(r->method & NGX_HTTP_GET)) {
        ngx_http_push_stream_add_response_header(r, &NGX_HTTP_PUSH_STREAM_HEADER_ALLOW, &NGX_HTTP_PUSH_STREAM_ALLOW_GET);
        return ngx_http_push_stream_send_only_header_response(r, NGX_HTTP_NOT_ALLOWED, NULL);
    }

    //create a temporary pool to allocate temporary elements
    if ((temp_pool = ngx_create_pool(NGX_CYCLE_POOL_SIZE, r->connection->log)) == NULL) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "push stream module: unable to allocate memory for temporary pool");
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    //attach a cleaner to remove the request from the channel
    if ((cln = ngx_pool_cleanup_add(r->pool, sizeof(ngx_http_push_stream_subscriber_cleanup_t))) == NULL) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "push stream module: unable to allocate memory for cleanup");
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    if ((worker_subscriber = ngx_pcalloc(r->pool, sizeof(ngx_http_push_stream_worker_subscriber_t))) == NULL) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "push stream module: unable to allocate worker subscriber");
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    worker_subscriber->request = r;
    worker_subscriber->worker_subscribed_pid = ngx_pid;
    worker_subscriber->expires = (cf->subscriber_connection_timeout == NGX_CONF_UNSET) ? 0 : (ngx_time() + cf->subscriber_connection_timeout);
    ngx_queue_init(&worker_subscriber->queue);
    ngx_queue_init(&worker_subscriber->subscriptions_sentinel.queue);

    //get channels ids and backtracks from path
    channels_ids = ngx_http_push_stream_parse_channels_ids_from_path(r, temp_pool);
    if ((channels_ids == NULL) || ngx_queue_empty(&channels_ids->queue)) {
        ngx_log_error(NGX_LOG_WARN, r->connection->log, 0, "push stream module: the $push_stream_channel_path variable is required but is not set");
        ngx_destroy_pool(temp_pool);
        return ngx_http_push_stream_send_only_header_response(r, NGX_HTTP_BAD_REQUEST, &NGX_HTTP_PUSH_STREAM_NO_CHANNEL_ID_MESSAGE);
    }

    //validate channels: name, length and quantity. check if channel exists when authorized_channels_only is on
    cur = channels_ids;
    while ((cur = (ngx_http_push_stream_requested_channel_t *) ngx_queue_next(&cur->queue)) != channels_ids) {
        // could not be ALL channel
        if (ngx_memn2cmp(cur->id->data, NGX_HTTP_PUSH_STREAM_ALL_CHANNELS_INFO_ID.data, cur->id->len, NGX_HTTP_PUSH_STREAM_ALL_CHANNELS_INFO_ID.len) == 0) {
            ngx_destroy_pool(temp_pool);
            return ngx_http_push_stream_send_only_header_response(r, NGX_HTTP_FORBIDDEN, &NGX_HTTP_PUSH_STREAM_NO_CHANNEL_ID_NOT_AUTHORIZED_MESSAGE);
        }

        // could not have a large size
        if ((cf->max_channel_id_length != NGX_CONF_UNSET_UINT) && (cur->id->len > cf->max_channel_id_length)) {
            ngx_log_error(NGX_LOG_WARN, r->connection->log, 0, "push stream module: channel id is larger than allowed %d", cur->id->len);
            ngx_destroy_pool(temp_pool);
            return ngx_http_push_stream_send_only_header_response(r, NGX_HTTP_BAD_REQUEST, &NGX_HTTP_PUSH_STREAM_TOO_LARGE_CHANNEL_ID_MESSAGE);
        }

        // count subscribed channel and boradcasts
        subscribed_channels_qtd++;
        is_broadcast_channel = 0;
        if ((cf->broadcast_channel_prefix.len > 0) && (ngx_strncmp(cur->id->data, cf->broadcast_channel_prefix.data, cf->broadcast_channel_prefix.len) == 0)) {
            is_broadcast_channel = 1;
            subscribed_broadcast_channels_qtd++;
        }

        // check if channel exists when authorized_channels_only is on
        if (cf->authorized_channels_only && !is_broadcast_channel && (((channel = ngx_http_push_stream_find_channel(cur->id, r->connection->log)) == NULL) || (channel->stored_messages == 0))) {
            ngx_destroy_pool(temp_pool);
            return ngx_http_push_stream_send_only_header_response(r, NGX_HTTP_FORBIDDEN, &NGX_HTTP_PUSH_STREAM_CANNOT_CREATE_CHANNELS);
        }
    }

    // check if number of subscribed broadcast channels is acceptable
    if ((cf->broadcast_channel_max_qtd != NGX_CONF_UNSET_UINT) && (subscribed_broadcast_channels_qtd > 0) && ((subscribed_broadcast_channels_qtd > cf->broadcast_channel_max_qtd) || (subscribed_broadcast_channels_qtd == subscribed_channels_qtd))) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "push stream module: max subscribed broadcast channels exceeded");
        ngx_destroy_pool(temp_pool);
        return ngx_http_push_stream_send_only_header_response(r, NGX_HTTP_FORBIDDEN, &NGX_HTTP_PUSH_STREAM_TOO_MUCH_BROADCAST_CHANNELS);
    }

    // create the channels in advance, if doesn't exist, to ensure max number of channels in the server
    cur = channels_ids;
    while ((cur = (ngx_http_push_stream_requested_channel_t *) ngx_queue_next(&cur->queue)) != channels_ids) {
        channel = ngx_http_push_stream_get_channel(cur->id, r->connection->log, cf);
        if (channel == NULL) {
            ngx_log_error(NGX_LOG_ERR, (r)->connection->log, 0, "push stream module: unable to allocate memory for new channel");
            return ngx_http_push_stream_send_only_header_response(r, NGX_HTTP_INTERNAL_SERVER_ERROR, NULL);
        }

        if (channel == NGX_HTTP_PUSH_STREAM_NUMBER_OF_CHANNELS_EXCEEDED) {
            ngx_log_error(NGX_LOG_ERR, (r)->connection->log, 0, "push stream module: number of channels were exceeded");
            return ngx_http_push_stream_send_only_header_response(r, NGX_HTTP_FORBIDDEN, &NGX_HTTP_PUSH_STREAM_NUMBER_OF_CHANNELS_EXCEEDED_MESSAGE);
        }
    }

    // set a cleaner to subscriber
    cln->handler = (ngx_pool_cleanup_pt) ngx_http_push_stream_subscriber_cleanup;
    clndata = (ngx_http_push_stream_subscriber_cleanup_t *) cln->data;
    clndata->worker_subscriber = worker_subscriber;
    clndata->worker_subscriber->clndata = clndata;

    // increment request reference count to keep connection open
    r->main->count++;

    // responding subscriber
    r->read_event_handler = ngx_http_test_reading;
    r->write_event_handler = ngx_http_request_empty_handler;

    r->headers_out.content_type = cf->content_type;
    r->headers_out.status = NGX_HTTP_OK;
    r->headers_out.content_length_n = -1;

    ngx_http_push_stream_add_response_header(r, &NGX_HTTP_PUSH_STREAM_HEADER_TRANSFER_ENCODING, &NGX_HTTP_PUSH_STREAM_HEADER_CHUNCKED);
    ngx_http_send_header(r);

    // sending response content header
    if (ngx_http_push_stream_send_response_content_header(r, cf) == NGX_ERROR) {
        ngx_log_error(NGX_LOG_ERR, (r)->connection->log, 0, "push stream module: could not send content header to subscriber");
        ngx_destroy_pool(temp_pool);
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    ngx_shmtx_lock(&shpool->mutex);

    // adding subscriber to woker list of subscribers
    ngx_queue_insert_tail(&thisworker_data->worker_subscribers_sentinel->queue, &worker_subscriber->queue);

    // increment global subscribers count
    data->subscribers++;
    thisworker_data->subscribers++;

    ngx_shmtx_unlock(&shpool->mutex);

    // adding subscriber to channel(s) and send backtrack messages
    cur = channels_ids;
    while ((cur = (ngx_http_push_stream_requested_channel_t    *) ngx_queue_next(&cur->queue)) != channels_ids) {
        if (ngx_http_push_stream_subscriber_assign_channel(shpool, cf, r, cur, &worker_subscriber->subscriptions_sentinel, temp_pool) != NGX_OK) {
            ngx_destroy_pool(temp_pool);
            return NGX_HTTP_INTERNAL_SERVER_ERROR;
        }
    }

    // setting disconnect and ping timer
    ngx_http_push_stream_disconnect_timer_set(cf);
    ngx_http_push_stream_ping_timer_set(cf);

    ngx_destroy_pool(temp_pool);
    return NGX_DONE;
}

static ngx_int_t
ngx_http_push_stream_subscriber_assign_channel(ngx_slab_pool_t *shpool, ngx_http_push_stream_loc_conf_t *cf, ngx_http_request_t *r, ngx_http_push_stream_requested_channel_t *requested_channel, ngx_http_push_stream_subscription_t *subscriptions_sentinel, ngx_pool_t *temp_pool)
{
    ngx_http_push_stream_pid_queue_t           *sentinel, *cur, *found;
    ngx_http_push_stream_channel_t             *channel;
    ngx_http_push_stream_subscriber_t          *subscriber;
    ngx_http_push_stream_subscriber_t          *subscriber_sentinel;
    ngx_http_push_stream_msg_t                 *message, *message_sentinel;
    ngx_http_push_stream_subscription_t        *subscription;

    channel = ngx_http_push_stream_get_channel(requested_channel->id, r->connection->log, cf);
    if (channel == NULL) {
        // unable to allocate channel OR channel not found
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "push stream module: unable to allocate shared memory for channel %s", requested_channel->id->data);
        return NGX_ERROR;
    }

    if (channel == NGX_HTTP_PUSH_STREAM_NUMBER_OF_CHANNELS_EXCEEDED) {
        ngx_log_error(NGX_LOG_ERR, (r)->connection->log, 0, "push stream module: number of channels were exceeded");
        return NGX_ERROR;
    }


    sentinel = &channel->workers_with_subscribers;
    cur = sentinel;

    found = NULL;
    while ((cur = (ngx_http_push_stream_pid_queue_t *) ngx_queue_next(&cur->queue)) != sentinel) {
        if (cur->pid == ngx_pid) {
            found = cur;
            break;
        }
    }

    if (found == NULL) { // found nothing
        ngx_shmtx_lock(&shpool->mutex);
        // check if channel still exists
        channel = ngx_http_push_stream_find_channel_locked(requested_channel->id, r->connection->log);
        if (channel == NULL) {
            ngx_shmtx_unlock(&(shpool)->mutex);
            ngx_log_error(NGX_LOG_ERR, (r)->connection->log, 0, "push stream module: something goes very wrong, arrived on ngx_http_push_stream_subscriber_assign_channel without created channel %s", requested_channel->id->data);
            return NGX_ERROR;
        }

        if ((found = ngx_slab_alloc_locked(shpool, sizeof(ngx_http_push_stream_pid_queue_t))) == NULL) {
            ngx_shmtx_unlock(&shpool->mutex);
            ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "push stream module: unable to allocate worker subscriber queue marker in shared memory");
            return NGX_ERROR;
        }
        // initialize
        ngx_queue_insert_tail(&sentinel->queue, &found->queue);

        found->pid = ngx_pid;
        found->slot = ngx_process_slot;
        ngx_queue_init(&found->subscriber_sentinel.queue);
        ngx_shmtx_unlock(&shpool->mutex);
    }

    if ((subscription = ngx_palloc(r->pool, sizeof(ngx_http_push_stream_subscription_t))) == NULL) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "push stream module: unable to allocate subscribed channel reference");
        return NGX_ERROR;
    }

    if ((subscriber = ngx_palloc(r->pool, sizeof(ngx_http_push_stream_subscriber_t))) == NULL) { // unable to allocate request queue element
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "push stream module: unable to allocate subscribed channel reference");
        return NGX_ERROR;
    }

    subscriber_sentinel = &found->subscriber_sentinel;

    subscriber->request = r;

    subscription->channel = channel;
    subscription->subscriber = subscriber;

    // send old messages to new subscriber
    if (channel->stored_messages > 0) {
        message_sentinel = &channel->message_queue;
        message = message_sentinel;
        ngx_uint_t qtd = (requested_channel->backtrack_messages > channel->stored_messages) ? channel->stored_messages : requested_channel->backtrack_messages;
        ngx_uint_t start = channel->stored_messages - qtd;
        // positioning at first message, and send the others
        while ((qtd > 0) && (!message->deleted) && ((message = (ngx_http_push_stream_msg_t *) ngx_queue_next(&message->queue)) != message_sentinel)) {
            if (start == 0) {
                ngx_str_t *str = ngx_http_push_stream_get_formatted_message(r, channel, message, r->pool);
                if (str != NULL) {
                    ngx_http_push_stream_send_response_text(r, str->data, str->len, 0);
                }

                qtd--;
            } else {
                start--;
            }
        }
    }

    ngx_shmtx_lock(&shpool->mutex);
    // check if channel still exists
    channel = ngx_http_push_stream_find_channel_locked(requested_channel->id, r->connection->log);
    if (channel == NULL) {
        ngx_shmtx_unlock(&(shpool)->mutex);
        ngx_log_error(NGX_LOG_ERR, (r)->connection->log, 0, "push stream module: something goes very wrong, arrived on ngx_http_push_stream_subscriber_assign_channel without created channel %s", requested_channel->id->data);
        return NGX_ERROR;
    }
    channel->subscribers++; // do this only when we know everything went okay
    ngx_queue_insert_tail(&subscriptions_sentinel->queue, &subscription->queue);
    ngx_queue_insert_tail(&subscriber_sentinel->queue, &subscriber->queue);
    ngx_shmtx_unlock(&shpool->mutex);

    return NGX_OK;
}

ngx_http_push_stream_requested_channel_t *
ngx_http_push_stream_parse_channels_ids_from_path(ngx_http_request_t *r, ngx_pool_t *pool) {
    ngx_http_push_stream_loc_conf_t                *cf = ngx_http_get_module_loc_conf(r, ngx_http_push_stream_module);
    ngx_http_variable_value_t                      *vv_channels_path = ngx_http_get_indexed_variable(r, cf->index_channels_path);
    ngx_http_push_stream_requested_channel_t       *channels_ids, *cur;
    u_char                                         *channel_pos, *slash_pos, *backtrack_pos;
    ngx_uint_t                                      len, backtrack_messages;
    ngx_str_t                                      *channels_path;

    if (vv_channels_path == NULL || vv_channels_path->not_found || vv_channels_path->len == 0) {
        return NULL;
    }

    // make channels_path one unit larger than vv_channels_path to have allways a \0 in the end
    if ((channels_path = ngx_pcalloc(pool, sizeof(ngx_str_t) + vv_channels_path->len + 1)) == NULL) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "push stream module: unable to allocate memory for channels_path string");
        return NULL;
    }

    if ((channels_ids = ngx_pcalloc(pool, sizeof(ngx_http_push_stream_requested_channel_t))) == NULL) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "push stream module: unable to allocate memory for channels_ids queue");
        return NULL;
    }

    channels_path->data = (u_char *) (channels_path + 1);
    channels_path->len = vv_channels_path->len;
    ngx_memset(channels_path->data, '\0', vv_channels_path->len + 1);
    ngx_memcpy(channels_path->data, vv_channels_path->data, vv_channels_path->len);

    ngx_queue_init(&channels_ids->queue);

    channel_pos = channels_path->data;

    // doing the parser of given channel path
    while (channel_pos != NULL) {
        backtrack_messages = 0;
        len = 0;

        backtrack_pos = (u_char *) ngx_strstr(channel_pos, NGX_HTTP_PUSH_STREAM_BACKTRACK_SEP.data);
        slash_pos = (u_char *) ngx_strstr(channel_pos, NGX_HTTP_PUSH_STREAM_SLASH.data);

        if ((backtrack_pos != NULL) && (slash_pos != NULL)) {
            if (slash_pos > backtrack_pos) {
                len = backtrack_pos - channel_pos;
                backtrack_pos = backtrack_pos + NGX_HTTP_PUSH_STREAM_BACKTRACK_SEP.len;
                if (slash_pos > backtrack_pos) {
                    backtrack_messages = ngx_atoi(backtrack_pos, slash_pos - backtrack_pos);
                }
            } else {
                len = slash_pos - channel_pos;
            }
        } else if (backtrack_pos != NULL) {
            len = backtrack_pos - channel_pos;
            backtrack_pos = backtrack_pos + NGX_HTTP_PUSH_STREAM_BACKTRACK_SEP.len;
            if ((channels_path->data + channels_path->len) > backtrack_pos) {
                backtrack_messages = ngx_atoi(backtrack_pos, (channels_path->data + channels_path->len) - backtrack_pos);
            }
        } else if (slash_pos != NULL) {
            len = slash_pos - channel_pos;
        } else {
            len = channels_path->data + channels_path->len - channel_pos;
        }

        if (len > 0) {

            if ((cur = ngx_pcalloc(pool, sizeof(ngx_http_push_stream_requested_channel_t))) == NULL) {
                ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "push stream module: unable to allocate memory for channel_id item");
                return NULL;
            }

            if ((cur->id = ngx_pcalloc(pool, sizeof(ngx_str_t) + len + 1)) == NULL) {
                ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "push stream module: unable to allocate memory for channel_id string");
                return NULL;
            }
            cur->id->data = (u_char *) (cur->id + 1);
            cur->id->len = len;
            ngx_memset(cur->id->data, '\0', len + 1);
            ngx_memcpy(cur->id->data, channel_pos, len);
            cur->backtrack_messages = (backtrack_messages > 0) ? backtrack_messages : 0;

            ngx_queue_insert_tail(&channels_ids->queue, &cur->queue);
        }

        channel_pos = NULL;
        if (slash_pos != NULL) {
            channel_pos = slash_pos + NGX_HTTP_PUSH_STREAM_SLASH.len;
        }
    }

    return channels_ids;
}

static void
ngx_http_push_stream_subscriber_cleanup(ngx_http_push_stream_subscriber_cleanup_t *data)
{
    if (data->worker_subscriber != NULL) {
        ngx_http_push_stream_worker_subscriber_cleanup(data->worker_subscriber);
    }
}
