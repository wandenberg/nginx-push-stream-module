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
 * ngx_http_push_stream_module_subscriber.c
 *
 * Created: Oct 26, 2010
 * Authors: Wandenberg Peixoto <wandenberg@gmail.com>, Rogério Carvalho Schneider <stockrt@gmail.com>
 */

#include <ngx_http_push_stream_module_subscriber.h>

static ngx_int_t                                 ngx_http_push_stream_subscriber_assign_channel(ngx_http_push_stream_main_conf_t *mcf, ngx_http_push_stream_loc_conf_t *cf, ngx_http_request_t *r, ngx_http_push_stream_requested_channel_t *requested_channel, time_t if_modified_since, ngx_int_t tag, ngx_str_t *last_event_id, ngx_http_push_stream_subscriber_t *subscriber, ngx_pool_t *temp_pool);
static ngx_http_push_stream_subscriber_t        *ngx_http_push_stream_subscriber_prepare_request_to_keep_connected(ngx_http_request_t *r);
static ngx_int_t                                 ngx_http_push_stream_registry_subscriber(ngx_http_request_t *r, ngx_http_push_stream_subscriber_t *worker_subscriber);
static ngx_flag_t                                ngx_http_push_stream_has_old_messages_to_send(ngx_http_push_stream_channel_t *channel, ngx_uint_t backtrack, time_t if_modified_since, ngx_int_t tag, time_t greater_message_time, ngx_int_t greater_message_tag, ngx_str_t *last_event_id);
static void                                      ngx_http_push_stream_send_old_messages(ngx_http_request_t *r, ngx_http_push_stream_channel_t *channel, ngx_uint_t backtrack, time_t if_modified_since, ngx_int_t tag, time_t greater_message_time, ngx_int_t greater_message_tag, ngx_str_t *last_event_id);
static ngx_http_push_stream_pid_queue_t         *ngx_http_push_stream_get_worker_subscriber_channel_sentinel_locked(ngx_slab_pool_t *shpool, ngx_http_push_stream_channel_t *channel, ngx_log_t *log);
static ngx_http_push_stream_subscription_t      *ngx_http_push_stream_create_channel_subscription(ngx_http_request_t *r, ngx_http_push_stream_channel_t *channel, ngx_http_push_stream_subscriber_t *subscriber);
static ngx_int_t                                 ngx_http_push_stream_assing_subscription_to_channel(ngx_slab_pool_t *shpool, ngx_http_push_stream_channel_t *channel, ngx_http_push_stream_subscription_t *subscription, ngx_queue_t *subscriptions, ngx_log_t *log);
static ngx_int_t                                 ngx_http_push_stream_subscriber_polling_handler(ngx_http_request_t *r, ngx_http_push_stream_requested_channel_t *channels_ids, time_t if_modified_since, ngx_int_t tag, ngx_str_t *last_event_id, ngx_flag_t longpolling, ngx_pool_t *temp_pool);
static ngx_http_push_stream_padding_t           *ngx_http_push_stream_get_padding_by_user_agent(ngx_http_request_t *r);
void                                             ngx_http_push_stream_websocket_reading(ngx_http_request_t *r);

static ngx_int_t
ngx_http_push_stream_subscriber_handler(ngx_http_request_t *r)
{
    ngx_http_push_stream_main_conf_t               *mcf = ngx_http_get_module_main_conf(r, ngx_http_push_stream_module);
    ngx_http_push_stream_loc_conf_t                *cf = ngx_http_get_module_loc_conf(r, ngx_http_push_stream_module);
    ngx_http_push_stream_subscriber_t              *worker_subscriber;
    ngx_http_push_stream_requested_channel_t       *requested_channels, *requested_channel;
    ngx_queue_t                                    *q;
    ngx_http_push_stream_module_ctx_t              *ctx;
    ngx_int_t                                       tag;
    time_t                                          if_modified_since;
    ngx_str_t                                      *last_event_id = NULL;
    ngx_str_t                                      *push_mode;
    ngx_flag_t                                      polling, longpolling;
    ngx_int_t                                       status_code;
    ngx_str_t                                      *explain_error_message;
    ngx_str_t                                       vv_allowed_origins = ngx_null_string;

    // add headers to support cross domain requests
    if (cf->allowed_origins != NULL) {
        ngx_http_push_stream_complex_value(r, cf->allowed_origins, &vv_allowed_origins);
    }

    if (vv_allowed_origins.len > 0) {
        ngx_http_push_stream_add_response_header(r, &NGX_HTTP_PUSH_STREAM_HEADER_ACCESS_CONTROL_ALLOW_ORIGIN, &vv_allowed_origins);
        ngx_http_push_stream_add_response_header(r, &NGX_HTTP_PUSH_STREAM_HEADER_ACCESS_CONTROL_ALLOW_METHODS, &NGX_HTTP_PUSH_STREAM_ALLOW_GET);
        ngx_http_push_stream_add_response_header(r, &NGX_HTTP_PUSH_STREAM_HEADER_ACCESS_CONTROL_ALLOW_HEADERS, &NGX_HTTP_PUSH_STREAM_ALLOWED_HEADERS);
    }

    if (r->method & NGX_HTTP_OPTIONS) {
        return ngx_http_push_stream_send_only_header_response(r, NGX_HTTP_OK, NULL);
    }

    ngx_http_push_stream_set_expires(r, NGX_HTTP_PUSH_STREAM_EXPIRES_EPOCH, 0);

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
    requested_channels = ngx_http_push_stream_parse_channels_ids_from_path(r, r->pool);
    if ((requested_channels == NULL) || ngx_queue_empty(&requested_channels->queue)) {
        ngx_log_error(NGX_LOG_WARN, r->connection->log, 0, "push stream module: the push_stream_channels_path is required but is not set");
        return ngx_http_push_stream_send_only_header_response(r, NGX_HTTP_BAD_REQUEST, &NGX_HTTP_PUSH_STREAM_NO_CHANNEL_ID_MESSAGE);
    }

    //validate channels: name, length and quantity. check if channel exists when authorized_channels_only is on. check if channel is full of subscribers
    if (ngx_http_push_stream_validate_channels(r, requested_channels, &status_code, &explain_error_message) == NGX_ERROR) {
        return ngx_http_push_stream_send_only_header_response(r, status_code, explain_error_message);
    }

    // get control values
    ngx_http_push_stream_get_last_received_message_values(r, &if_modified_since, &tag, &last_event_id);

    push_mode = ngx_http_push_stream_get_header(r, &NGX_HTTP_PUSH_STREAM_HEADER_MODE);
    polling = ((cf->location_type == NGX_HTTP_PUSH_STREAM_SUBSCRIBER_MODE_POLLING) || ((push_mode != NULL) && (push_mode->len == NGX_HTTP_PUSH_STREAM_MODE_POLLING.len) && (ngx_strncasecmp(push_mode->data, NGX_HTTP_PUSH_STREAM_MODE_POLLING.data, NGX_HTTP_PUSH_STREAM_MODE_POLLING.len) == 0)));
    longpolling = ((cf->location_type == NGX_HTTP_PUSH_STREAM_SUBSCRIBER_MODE_LONGPOLLING) || ((push_mode != NULL) && (push_mode->len == NGX_HTTP_PUSH_STREAM_MODE_LONGPOLLING.len) && (ngx_strncasecmp(push_mode->data, NGX_HTTP_PUSH_STREAM_MODE_LONGPOLLING.data, NGX_HTTP_PUSH_STREAM_MODE_LONGPOLLING.len) == 0)));

    if (polling || longpolling) {
        ngx_int_t result = ngx_http_push_stream_subscriber_polling_handler(r, requested_channels, if_modified_since, tag, last_event_id, longpolling, ctx->temp_pool);
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

    ngx_http_send_header(r);

    // sending response content header
    if (ngx_http_push_stream_send_response_content_header(r, cf) == NGX_ERROR) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "push stream module: could not send content header to subscriber");
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    if (ngx_http_push_stream_registry_subscriber(r, worker_subscriber) == NGX_ERROR) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    // adding subscriber to channel(s) and send old messages
    for (q = ngx_queue_head(&requested_channels->queue); q != ngx_queue_sentinel(&requested_channels->queue); q = ngx_queue_next(q)) {
        requested_channel = ngx_queue_data(q, ngx_http_push_stream_requested_channel_t, queue);

        if (ngx_http_push_stream_subscriber_assign_channel(mcf, cf, r, requested_channel, if_modified_since, tag, last_event_id, worker_subscriber, ctx->temp_pool) != NGX_OK) {
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
ngx_http_push_stream_subscriber_polling_handler(ngx_http_request_t *r, ngx_http_push_stream_requested_channel_t *requested_channels, time_t if_modified_since, ngx_int_t tag, ngx_str_t *last_event_id, ngx_flag_t longpolling, ngx_pool_t *temp_pool)
{
    ngx_http_push_stream_main_conf_t               *mcf = ngx_http_get_module_main_conf(r, ngx_http_push_stream_module);
    ngx_http_push_stream_loc_conf_t                *cf = ngx_http_get_module_loc_conf(r, ngx_http_push_stream_module);
    ngx_slab_pool_t                                *shpool = mcf->shpool;
    ngx_http_push_stream_module_ctx_t              *ctx = ngx_http_get_module_ctx(r, ngx_http_push_stream_module);
    ngx_http_push_stream_requested_channel_t       *requested_channel;
    ngx_queue_t                                    *q;
    ngx_http_push_stream_subscriber_t              *worker_subscriber;
    ngx_http_push_stream_subscription_t            *subscription;
    time_t                                          greater_message_time;
    ngx_int_t                                       greater_message_tag;
    ngx_flag_t                                      has_message_to_send = 0;
    ngx_str_t                                       callback_function_name;

    if (ngx_http_arg(r, NGX_HTTP_PUSH_STREAM_CALLBACK.data, NGX_HTTP_PUSH_STREAM_CALLBACK.len, &callback_function_name) == NGX_OK) {
        ngx_http_push_stream_unescape_uri(&callback_function_name);
        if ((ctx->callback = ngx_pcalloc(r->pool, sizeof(ngx_str_t))) == NULL) {
            ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "push stream module: unable to allocate memory for callback function name");
            return NGX_HTTP_INTERNAL_SERVER_ERROR;
        }
        ctx->callback->data = callback_function_name.data;
        ctx->callback->len = callback_function_name.len;
    }

    greater_message_tag = tag;
    greater_message_time = (if_modified_since < 0) ? 0 : if_modified_since;

    // check if has any message to send
    for (q = ngx_queue_head(&requested_channels->queue); q != ngx_queue_sentinel(&requested_channels->queue); q = ngx_queue_next(q)) {
        requested_channel = ngx_queue_data(q, ngx_http_push_stream_requested_channel_t, queue);

        if (ngx_http_push_stream_has_old_messages_to_send(requested_channel->channel, requested_channel->backtrack_messages, if_modified_since, tag, greater_message_time, greater_message_tag, last_event_id)) {
            has_message_to_send = 1;
            if (requested_channel->channel->last_message_time > greater_message_time) {
                greater_message_time = requested_channel->channel->last_message_time;
                greater_message_tag = requested_channel->channel->last_message_tag;
            } else {
                if ((requested_channel->channel->last_message_time == greater_message_time) && (requested_channel->channel->last_message_tag > greater_message_tag) ) {
                    greater_message_tag = requested_channel->channel->last_message_tag;
                }
            }
        }
    }


    if (longpolling && !has_message_to_send) {
        // long polling mode without messages
        if ((worker_subscriber = ngx_http_push_stream_subscriber_prepare_request_to_keep_connected(r)) == NULL) {
            return NGX_HTTP_INTERNAL_SERVER_ERROR;
        }
        worker_subscriber->longpolling = 1;

        if (ngx_http_push_stream_registry_subscriber(r, worker_subscriber) == NGX_ERROR) {
            return NGX_HTTP_INTERNAL_SERVER_ERROR;
        }

        // adding subscriber to channel(s)
        for (q = ngx_queue_head(&requested_channels->queue); q != ngx_queue_sentinel(&requested_channels->queue); q = ngx_queue_next(q)) {
            requested_channel = ngx_queue_data(q, ngx_http_push_stream_requested_channel_t, queue);

            if ((subscription = ngx_http_push_stream_create_channel_subscription(r, requested_channel->channel, worker_subscriber)) == NULL) {
                return NGX_HTTP_INTERNAL_SERVER_ERROR;
            }

            ngx_http_push_stream_assing_subscription_to_channel(shpool, requested_channel->channel, subscription, &worker_subscriber->subscriptions, r->connection->log);
        }

        return NGX_DONE;
    }

    // polling or long polling with messages to send

    ngx_http_push_stream_add_polling_headers(r, greater_message_time, greater_message_tag, temp_pool);

    if (!has_message_to_send) {
        // polling subscriber requests get a 304 with their entity tags preserved if don't have new messages.
        return ngx_http_push_stream_send_only_header_response(r, NGX_HTTP_NOT_MODIFIED, NULL);
    }

    // polling with messages or long polling without messages to send
    r->headers_out.status = NGX_HTTP_OK;
    r->headers_out.content_length_n = -1;

    ngx_http_send_header(r);

    // sending response content header
    if (ngx_http_push_stream_send_response_content_header(r, cf) == NGX_ERROR) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "push stream module: could not send content header to subscriber");
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    if (ctx->callback != NULL) {
        ngx_http_push_stream_send_response_text(r, ctx->callback->data, ctx->callback->len, 0);
        ngx_http_push_stream_send_response_text(r, NGX_HTTP_PUSH_STREAM_CALLBACK_INIT_CHUNK.data, NGX_HTTP_PUSH_STREAM_CALLBACK_INIT_CHUNK.len, 0);
    }

    for (q = ngx_queue_head(&requested_channels->queue); q != ngx_queue_sentinel(&requested_channels->queue); q = ngx_queue_next(q)) {
        requested_channel = ngx_queue_data(q, ngx_http_push_stream_requested_channel_t, queue);
        ngx_http_push_stream_send_old_messages(r, requested_channel->channel, requested_channel->backtrack_messages, if_modified_since, tag, greater_message_time, greater_message_tag, last_event_id);
    }

    if (ctx->callback != NULL) {
        ngx_http_push_stream_send_response_text(r, NGX_HTTP_PUSH_STREAM_CALLBACK_END_CHUNK.data, NGX_HTTP_PUSH_STREAM_CALLBACK_END_CHUNK.len, 0);
    }

    if (cf->footer_template.len > 0) {
        ngx_http_push_stream_send_response_text(r, cf->footer_template.data, cf->footer_template.len, 0);
    }

    ngx_http_send_special(r, NGX_HTTP_LAST | NGX_HTTP_FLUSH);

    return NGX_OK;
}

static ngx_int_t
ngx_http_push_stream_subscriber_assign_channel(ngx_http_push_stream_main_conf_t *mcf, ngx_http_push_stream_loc_conf_t *cf, ngx_http_request_t *r, ngx_http_push_stream_requested_channel_t *requested_channel, time_t if_modified_since, ngx_int_t tag, ngx_str_t *last_event_id, ngx_http_push_stream_subscriber_t *subscriber, ngx_pool_t *temp_pool)
{
    ngx_http_push_stream_subscription_t        *subscription;
    ngx_slab_pool_t                            *shpool = mcf->shpool;

    if ((subscription = ngx_http_push_stream_create_channel_subscription(r, requested_channel->channel, subscriber)) == NULL) {
        return NGX_ERROR;
    }

    // send old messages to new subscriber
    ngx_http_push_stream_send_old_messages(r, requested_channel->channel, requested_channel->backtrack_messages, if_modified_since, tag, 0, -1, last_event_id);

    return ngx_http_push_stream_assing_subscription_to_channel(shpool, requested_channel->channel, subscription, &subscriber->subscriptions, r->connection->log);
}


static ngx_int_t
ngx_http_push_stream_validate_channels(ngx_http_request_t *r, ngx_http_push_stream_requested_channel_t *requested_channels, ngx_int_t *status_code, ngx_str_t **explain_error_message)
{
    ngx_http_push_stream_main_conf_t               *mcf = ngx_http_get_module_main_conf(r, ngx_http_push_stream_module);
    ngx_http_push_stream_loc_conf_t                *cf = ngx_http_get_module_loc_conf(r, ngx_http_push_stream_module);
    ngx_http_push_stream_requested_channel_t       *requested_channel;
    ngx_queue_t                                    *q;
    ngx_uint_t                                      subscribed_channels_qtd = 0;
    ngx_uint_t                                      subscribed_wildcard_channels_qtd = 0;
    ngx_flag_t                                      is_wildcard_channel;

    for (q = ngx_queue_head(&requested_channels->queue); q != ngx_queue_sentinel(&requested_channels->queue); q = ngx_queue_next(q)) {
        requested_channel = ngx_queue_data(q, ngx_http_push_stream_requested_channel_t, queue);
        // could not be ALL channel or contain wildcard
        if ((ngx_memn2cmp(requested_channel->id->data, NGX_HTTP_PUSH_STREAM_ALL_CHANNELS_INFO_ID.data, requested_channel->id->len, NGX_HTTP_PUSH_STREAM_ALL_CHANNELS_INFO_ID.len) == 0) || (ngx_strchr(requested_channel->id->data, '*') != NULL)) {
            *status_code = NGX_HTTP_FORBIDDEN;
            *explain_error_message = (ngx_str_t *) &NGX_HTTP_PUSH_STREAM_CHANNEL_ID_NOT_AUTHORIZED_MESSAGE;
            return NGX_ERROR;
        }

        // could not have a large size
        if ((mcf->max_channel_id_length != NGX_CONF_UNSET_UINT) && (requested_channel->id->len > mcf->max_channel_id_length)) {
            ngx_log_error(NGX_LOG_WARN, r->connection->log, 0, "push stream module: channel id is larger than allowed %d", requested_channel->id->len);
            *status_code = NGX_HTTP_BAD_REQUEST;
            *explain_error_message = (ngx_str_t *) &NGX_HTTP_PUSH_STREAM_TOO_LARGE_CHANNEL_ID_MESSAGE;
            return NGX_ERROR;
        }

        // count subscribed normal and wildcard channels
        subscribed_channels_qtd++;
        is_wildcard_channel = 0;
        if ((mcf->wildcard_channel_prefix.len > 0) && (ngx_strncmp(requested_channel->id->data, mcf->wildcard_channel_prefix.data, mcf->wildcard_channel_prefix.len) == 0)) {
            is_wildcard_channel = 1;
            subscribed_wildcard_channels_qtd++;
        }

        requested_channel->channel = ngx_http_push_stream_find_channel(requested_channel->id, r->connection->log, mcf);

        // check if channel exists when authorized_channels_only is on
        if (cf->authorized_channels_only && !is_wildcard_channel && ((requested_channel->channel == NULL) || (requested_channel->channel->stored_messages == 0))) {
            *status_code = NGX_HTTP_FORBIDDEN;
            *explain_error_message = (ngx_str_t *) &NGX_HTTP_PUSH_STREAM_CANNOT_CREATE_CHANNELS;
            return NGX_ERROR;
        }

        // check if channel is full of subscribers
        if ((mcf->max_subscribers_per_channel != NGX_CONF_UNSET_UINT) && ((requested_channel->channel != NULL) && (requested_channel->channel->subscribers >= mcf->max_subscribers_per_channel))) {
            *status_code = NGX_HTTP_FORBIDDEN;
            *explain_error_message = (ngx_str_t *) &NGX_HTTP_PUSH_STREAM_TOO_SUBSCRIBERS_PER_CHANNEL;
            return NGX_ERROR;
        }

        // check if is allowed to connect to events channel
        if (!cf->allow_connections_to_events_channel && (requested_channel->channel != NULL) && requested_channel->channel->for_events) {
            ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "push stream module: subscription to events channel is not allowed");
            *status_code = NGX_HTTP_FORBIDDEN;
            *explain_error_message = (ngx_str_t *) &NGX_HTTP_PUSH_STREAM_SUBSCRIPTION_EVENTS_CHANNEL_FORBIDDEN_MESSAGE;
            return NGX_ERROR;
        }
    }

    // check if number of subscribed wildcard channels is acceptable
    if ((cf->wildcard_channel_max_qtd != NGX_CONF_UNSET_UINT) && (subscribed_wildcard_channels_qtd > 0) && ((subscribed_wildcard_channels_qtd > cf->wildcard_channel_max_qtd) || (subscribed_wildcard_channels_qtd == subscribed_channels_qtd))) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "push stream module: max subscribed wildcard channels exceeded");
        *status_code = NGX_HTTP_FORBIDDEN;
        *explain_error_message = (ngx_str_t *) &NGX_HTTP_PUSH_STREAM_TOO_MUCH_WILDCARD_CHANNELS;
        return NGX_ERROR;
    }

    // create the channels in advance, if doesn't exist, to ensure max number of channels in the server
    for (q = ngx_queue_head(&requested_channels->queue); q != ngx_queue_sentinel(&requested_channels->queue); q = ngx_queue_next(q)) {
        requested_channel = ngx_queue_data(q, ngx_http_push_stream_requested_channel_t, queue);
        if (requested_channel->channel != NULL) {
            continue;
        }

        requested_channel->channel = ngx_http_push_stream_get_channel(requested_channel->id, r->connection->log, mcf);
        if (requested_channel->channel == NULL) {
            ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "push stream module: unable to allocate memory for new channel");
            *status_code = NGX_HTTP_INTERNAL_SERVER_ERROR;
            *explain_error_message = (ngx_str_t *) &NGX_HTTP_PUSH_STREAM_EMPTY;
            return NGX_ERROR;
        }

        if (requested_channel->channel == NGX_HTTP_PUSH_STREAM_NUMBER_OF_CHANNELS_EXCEEDED) {
            ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "push stream module: number of channels were exceeded");
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
    ngx_http_push_stream_module_ctx_t              *ctx = ngx_http_get_module_ctx(r, ngx_http_push_stream_module);
    ngx_http_push_stream_subscriber_t              *worker_subscriber;

    if ((worker_subscriber = ngx_pcalloc(r->pool, sizeof(ngx_http_push_stream_subscriber_t))) == NULL) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "push stream module: unable to allocate worker subscriber");
        return NULL;
    }

    worker_subscriber->longpolling = 0;
    worker_subscriber->request = r;
    worker_subscriber->worker_subscribed_pid = ngx_pid;
    ngx_queue_init(&worker_subscriber->worker_queue);
    ngx_queue_init(&worker_subscriber->subscriptions);
    ctx->subscriber = worker_subscriber;

    // increment request reference count to keep connection open
    r->main->count++;

    // responding subscriber
    r->read_event_handler = (cf->location_type == NGX_HTTP_PUSH_STREAM_SUBSCRIBER_MODE_WEBSOCKET) ? ngx_http_push_stream_websocket_reading : ngx_http_test_reading;
    r->write_event_handler = ngx_http_request_empty_handler;

    if (cf->location_type == NGX_HTTP_PUSH_STREAM_SUBSCRIBER_MODE_EVENTSOURCE) {
        r->headers_out.content_type_len = NGX_HTTP_PUSH_STREAM_EVENTSOURCE_CONTENT_TYPE.len;
        r->headers_out.content_type = NGX_HTTP_PUSH_STREAM_EVENTSOURCE_CONTENT_TYPE;
    } else {
        ngx_http_set_content_type(r);
    }

    r->headers_out.status = NGX_HTTP_OK;
    r->headers_out.content_length_n = -1;

    return worker_subscriber;
}

static ngx_int_t
ngx_http_push_stream_registry_subscriber(ngx_http_request_t *r, ngx_http_push_stream_subscriber_t *worker_subscriber)
{
    ngx_http_push_stream_main_conf_t               *mcf = ngx_http_get_module_main_conf(r, ngx_http_push_stream_module);
    ngx_http_push_stream_loc_conf_t                *cf = ngx_http_get_module_loc_conf(r, ngx_http_push_stream_module);
    ngx_http_push_stream_shm_data_t                *data = mcf->shm_data;
    ngx_http_push_stream_worker_data_t             *thisworker_data = &data->ipc[ngx_process_slot];
    ngx_msec_t                                      connection_ttl = worker_subscriber->longpolling ? cf->longpolling_connection_ttl : cf->subscriber_connection_ttl;
    ngx_http_push_stream_module_ctx_t              *ctx = ngx_http_get_module_ctx(r, ngx_http_push_stream_module);
    ngx_slab_pool_t                                *shpool = mcf->shpool;

    // adding subscriber to worker list of subscribers
    ngx_queue_insert_tail(&thisworker_data->subscribers_queue, &worker_subscriber->worker_queue);

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
    ngx_shmtx_lock(&shpool->mutex);
    data->subscribers++;
    ngx_shmtx_unlock(&shpool->mutex);
    thisworker_data->subscribers++;

    return NGX_OK;
}

static ngx_flag_t
ngx_http_push_stream_has_old_messages_to_send(ngx_http_push_stream_channel_t *channel, ngx_uint_t backtrack, time_t if_modified_since, ngx_int_t tag, time_t greater_message_time, ngx_int_t greater_message_tag, ngx_str_t *last_event_id)
{
    ngx_flag_t old_messages = 0;
    ngx_http_push_stream_msg_t *message;
    ngx_queue_t                *q;

    if (channel->stored_messages > 0) {

        if (backtrack > 0) {
            old_messages = 1;
        } else if ((last_event_id != NULL) || (if_modified_since >= 0)) {
            ngx_flag_t found = 0;
            ngx_shmtx_lock(channel->mutex);
            for (q = ngx_queue_head(&channel->message_queue); q != ngx_queue_sentinel(&channel->message_queue); q = ngx_queue_next(q)) {
                message = ngx_queue_data(q, ngx_http_push_stream_msg_t, queue);
                if (message->deleted) {
                    break;
                }

                if ((!found) && (last_event_id != NULL) && (message->event_id != NULL) && (ngx_memn2cmp(message->event_id->data, last_event_id->data, message->event_id->len, last_event_id->len) == 0)) {
                    found = 1;
                    continue;
                }

                if ((!found) && (if_modified_since >= 0) && ((message->time > if_modified_since) || ((message->time == if_modified_since) && (tag >= 0) && (message->tag >= tag)))) {
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
            ngx_shmtx_unlock(channel->mutex);
        }
    }
    return old_messages;
}

static void
ngx_http_push_stream_send_old_messages(ngx_http_request_t *r, ngx_http_push_stream_channel_t *channel, ngx_uint_t backtrack, time_t if_modified_since, ngx_int_t tag, time_t greater_message_time, ngx_int_t greater_message_tag, ngx_str_t *last_event_id)
{
    ngx_http_push_stream_module_ctx_t     *ctx = ngx_http_get_module_ctx(r, ngx_http_push_stream_module);
    ngx_http_push_stream_msg_t            *message;
    ngx_queue_t                           *q;

    if (ngx_http_push_stream_has_old_messages_to_send(channel, backtrack, if_modified_since, tag, greater_message_time, greater_message_tag, last_event_id)) {
        if (backtrack > 0) {
            ngx_uint_t qtd = (backtrack > channel->stored_messages) ? channel->stored_messages : backtrack;
            ngx_uint_t start = channel->stored_messages - qtd;
            ngx_shmtx_lock(channel->mutex);
            // positioning at first message, and send the others
            for (q = ngx_queue_head(&channel->message_queue); (qtd > 0) && q != ngx_queue_sentinel(&channel->message_queue); q = ngx_queue_next(q)) {
                message = ngx_queue_data(q, ngx_http_push_stream_msg_t, queue);
                if (message->deleted) {
                    break;
                }

                if (start == 0) {
                    qtd--;
                    ngx_http_push_stream_send_response_message(r, channel, message, 0, ctx->message_sent);
                } else {
                    start--;
                }
            }
            ngx_shmtx_unlock(channel->mutex);
        } else if ((last_event_id != NULL) || (if_modified_since >= 0)) {
            ngx_flag_t found = 0;
            ngx_shmtx_lock(channel->mutex);
            for (q = ngx_queue_head(&channel->message_queue); q != ngx_queue_sentinel(&channel->message_queue); q = ngx_queue_next(q)) {
                message = ngx_queue_data(q, ngx_http_push_stream_msg_t, queue);
                if (message->deleted) {
                    break;
                }

                if ((!found) && (last_event_id != NULL) && (message->event_id != NULL) && (ngx_memn2cmp(message->event_id->data, last_event_id->data, message->event_id->len, last_event_id->len) == 0)) {
                    found = 1;
                    continue;
                }

                if ((!found) && (if_modified_since >= 0) && ((message->time > if_modified_since) || ((message->time == if_modified_since) && (tag >= 0) && (message->tag >= tag)))) {
                    found = 1;
                    if ((message->time == if_modified_since) && (message->tag == tag)) {
                        continue;
                    }
                }

                if (found && (((greater_message_time == 0) && (greater_message_tag == -1)) || (greater_message_time > message->time) || ((greater_message_time == message->time) && (greater_message_tag >= message->tag)))) {
                    ngx_http_push_stream_send_response_message(r, channel, message, 0, ctx->message_sent);
                }
            }
            ngx_shmtx_unlock(channel->mutex);
        }
    }
}

static ngx_http_push_stream_pid_queue_t *
ngx_http_push_stream_get_worker_subscriber_channel_sentinel_locked(ngx_slab_pool_t *shpool, ngx_http_push_stream_channel_t *channel, ngx_log_t *log)
{
    ngx_http_push_stream_pid_queue_t     *worker_sentinel;
    ngx_queue_t                          *q;

    for (q = ngx_queue_head(&channel->workers_with_subscribers); q != ngx_queue_sentinel(&channel->workers_with_subscribers); q = ngx_queue_next(q)) {
        worker_sentinel = ngx_queue_data(q, ngx_http_push_stream_pid_queue_t, queue);
        if (worker_sentinel->pid == ngx_pid) {
            return worker_sentinel;
        }
    }

    if ((worker_sentinel = ngx_slab_alloc(shpool, sizeof(ngx_http_push_stream_pid_queue_t))) == NULL) {
        ngx_log_error(NGX_LOG_ERR, log, 0, "push stream module: unable to allocate worker subscriber queue marker in shared memory");
        return NULL;
    }

    // initialize
    ngx_queue_insert_tail(&channel->workers_with_subscribers, &worker_sentinel->queue);

    worker_sentinel->subscribers = 0;
    worker_sentinel->pid = ngx_pid;
    worker_sentinel->slot = ngx_process_slot;
    ngx_queue_init(&worker_sentinel->subscriptions);

    return worker_sentinel;
}

static ngx_http_push_stream_subscription_t *
ngx_http_push_stream_create_channel_subscription(ngx_http_request_t *r, ngx_http_push_stream_channel_t *channel, ngx_http_push_stream_subscriber_t *subscriber)
{
    ngx_http_push_stream_subscription_t        *subscription;

    if ((subscription = ngx_pcalloc(r->pool, sizeof(ngx_http_push_stream_subscription_t))) == NULL) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "push stream module: unable to allocate subscribed channel reference");
        return NULL;
    }

    subscription->channel_worker_sentinel = NULL;
    subscription->channel = channel;
    subscription->subscriber = subscriber;
    ngx_queue_init(&subscription->queue);
    ngx_queue_init(&subscription->channel_worker_queue);

    return subscription;
}

static ngx_int_t
ngx_http_push_stream_assing_subscription_to_channel(ngx_slab_pool_t *shpool, ngx_http_push_stream_channel_t *channel, ngx_http_push_stream_subscription_t *subscription, ngx_queue_t *subscriptions, ngx_log_t *log)
{
    ngx_http_push_stream_main_conf_t           *mcf = ngx_http_get_module_main_conf(subscription->subscriber->request, ngx_http_push_stream_module);
    ngx_http_push_stream_pid_queue_t           *worker_subscribers_sentinel;

    ngx_shmtx_lock(channel->mutex);
    if ((worker_subscribers_sentinel = ngx_http_push_stream_get_worker_subscriber_channel_sentinel_locked(shpool, channel, log)) == NULL) {
        ngx_shmtx_unlock(channel->mutex);
        return NGX_ERROR;
    }

    channel->subscribers++; // do this only when we know everything went okay
    worker_subscribers_sentinel->subscribers++;
    channel->expires = ngx_time() + mcf->channel_inactivity_time;
    ngx_queue_insert_tail(subscriptions, &subscription->queue);
    ngx_queue_insert_tail(&worker_subscribers_sentinel->subscriptions, &subscription->channel_worker_queue);
    subscription->channel_worker_sentinel = worker_subscribers_sentinel;
    ngx_shmtx_unlock(channel->mutex);

    ngx_http_push_stream_send_event(mcf, log, channel, &NGX_HTTP_PUSH_STREAM_EVENT_TYPE_CLIENT_SUBSCRIBED, NULL);

    return NGX_OK;
}


static ngx_http_push_stream_padding_t *
ngx_http_push_stream_get_padding_by_user_agent(ngx_http_request_t *r)
{
    ngx_http_push_stream_loc_conf_t                *cf = ngx_http_get_module_loc_conf(r, ngx_http_push_stream_module);
    ngx_queue_t                                    *q;
    ngx_str_t                                       vv_user_agent = ngx_null_string;

    if (cf->user_agent != NULL) {
        ngx_http_push_stream_complex_value(r, cf->user_agent, &vv_user_agent);
    } else if (r->headers_in.user_agent != NULL) {
        vv_user_agent = r->headers_in.user_agent->value;
    }

    if ((cf->paddings != NULL) && (vv_user_agent.len > 0)) {
        for (q = ngx_queue_head(cf->paddings); q != ngx_queue_sentinel(cf->paddings); q = ngx_queue_next(q)) {
            ngx_http_push_stream_padding_t *padding = ngx_queue_data(q, ngx_http_push_stream_padding_t, queue);
            if (ngx_regex_exec(padding->agent, &vv_user_agent, NULL, 0) >= 0) {
                return padding;
            }
        }
    }

    return NULL;
}
