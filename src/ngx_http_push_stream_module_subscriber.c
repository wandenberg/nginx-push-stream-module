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

static ngx_int_t                                 ngx_http_push_stream_subscriber_assign_channel(ngx_slab_pool_t *shpool, ngx_http_push_stream_loc_conf_t *cf, ngx_http_request_t *r, ngx_http_push_stream_requested_channel_t *requested_channel, time_t if_modified_since, ngx_str_t *last_event_id, ngx_http_push_stream_subscriber_t *subscriber, ngx_pool_t *temp_pool);
static ngx_http_push_stream_subscriber_t        *ngx_http_push_stream_subscriber_prepare_request_to_keep_connected(ngx_http_request_t *r);
static ngx_int_t                                 ngx_http_push_stream_registry_subscriber_locked(ngx_http_request_t *r, ngx_http_push_stream_subscriber_t *worker_subscriber);
static ngx_flag_t                                ngx_http_push_stream_has_old_messages_to_send(ngx_http_push_stream_channel_t *channel, ngx_uint_t backtrack, time_t if_modified_since, ngx_int_t tag, time_t greater_message_time, ngx_int_t greater_message_tag, ngx_str_t *last_event_id);
static void                                      ngx_http_push_stream_send_old_messages(ngx_http_request_t *r, ngx_http_push_stream_channel_t *channel, ngx_uint_t backtrack, time_t if_modified_since, ngx_int_t tag, time_t greater_message_time, ngx_int_t greater_message_tag, ngx_str_t *last_event_id);
static ngx_http_push_stream_pid_queue_t         *ngx_http_push_stream_create_worker_subscriber_channel_sentinel_locked(ngx_slab_pool_t *shpool, ngx_str_t *channel_id, ngx_log_t *log);
static ngx_http_push_stream_subscription_t      *ngx_http_push_stream_create_channel_subscription(ngx_http_request_t *r, ngx_http_push_stream_channel_t *channel, ngx_http_push_stream_subscriber_t *subscriber);
static ngx_int_t                                 ngx_http_push_stream_assing_subscription_to_channel_locked(ngx_slab_pool_t *shpool, ngx_str_t *channel_id, ngx_http_push_stream_subscription_t *subscription, ngx_http_push_stream_subscription_t *subscriptions_sentinel, ngx_log_t *log);
static ngx_int_t                                 ngx_http_push_stream_subscriber_polling_handler(ngx_http_request_t *r, ngx_http_push_stream_requested_channel_t *channels_ids, time_t if_modified_since, ngx_str_t *last_event_id, ngx_flag_t longpolling, ngx_pool_t *temp_pool);
static ngx_http_push_stream_padding_t           *ngx_http_push_stream_get_padding_by_user_agent(ngx_http_request_t *r);

static ngx_int_t
ngx_http_push_stream_subscriber_handler(ngx_http_request_t *r)
{
    ngx_slab_pool_t                                *shpool = (ngx_slab_pool_t *)ngx_http_push_stream_shm_zone->shm.addr;
    ngx_http_push_stream_loc_conf_t                *cf = ngx_http_get_module_loc_conf(r, ngx_http_push_stream_module);
    ngx_http_push_stream_subscriber_t              *worker_subscriber;
    ngx_http_push_stream_requested_channel_t       *channels_ids, *cur;
    ngx_http_push_stream_subscriber_ctx_t          *ctx;
    time_t                                          if_modified_since;
    ngx_str_t                                      *last_event_id, vv_time = ngx_null_string;
    ngx_str_t                                      *push_mode;
    ngx_flag_t                                      polling, longpolling;
    ngx_int_t                                       rc;
    ngx_int_t                                       status_code;
    ngx_str_t                                      *explain_error_message;

    // add headers to support cross domain requests
    ngx_http_push_stream_add_response_header(r, &NGX_HTTP_PUSH_STREAM_HEADER_ACCESS_CONTROL_ALLOW_ORIGIN, &cf->allowed_origins);
    ngx_http_push_stream_add_response_header(r, &NGX_HTTP_PUSH_STREAM_HEADER_ACCESS_CONTROL_ALLOW_METHODS, &NGX_HTTP_PUSH_STREAM_ALLOW_GET);
    ngx_http_push_stream_add_response_header(r, &NGX_HTTP_PUSH_STREAM_HEADER_ACCESS_CONTROL_ALLOW_HEADERS, &NGX_HTTP_PUSH_STREAM_ALLOWED_HEADERS);

    if (r->method & NGX_HTTP_OPTIONS) {
        return ngx_http_push_stream_send_only_header_response(r, NGX_HTTP_OK, NULL);
    }

    // only accept GET method
    if (!(r->method & NGX_HTTP_GET)) {
        ngx_http_push_stream_add_response_header(r, &NGX_HTTP_PUSH_STREAM_HEADER_ALLOW, &NGX_HTTP_PUSH_STREAM_ALLOW_GET);
        return ngx_http_push_stream_send_only_header_response(r, NGX_HTTP_NOT_ALLOWED, NULL);
    }

    if ((ctx = ngx_http_push_stream_add_request_context(r)) == NULL) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "push stream module: unable to create request context");
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    //get channels ids and backtracks from path
    channels_ids = ngx_http_push_stream_parse_channels_ids_from_path(r, ctx->temp_pool);
    if ((channels_ids == NULL) || ngx_queue_empty(&channels_ids->queue)) {
        ngx_log_error(NGX_LOG_WARN, r->connection->log, 0, "push stream module: the $push_stream_channels_path variable is required but is not set");
        return ngx_http_push_stream_send_only_header_response(r, NGX_HTTP_BAD_REQUEST, &NGX_HTTP_PUSH_STREAM_NO_CHANNEL_ID_MESSAGE);
    }

    //validate channels: name, length and quantity. check if channel exists when authorized_channels_only is on. check if channel is full of subscribers
    if (ngx_http_push_stream_validate_channels(r, channels_ids, &status_code, &explain_error_message) == NGX_ERROR) {
        return ngx_http_push_stream_send_only_header_response(r, status_code, explain_error_message);
    }

    if (cf->last_received_message_time != NULL) {
        ngx_http_push_stream_complex_value(r, cf->last_received_message_time, &vv_time);
    } else if (r->headers_in.if_modified_since != NULL) {
        vv_time = r->headers_in.if_modified_since->value;
    }

    // get control headers
    if_modified_since = vv_time.len ? ngx_http_parse_time(vv_time.data, vv_time.len) : -1;
    last_event_id = ngx_http_push_stream_get_header(r, &NGX_HTTP_PUSH_STREAM_HEADER_LAST_EVENT_ID);

    push_mode = ngx_http_push_stream_get_header(r, &NGX_HTTP_PUSH_STREAM_HEADER_MODE);
    polling = ((cf->location_type == NGX_HTTP_PUSH_STREAM_SUBSCRIBER_MODE_POLLING) || ((push_mode != NULL) && (push_mode->len == NGX_HTTP_PUSH_STREAM_MODE_POLLING.len) && (ngx_strncasecmp(push_mode->data, NGX_HTTP_PUSH_STREAM_MODE_POLLING.data, NGX_HTTP_PUSH_STREAM_MODE_POLLING.len) == 0)));
    longpolling = ((cf->location_type == NGX_HTTP_PUSH_STREAM_SUBSCRIBER_MODE_LONGPOLLING) || ((push_mode != NULL) && (push_mode->len == NGX_HTTP_PUSH_STREAM_MODE_LONGPOLLING.len) && (ngx_strncasecmp(push_mode->data, NGX_HTTP_PUSH_STREAM_MODE_LONGPOLLING.data, NGX_HTTP_PUSH_STREAM_MODE_LONGPOLLING.len) == 0)));

    if (polling || longpolling) {
        ngx_int_t result = ngx_http_push_stream_subscriber_polling_handler(r, channels_ids, if_modified_since, last_event_id, longpolling, ctx->temp_pool);
        if (ctx->temp_pool != NULL) {
            ngx_destroy_pool(ctx->temp_pool);
            ctx->temp_pool = NULL;
        }
        return result;
    }

    ctx->padding = ngx_http_push_stream_get_padding_by_user_agent(r);

    // stream access
    if ((worker_subscriber = ngx_http_push_stream_subscriber_prepare_request_to_keep_connected(r)) == NULL) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    ngx_http_push_stream_add_response_header(r, &NGX_HTTP_PUSH_STREAM_HEADER_TRANSFER_ENCODING, &NGX_HTTP_PUSH_STREAM_HEADER_CHUNCKED);
    ngx_http_send_header(r);

    // sending response content header
    if (ngx_http_push_stream_send_response_content_header(r, cf) == NGX_ERROR) {
        ngx_log_error(NGX_LOG_ERR, (r)->connection->log, 0, "push stream module: could not send content header to subscriber");
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    ngx_shmtx_lock(&shpool->mutex);
    rc = ngx_http_push_stream_registry_subscriber_locked(r, worker_subscriber);
    ngx_shmtx_unlock(&shpool->mutex);

    if (rc == NGX_ERROR) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    // adding subscriber to channel(s) and send backtrack messages
    cur = channels_ids;
    while ((cur = (ngx_http_push_stream_requested_channel_t *) ngx_queue_next(&cur->queue)) != channels_ids) {
        if (ngx_http_push_stream_subscriber_assign_channel(shpool, cf, r, cur, if_modified_since, last_event_id, worker_subscriber, ctx->temp_pool) != NGX_OK) {
            return NGX_HTTP_INTERNAL_SERVER_ERROR;
        }
    }

    if (ctx->temp_pool != NULL) {
        ngx_destroy_pool(ctx->temp_pool);
        ctx->temp_pool = NULL;
    }
    return NGX_DONE;
}

static ngx_int_t
ngx_http_push_stream_subscriber_polling_handler(ngx_http_request_t *r, ngx_http_push_stream_requested_channel_t *channels_ids, time_t if_modified_since, ngx_str_t *last_event_id, ngx_flag_t longpolling, ngx_pool_t *temp_pool)
{
    ngx_http_push_stream_loc_conf_t                *cf = ngx_http_get_module_loc_conf(r, ngx_http_push_stream_module);
    ngx_slab_pool_t                                *shpool = (ngx_slab_pool_t *)ngx_http_push_stream_shm_zone->shm.addr;
    ngx_http_push_stream_subscriber_ctx_t          *ctx = ngx_http_get_module_ctx(r, ngx_http_push_stream_module);
    ngx_http_push_stream_requested_channel_t       *cur;
    ngx_http_push_stream_subscriber_t              *worker_subscriber;
    ngx_http_push_stream_channel_t                 *channel;
    ngx_http_push_stream_subscription_t            *subscription;
    ngx_str_t                                      *etag = NULL, vv_etag = ngx_null_string;
    ngx_int_t                                       tag;
    time_t                                          greater_message_time;
    ngx_int_t                                       greater_message_tag;
    ngx_flag_t                                      has_message_to_send = 0;
    ngx_str_t                                       callback_function_name;

    if (cf->last_received_message_tag != NULL) {
        ngx_http_push_stream_complex_value(r, cf->last_received_message_tag, &vv_etag);
        etag = vv_etag.len ? &vv_etag : NULL;
    } else {
        etag = ngx_http_push_stream_get_header(r, &NGX_HTTP_PUSH_STREAM_HEADER_IF_NONE_MATCH);
    }

    if (ngx_http_arg(r, NGX_HTTP_PUSH_STREAM_CALLBACK.data, NGX_HTTP_PUSH_STREAM_CALLBACK.len, &callback_function_name) == NGX_OK) {
        ngx_http_push_stream_unescape_uri(&callback_function_name);
        if ((ctx->callback = ngx_http_push_stream_get_formatted_chunk(callback_function_name.data, callback_function_name.len, r->pool)) == NULL) {
            ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "push stream module: unable to allocate memory for callback function name");
            return NGX_HTTP_INTERNAL_SERVER_ERROR;
        }
    }

    tag = ((etag != NULL) && ((tag = ngx_atoi(etag->data, etag->len)) != NGX_ERROR)) ? ngx_abs(tag) : -1;

    greater_message_tag = tag;
    greater_message_time = if_modified_since = (if_modified_since < 0) ? 0 : if_modified_since;

    ngx_shmtx_lock(&shpool->mutex);

    // check if has any message to send
    cur = channels_ids;
    while ((cur = (ngx_http_push_stream_requested_channel_t *) ngx_queue_next(&cur->queue)) != channels_ids) {
        channel = ngx_http_push_stream_find_channel(cur->id, r->connection->log);
        if (channel == NULL) {
            // channel not found
            ngx_shmtx_unlock(&shpool->mutex);
            ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "push stream module: unable to allocate shared memory for channel %s", cur->id->data);
            return NGX_HTTP_INTERNAL_SERVER_ERROR;
        }

        if (ngx_http_push_stream_has_old_messages_to_send(channel, cur->backtrack_messages, if_modified_since, tag, greater_message_time, greater_message_tag, last_event_id)) {
            has_message_to_send = 1;
            if (channel->last_message_time > greater_message_time) {
                greater_message_time = channel->last_message_time;
                greater_message_tag = channel->last_message_tag;
            } else {
                if ((channel->last_message_time == greater_message_time) && (channel->last_message_tag > greater_message_tag) ) {
                    greater_message_tag = channel->last_message_tag;
                }
            }
        }
    }


    if (longpolling && !has_message_to_send) {
        // long polling mode without messages
        if ((worker_subscriber = ngx_http_push_stream_subscriber_prepare_request_to_keep_connected(r)) == NULL) {
            ngx_shmtx_unlock(&shpool->mutex);
            return NGX_HTTP_INTERNAL_SERVER_ERROR;
        }
        worker_subscriber->longpolling = 1;

        if (ngx_http_push_stream_registry_subscriber_locked(r, worker_subscriber) == NGX_ERROR) {
            ngx_shmtx_unlock(&shpool->mutex);
            return NGX_HTTP_INTERNAL_SERVER_ERROR;
        }

        // adding subscriber to channel(s)
        cur = channels_ids;
        while ((cur = (ngx_http_push_stream_requested_channel_t *) ngx_queue_next(&cur->queue)) != channels_ids) {
            if ((channel = ngx_http_push_stream_find_channel(cur->id, r->connection->log)) == NULL) {
                // channel not found
                ngx_shmtx_unlock(&shpool->mutex);
                ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "push stream module: unable to allocate shared memory for channel %s", cur->id->data);
                return NGX_HTTP_INTERNAL_SERVER_ERROR;
            }

            if ((subscription = ngx_http_push_stream_create_channel_subscription(r, channel, worker_subscriber)) == NULL) {
                ngx_shmtx_unlock(&shpool->mutex);
                return NGX_HTTP_INTERNAL_SERVER_ERROR;
            }

            ngx_http_push_stream_assing_subscription_to_channel_locked(shpool, cur->id, subscription, &worker_subscriber->subscriptions_sentinel, r->connection->log);
        }

        ngx_shmtx_unlock(&shpool->mutex);
        return NGX_DONE;
    }

    ngx_shmtx_unlock(&shpool->mutex);

    // polling or long polling without messages to send

    ngx_http_push_stream_add_polling_headers(r, greater_message_time, greater_message_tag, temp_pool);

    if (!has_message_to_send) {
        // polling subscriber requests get a 304 with their entity tags preserved if don't have new messages.
        return ngx_http_push_stream_send_only_header_response(r, NGX_HTTP_NOT_MODIFIED, NULL);
    }

    // polling with messages or long polling without messages to send
    r->headers_out.status = NGX_HTTP_OK;
    r->headers_out.content_length_n = -1;

    ngx_http_push_stream_add_response_header(r, &NGX_HTTP_PUSH_STREAM_HEADER_TRANSFER_ENCODING, &NGX_HTTP_PUSH_STREAM_HEADER_CHUNCKED);
    ngx_http_send_header(r);

    // sending response content header
    if (ngx_http_push_stream_send_response_content_header(r, cf) == NGX_ERROR) {
        ngx_log_error(NGX_LOG_ERR, (r)->connection->log, 0, "push stream module: could not send content header to subscriber");
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    if (ctx->callback != NULL) {
        ngx_http_push_stream_send_response_text(r, ctx->callback->data, ctx->callback->len, 0);
        ngx_http_push_stream_send_response_text(r, NGX_HTTP_PUSH_STREAM_CALLBACK_INIT_CHUNK.data, NGX_HTTP_PUSH_STREAM_CALLBACK_INIT_CHUNK.len, 0);
    }

    cur = channels_ids;
    while ((cur = (ngx_http_push_stream_requested_channel_t *) ngx_queue_next(&cur->queue)) != channels_ids) {
        channel = ngx_http_push_stream_find_channel(cur->id, r->connection->log);
        if (channel == NULL) {
            // channel not found
            ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "push stream module: unable to allocate shared memory for channel %s", cur->id->data);
            return NGX_HTTP_INTERNAL_SERVER_ERROR;
        }
        ngx_http_push_stream_send_old_messages(r, channel, cur->backtrack_messages, if_modified_since, tag, greater_message_time, greater_message_tag, last_event_id);
    }

    if (ctx->callback != NULL) {
        ngx_http_push_stream_send_response_text(r, NGX_HTTP_PUSH_STREAM_CALLBACK_END_CHUNK.data, NGX_HTTP_PUSH_STREAM_CALLBACK_END_CHUNK.len, 0);
    }

    if (cf->footer_template.len > 0) {
        ngx_http_push_stream_send_response_text(r, cf->footer_template.data, cf->footer_template.len, 0);
    }

    ngx_http_push_stream_send_response_text(r, NGX_HTTP_PUSH_STREAM_LAST_CHUNK.data, NGX_HTTP_PUSH_STREAM_LAST_CHUNK.len, 1);

    return NGX_OK;
}

static ngx_int_t
ngx_http_push_stream_subscriber_assign_channel(ngx_slab_pool_t *shpool, ngx_http_push_stream_loc_conf_t *cf, ngx_http_request_t *r, ngx_http_push_stream_requested_channel_t *requested_channel, time_t if_modified_since, ngx_str_t *last_event_id, ngx_http_push_stream_subscriber_t *subscriber, ngx_pool_t *temp_pool)
{
    ngx_http_push_stream_channel_t             *channel;
    ngx_http_push_stream_subscription_t        *subscription;
    ngx_int_t                                   result;

    if ((channel = ngx_http_push_stream_find_channel(requested_channel->id, r->connection->log)) == NULL) {
        // channel not found
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "push stream module: unable to allocate shared memory for channel %s", requested_channel->id->data);
        return NGX_ERROR;
    }

    if ((subscription = ngx_http_push_stream_create_channel_subscription(r, channel, subscriber)) == NULL) {
        return NGX_ERROR;
    }

    // send old messages to new subscriber
    ngx_http_push_stream_send_old_messages(r, channel, requested_channel->backtrack_messages, if_modified_since, 0, 0, -1, last_event_id);

    ngx_shmtx_lock(&shpool->mutex);
    result = ngx_http_push_stream_assing_subscription_to_channel_locked(shpool, requested_channel->id, subscription, &subscriber->subscriptions_sentinel, r->connection->log);
    ngx_shmtx_unlock(&shpool->mutex);

    return result;
}


ngx_http_push_stream_requested_channel_t *
ngx_http_push_stream_parse_channels_ids_from_path(ngx_http_request_t *r, ngx_pool_t *pool) {
    ngx_http_push_stream_main_conf_t               *mcf = ngx_http_get_module_main_conf(r, ngx_http_push_stream_module);
    ngx_http_push_stream_loc_conf_t                *cf = ngx_http_get_module_loc_conf(r, ngx_http_push_stream_module);
    ngx_http_variable_value_t                      *vv_channels_path = ngx_http_get_indexed_variable(r, cf->index_channels_path);
    ngx_http_push_stream_requested_channel_t       *channels_ids, *cur;
    ngx_str_t                                       aux;
    int                                             captures[15];
    ngx_int_t                                       n;

    if (vv_channels_path == NULL || vv_channels_path->not_found || vv_channels_path->len == 0) {
        return NULL;
    }

    if ((channels_ids = ngx_pcalloc(pool, sizeof(ngx_http_push_stream_requested_channel_t))) == NULL) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "push stream module: unable to allocate memory for channels_ids queue");
        return NULL;
    }

    ngx_queue_init(&channels_ids->queue);

    // doing the parser of given channel path
    aux.data = vv_channels_path->data;
    do {
        aux.len = vv_channels_path->len - (aux.data - vv_channels_path->data);
        if ((n = ngx_regex_exec(mcf->backtrack_parser_regex, &aux, captures, 15)) >= 0) {
            if ((cur = ngx_pcalloc(pool, sizeof(ngx_http_push_stream_requested_channel_t))) == NULL) {
                ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "push stream module: unable to allocate memory for channel_id item");
                return NULL;
            }

            if ((cur->id = ngx_http_push_stream_create_str(pool, captures[0])) == NULL) {
                ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "push stream module: unable to allocate memory for channel_id string");
                return NULL;
            }
            ngx_memcpy(cur->id->data, aux.data, captures[0]);
            cur->backtrack_messages = 0;
            if (captures[7] > captures[6]) {
                cur->backtrack_messages = ngx_atoi(aux.data + captures[6], captures[7] - captures[6]);
            }

            ngx_queue_insert_tail(&channels_ids->queue, &cur->queue);

            aux.data = aux.data + captures[1];
        }
    } while ((n != NGX_REGEX_NO_MATCHED) && (aux.data < (vv_channels_path->data + vv_channels_path->len)));

    return channels_ids;
}


static ngx_int_t
ngx_http_push_stream_validate_channels(ngx_http_request_t *r, ngx_http_push_stream_requested_channel_t *channels_ids, ngx_int_t *status_code, ngx_str_t **explain_error_message)
{
    ngx_http_push_stream_main_conf_t               *mcf = ngx_http_push_stream_module_main_conf;
    ngx_http_push_stream_loc_conf_t                *cf = ngx_http_get_module_loc_conf(r, ngx_http_push_stream_module);
    ngx_http_push_stream_requested_channel_t       *cur = channels_ids;
    ngx_uint_t                                      subscribed_channels_qtd = 0;
    ngx_uint_t                                      subscribed_broadcast_channels_qtd = 0;
    ngx_flag_t                                      is_broadcast_channel;
    ngx_http_push_stream_channel_t                 *channel;

    while ((cur = (ngx_http_push_stream_requested_channel_t *) ngx_queue_next(&cur->queue)) != channels_ids) {
        // could not be ALL channel or contain wildcard
        if ((ngx_memn2cmp(cur->id->data, NGX_HTTP_PUSH_STREAM_ALL_CHANNELS_INFO_ID.data, cur->id->len, NGX_HTTP_PUSH_STREAM_ALL_CHANNELS_INFO_ID.len) == 0) || (ngx_strchr(cur->id->data, '*') != NULL)) {
            *status_code = NGX_HTTP_FORBIDDEN;
            *explain_error_message = (ngx_str_t *) &NGX_HTTP_PUSH_STREAM_NO_CHANNEL_ID_NOT_AUTHORIZED_MESSAGE;
            return NGX_ERROR;
        }

        // could not have a large size
        if ((mcf->max_channel_id_length != NGX_CONF_UNSET_UINT) && (cur->id->len > mcf->max_channel_id_length)) {
            ngx_log_error(NGX_LOG_WARN, r->connection->log, 0, "push stream module: channel id is larger than allowed %d", cur->id->len);
            *status_code = NGX_HTTP_BAD_REQUEST;
            *explain_error_message = (ngx_str_t *) &NGX_HTTP_PUSH_STREAM_TOO_LARGE_CHANNEL_ID_MESSAGE;
            return NGX_ERROR;
        }

        // count subscribed channel and broadcasts
        subscribed_channels_qtd++;
        is_broadcast_channel = 0;
        if ((mcf->broadcast_channel_prefix.len > 0) && (ngx_strncmp(cur->id->data, mcf->broadcast_channel_prefix.data, mcf->broadcast_channel_prefix.len) == 0)) {
            is_broadcast_channel = 1;
            subscribed_broadcast_channels_qtd++;
        }

        // check if channel exists when authorized_channels_only is on
        if (cf->authorized_channels_only && !is_broadcast_channel && (((channel = ngx_http_push_stream_find_channel(cur->id, r->connection->log)) == NULL) || (channel->stored_messages == 0))) {
            *status_code = NGX_HTTP_FORBIDDEN;
            *explain_error_message = (ngx_str_t *) &NGX_HTTP_PUSH_STREAM_CANNOT_CREATE_CHANNELS;
            return NGX_ERROR;
        }

        // check if channel is full of subscribers
        if ((mcf->max_subscribers_per_channel != NGX_CONF_UNSET_UINT) && (((channel = ngx_http_push_stream_find_channel(cur->id, r->connection->log)) != NULL) && (channel->subscribers >= mcf->max_subscribers_per_channel))) {
            *status_code = NGX_HTTP_FORBIDDEN;
            *explain_error_message = (ngx_str_t *) &NGX_HTTP_PUSH_STREAM_TOO_SUBSCRIBERS_PER_CHANNEL;
            return NGX_ERROR;
        }
    }

    // check if number of subscribed broadcast channels is acceptable
    if ((cf->broadcast_channel_max_qtd != NGX_CONF_UNSET_UINT) && (subscribed_broadcast_channels_qtd > 0) && ((subscribed_broadcast_channels_qtd > cf->broadcast_channel_max_qtd) || (subscribed_broadcast_channels_qtd == subscribed_channels_qtd))) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "push stream module: max subscribed broadcast channels exceeded");
        *status_code = NGX_HTTP_FORBIDDEN;
        *explain_error_message = (ngx_str_t *) &NGX_HTTP_PUSH_STREAM_TOO_MUCH_BROADCAST_CHANNELS;
        return NGX_ERROR;
    }

    // create the channels in advance, if doesn't exist, to ensure max number of channels in the server
    cur = channels_ids;
    while ((cur = (ngx_http_push_stream_requested_channel_t *) ngx_queue_next(&cur->queue)) != channels_ids) {
        channel = ngx_http_push_stream_get_channel(cur->id, r->connection->log, cf);
        if (channel == NULL) {
            ngx_log_error(NGX_LOG_ERR, (r)->connection->log, 0, "push stream module: unable to allocate memory for new channel");
            *status_code = NGX_HTTP_INTERNAL_SERVER_ERROR;
            *explain_error_message = NULL;
            return NGX_ERROR;
        }

        if (channel == NGX_HTTP_PUSH_STREAM_NUMBER_OF_CHANNELS_EXCEEDED) {
            ngx_log_error(NGX_LOG_ERR, (r)->connection->log, 0, "push stream module: number of channels were exceeded");
            *status_code = NGX_HTTP_FORBIDDEN;
            *explain_error_message = (ngx_str_t *) &NGX_HTTP_PUSH_STREAM_NUMBER_OF_CHANNELS_EXCEEDED_MESSAGE;
            return NGX_ERROR;
        }
    }

    return NGX_OK;
}


static ngx_http_push_stream_subscriber_t *
ngx_http_push_stream_subscriber_prepare_request_to_keep_connected(ngx_http_request_t *r)
{
    ngx_http_push_stream_loc_conf_t                *cf = ngx_http_get_module_loc_conf(r, ngx_http_push_stream_module);
    ngx_http_push_stream_subscriber_ctx_t          *ctx = ngx_http_get_module_ctx(r, ngx_http_push_stream_module);
    ngx_http_push_stream_subscriber_t              *worker_subscriber;

    if ((worker_subscriber = ngx_pcalloc(r->pool, sizeof(ngx_http_push_stream_subscriber_t))) == NULL) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "push stream module: unable to allocate worker subscriber");
        return NULL;
    }

    worker_subscriber->longpolling = 0;
    worker_subscriber->request = r;
    worker_subscriber->worker_subscribed_pid = ngx_pid;
    ngx_queue_init(&worker_subscriber->subscriptions_sentinel.queue);
    ctx->subscriber = worker_subscriber;

    // increment request reference count to keep connection open
    r->main->count++;

    // responding subscriber
    r->read_event_handler = ngx_http_test_reading;
    r->write_event_handler = ngx_http_request_empty_handler;

    r->headers_out.content_type = cf->content_type;
    r->headers_out.status = NGX_HTTP_OK;
    r->headers_out.content_length_n = -1;

    return worker_subscriber;
}

static ngx_int_t
ngx_http_push_stream_registry_subscriber_locked(ngx_http_request_t *r, ngx_http_push_stream_subscriber_t *worker_subscriber)
{
    ngx_http_push_stream_shm_data_t                *data = (ngx_http_push_stream_shm_data_t *) ngx_http_push_stream_shm_zone->data;
    ngx_http_push_stream_worker_data_t             *thisworker_data = data->ipc + ngx_process_slot;
    ngx_http_push_stream_loc_conf_t                *cf = ngx_http_get_module_loc_conf(r, ngx_http_push_stream_module);
    ngx_msec_t                                      connection_ttl = worker_subscriber->longpolling ? cf->longpolling_connection_ttl : cf->subscriber_connection_ttl;
    ngx_http_push_stream_queue_elem_t              *element_subscriber;
    ngx_http_push_stream_subscriber_ctx_t          *ctx = ngx_http_get_module_ctx(r, ngx_http_push_stream_module);

    if ((element_subscriber = ngx_palloc(r->pool, sizeof(ngx_http_push_stream_queue_elem_t))) == NULL) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "push stream module: unable to allocate subscriber reference");
        return NGX_ERROR;
    }
    element_subscriber->value = worker_subscriber;
    worker_subscriber->worker_subscriber_element_ref = element_subscriber;

    // adding subscriber to worker list of subscribers
    ngx_queue_insert_tail(&thisworker_data->subscribers_sentinel->queue, &element_subscriber->queue);

    ctx->longpolling = worker_subscriber->longpolling;
    ctx->subscriber = worker_subscriber;

    if ((connection_ttl != NGX_CONF_UNSET_MSEC) || (cf->ping_message_interval != NGX_CONF_UNSET_MSEC)) {

        if (connection_ttl != NGX_CONF_UNSET_MSEC) {
            if ((ctx->disconnect_timer = ngx_pcalloc(worker_subscriber->request->pool, sizeof(ngx_event_t))) == NULL) {
                return NGX_ERROR;
            }
        }

        if ((!ctx->longpolling) && (cf->ping_message_interval != NGX_CONF_UNSET_MSEC)) {
            if ((ctx->ping_timer = ngx_pcalloc(worker_subscriber->request->pool, sizeof(ngx_event_t))) == NULL) {
                return NGX_ERROR;
            }
        }

        if (ctx->disconnect_timer != NULL) {
            ctx->disconnect_timer->handler = ngx_http_push_stream_disconnect_timer_wake_handler;
            ctx->disconnect_timer->data = worker_subscriber->request;
            ctx->disconnect_timer->log = worker_subscriber->request->connection->log;
            ngx_http_push_stream_timer_reset(connection_ttl, ctx->disconnect_timer);
        }

        if (ctx->ping_timer != NULL) {
            ctx->ping_timer->handler = ngx_http_push_stream_ping_timer_wake_handler;
            ctx->ping_timer->data = worker_subscriber->request;
            ctx->ping_timer->log = worker_subscriber->request->connection->log;
            ngx_http_push_stream_timer_reset(cf->ping_message_interval, ctx->ping_timer);
        }
    }

    // increment global subscribers count
    data->subscribers++;
    thisworker_data->subscribers++;

    return NGX_OK;
}

static ngx_flag_t
ngx_http_push_stream_has_old_messages_to_send(ngx_http_push_stream_channel_t *channel, ngx_uint_t backtrack, time_t if_modified_since, ngx_int_t tag, time_t greater_message_time, ngx_int_t greater_message_tag, ngx_str_t *last_event_id)
{
    ngx_flag_t old_messages = 0;
    ngx_http_push_stream_msg_t *message, *message_sentinel;

    message_sentinel = &channel->message_queue;
    message = message_sentinel;
    if (channel->stored_messages > 0) {

        if (backtrack > 0) {
            old_messages = 1;
        } else if ((last_event_id != NULL) || (if_modified_since >= 0)) {
            ngx_flag_t found = 0;
            while ((!message->deleted) && ((message = (ngx_http_push_stream_msg_t *) ngx_queue_next(&message->queue)) != message_sentinel)) {
                if ((!found) && (last_event_id != NULL) && (message->event_id != NULL) && (ngx_memn2cmp(message->event_id->data, last_event_id->data, message->event_id->len, last_event_id->len) == 0)) {
                    found = 1;
                    continue;
                }

                if ((!found) && (last_event_id == NULL) && (if_modified_since >= 0) && ((message->time > if_modified_since) || ((message->time == if_modified_since) && (tag >= 0) && (message->tag >= tag)))) {
                    found = 1;
                    if ((message->time == if_modified_since) && (message->tag == tag)) {
                        continue;
                    }
                }

                if (found) {
                    old_messages = 1;
                    break;
                }
            }
        }
    }
    return old_messages;
}

static void
ngx_http_push_stream_send_old_messages(ngx_http_request_t *r, ngx_http_push_stream_channel_t *channel, ngx_uint_t backtrack, time_t if_modified_since, ngx_int_t tag, time_t greater_message_time, ngx_int_t greater_message_tag, ngx_str_t *last_event_id)
{
    ngx_http_push_stream_msg_t                 *message, *message_sentinel;

    if (ngx_http_push_stream_has_old_messages_to_send(channel, backtrack, if_modified_since, tag, greater_message_time, greater_message_tag, last_event_id)) {
        message_sentinel = &channel->message_queue;
        message = message_sentinel;
        if (backtrack > 0) {
            ngx_uint_t qtd = (backtrack > channel->stored_messages) ? channel->stored_messages : backtrack;
            ngx_uint_t start = channel->stored_messages - qtd;
            // positioning at first message, and send the others
            while ((qtd > 0) && (!message->deleted) && ((message = (ngx_http_push_stream_msg_t *) ngx_queue_next(&message->queue)) != message_sentinel)) {
                if (start == 0) {
                    ngx_http_push_stream_send_response_message(r, channel, message, 0, 1);
                    qtd--;
                } else {
                    start--;
                }
            }
        } else if ((last_event_id != NULL) || (if_modified_since >= 0)) {
            ngx_flag_t found = 0;
            while ((!message->deleted) && ((message = (ngx_http_push_stream_msg_t *) ngx_queue_next(&message->queue)) != message_sentinel)) {
                if ((!found) && (last_event_id != NULL) && (message->event_id != NULL) && (ngx_memn2cmp(message->event_id->data, last_event_id->data, message->event_id->len, last_event_id->len) == 0)) {
                    found = 1;
                    continue;
                }

                if ((!found) && (last_event_id == NULL) && (if_modified_since >= 0) && ((message->time > if_modified_since) || ((message->time == if_modified_since) && (tag >= 0) && (message->tag >= tag)))) {
                    found = 1;
                    if ((message->time == if_modified_since) && (message->tag == tag)) {
                        continue;
                    }
                }

                if (found && (((greater_message_time == 0) && (greater_message_tag == -1)) || (greater_message_time > message->time) || ((greater_message_time == message->time) && (greater_message_tag >= message->tag)))) {
                    ngx_http_push_stream_send_response_message(r, channel, message, 0, 1);
                }
            }
        }
    }
}

static ngx_http_push_stream_pid_queue_t *
ngx_http_push_stream_create_worker_subscriber_channel_sentinel_locked(ngx_slab_pool_t *shpool, ngx_str_t *channel_id, ngx_log_t *log)
{
    ngx_http_push_stream_pid_queue_t     *worker_sentinel;
    ngx_http_push_stream_channel_t       *channel;

    // check if channel still exists
    if ((channel = ngx_http_push_stream_find_channel(channel_id, log)) == NULL) {
        ngx_log_error(NGX_LOG_ERR, log, 0, "push stream module: something goes very wrong, arrived on ngx_http_push_stream_subscriber_assign_channel without created channel %s", channel_id->data);
        return NULL;
    }

    if ((worker_sentinel = ngx_slab_alloc_locked(shpool, sizeof(ngx_http_push_stream_pid_queue_t))) == NULL) {
        ngx_log_error(NGX_LOG_ERR, log, 0, "push stream module: unable to allocate worker subscriber queue marker in shared memory");
        return NULL;
    }

    // initialize
    ngx_queue_insert_tail(&channel->workers_with_subscribers.queue, &worker_sentinel->queue);

    worker_sentinel->pid = ngx_pid;
    worker_sentinel->slot = ngx_process_slot;
    ngx_queue_init(&worker_sentinel->subscribers_sentinel.queue);

    return worker_sentinel;
}

static ngx_http_push_stream_subscription_t *
ngx_http_push_stream_create_channel_subscription(ngx_http_request_t *r, ngx_http_push_stream_channel_t *channel, ngx_http_push_stream_subscriber_t *subscriber)
{
    ngx_http_push_stream_subscription_t        *subscription;

    if ((subscription = ngx_palloc(r->pool, sizeof(ngx_http_push_stream_subscription_t))) == NULL) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "push stream module: unable to allocate subscribed channel reference");
        return NULL;
    }

    subscription->channel = channel;
    subscription->subscriber = subscriber;

    return subscription;
}

static ngx_int_t
ngx_http_push_stream_assing_subscription_to_channel_locked(ngx_slab_pool_t *shpool, ngx_str_t *channel_id, ngx_http_push_stream_subscription_t *subscription, ngx_http_push_stream_subscription_t *subscriptions_sentinel, ngx_log_t *log)
{
    ngx_http_push_stream_pid_queue_t           *cur, *worker_subscribers_sentinel = NULL;
    ngx_http_push_stream_channel_t             *channel;
    ngx_http_push_stream_queue_elem_t          *element_subscriber;

    // check if channel still exists
    if ((channel = ngx_http_push_stream_find_channel(channel_id, log)) == NULL) {
        ngx_log_error(NGX_LOG_ERR, log, 0, "push stream module: something goes very wrong, arrived on ngx_http_push_stream_subscriber_assign_channel without created channel %s", channel_id->data);
        return NGX_ERROR;
    }

    cur = &channel->workers_with_subscribers;
    while ((cur = (ngx_http_push_stream_pid_queue_t *) ngx_queue_next(&cur->queue)) != &channel->workers_with_subscribers) {
        if (cur->pid == ngx_pid) {
            worker_subscribers_sentinel = cur;
            break;
        }
    }

    if (worker_subscribers_sentinel == NULL) { // found nothing
        worker_subscribers_sentinel = ngx_http_push_stream_create_worker_subscriber_channel_sentinel_locked(shpool, channel_id, log);
        if (worker_subscribers_sentinel == NULL) {
            return NGX_ERROR;
        }
    }

    if ((element_subscriber = ngx_palloc(subscription->subscriber->request->pool, sizeof(ngx_http_push_stream_queue_elem_t))) == NULL) { // unable to allocate request queue element
        ngx_log_error(NGX_LOG_ERR, log, 0, "push stream module: unable to allocate subscriber reference");
        return NGX_ERROR;
    }
    element_subscriber->value = subscription->subscriber;
    subscription->channel_subscriber_element_ref = element_subscriber;

    channel->subscribers++; // do this only when we know everything went okay
    channel->last_activity_time = ngx_time();
    ngx_queue_insert_tail(&subscriptions_sentinel->queue, &subscription->queue);
    ngx_queue_insert_tail(&worker_subscribers_sentinel->subscribers_sentinel.queue, &element_subscriber->queue);
    return NGX_OK;
}


static ngx_http_push_stream_padding_t *
ngx_http_push_stream_get_padding_by_user_agent(ngx_http_request_t *r)
{
    ngx_http_push_stream_loc_conf_t                *cf = ngx_http_get_module_loc_conf(r, ngx_http_push_stream_module);
    ngx_http_push_stream_padding_t                 *padding = cf->paddings;
    ngx_str_t                                       vv_user_agent = ngx_null_string;

    if (cf->user_agent != NULL) {
        ngx_http_push_stream_complex_value(r, cf->user_agent, &vv_user_agent);
    } else if (r->headers_in.user_agent != NULL) {
        vv_user_agent = r->headers_in.user_agent->value;
    }

    if ((padding != NULL) && (vv_user_agent.len > 0)) {
        while ((padding = (ngx_http_push_stream_padding_t *) ngx_queue_next(&padding->queue)) != cf->paddings) {
            if (ngx_regex_exec(padding->agent, &vv_user_agent, NULL, 0) >= 0) {
                return padding;
            }
        }
    }

    return NULL;
}
