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
 * ngx_http_push_stream_module_websocket.c
 *
 * Created: Oct 20, 2011
 * Authors: Wandenberg Peixoto <wandenberg@gmail.com>, Rogério Carvalho Schneider <stockrt@gmail.com>
 */

#include <ngx_http_push_stream_module_websocket.h>

ngx_str_t *ngx_http_push_stream_generate_websocket_accept_value(ngx_http_request_t *r, ngx_str_t *sec_key, ngx_pool_t *temp_pool);
ngx_int_t  ngx_http_push_stream_recv(ngx_connection_t *c, ngx_event_t *rev, ngx_buf_t *buf, ssize_t len);
void       ngx_http_push_stream_set_buffer(ngx_buf_t *buf, u_char *start, u_char *last, ssize_t len);

static ngx_int_t
ngx_http_push_stream_websocket_handler(ngx_http_request_t *r)
{
#if !(NGX_HAVE_SHA1)
    ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "push stream module: sha1 support is needed to use WebSocket");
    return NGX_OK;
#endif
    ngx_http_push_stream_main_conf_t               *mcf = ngx_http_get_module_main_conf(r, ngx_http_push_stream_module);
    ngx_http_push_stream_loc_conf_t                *cf = ngx_http_get_module_loc_conf(r, ngx_http_push_stream_module);
    ngx_http_push_stream_subscriber_t              *worker_subscriber;
    ngx_http_push_stream_requested_channel_t       *requested_channels, *requested_channel;
    ngx_queue_t                                    *q;
    ngx_http_push_stream_module_ctx_t              *ctx;
    ngx_int_t                                       tag;
    time_t                                          if_modified_since;
    ngx_str_t                                      *last_event_id = NULL;
    ngx_int_t                                       status_code;
    ngx_str_t                                      *explain_error_message;
    ngx_str_t                                      *upgrade_header, *connection_header, *sec_key_header, *sec_version_header, *sec_accept_header;
    ngx_int_t                                       version;

    // WebSocket connections must not use keepalive
    r->keepalive = 0;

    // only accept GET method
    if (!(r->method & NGX_HTTP_GET)) {
        ngx_http_push_stream_add_response_header(r, &NGX_HTTP_PUSH_STREAM_HEADER_ALLOW, &NGX_HTTP_PUSH_STREAM_ALLOW_GET);
        return ngx_http_push_stream_send_only_header_response(r, NGX_HTTP_NOT_ALLOWED, NULL);
    }

    ngx_http_push_stream_set_expires(r, NGX_HTTP_PUSH_STREAM_EXPIRES_EPOCH, 0);

    upgrade_header = ngx_http_push_stream_get_header(r, &NGX_HTTP_PUSH_STREAM_HEADER_UPGRADE);
    connection_header = ngx_http_push_stream_get_header(r, &NGX_HTTP_PUSH_STREAM_HEADER_CONNECTION);
    sec_key_header = ngx_http_push_stream_get_header(r, &NGX_HTTP_PUSH_STREAM_HEADER_SEC_WEBSOCKET_KEY);
    sec_version_header = ngx_http_push_stream_get_header(r, &NGX_HTTP_PUSH_STREAM_HEADER_SEC_WEBSOCKET_VERSION);

    if ((upgrade_header == NULL) || (connection_header == NULL) || (sec_key_header == NULL) || (sec_version_header == NULL)) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "push stream module: %V", &NGX_HTTP_PUSH_STREAM_NO_MANDATORY_HEADERS_MESSAGE);
        return ngx_http_push_stream_send_only_header_response(r, NGX_HTTP_BAD_REQUEST, &NGX_HTTP_PUSH_STREAM_NO_MANDATORY_HEADERS_MESSAGE);
    }

    version = ngx_atoi(sec_version_header->data, sec_version_header->len);
    if ((version != NGX_HTTP_PUSH_STREAM_WEBSOCKET_VERSION_8) && (version != NGX_HTTP_PUSH_STREAM_WEBSOCKET_VERSION_13)) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "push stream module: version: %d %V", version, &NGX_HTTP_PUSH_STREAM_WRONG_WEBSOCKET_VERSION_MESSAGE);
        ngx_http_push_stream_add_response_header(r, &NGX_HTTP_PUSH_STREAM_HEADER_SEC_WEBSOCKET_VERSION, &NGX_HTTP_PUSH_STREAM_WEBSOCKET_SUPPORTED_VERSIONS);
        return ngx_http_push_stream_send_only_header_response(r, NGX_HTTP_BAD_REQUEST, &NGX_HTTP_PUSH_STREAM_WRONG_WEBSOCKET_VERSION_MESSAGE);
    }

    if ((ctx = ngx_http_push_stream_add_request_context(r)) == NULL) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "push stream module: unable to create request context");
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    if ((ctx->frame = ngx_pcalloc(r->pool, sizeof(ngx_http_push_stream_frame_t))) == NULL) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "push stream module: unable to create frame structure");
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }
    ctx->frame->step = NGX_HTTP_PUSH_STREAM_WEBSOCKET_READ_START_STEP;
    ctx->frame->last = NULL;
    ctx->frame->payload = NULL;

    if ((sec_accept_header = ngx_http_push_stream_generate_websocket_accept_value(r, sec_key_header, ctx->temp_pool)) == NULL) {
        ngx_log_error(NGX_LOG_WARN, r->connection->log, 0, "push stream module: could not generate security accept header value");
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    ngx_http_push_stream_add_response_header(r, &NGX_HTTP_PUSH_STREAM_HEADER_UPGRADE, &NGX_HTTP_PUSH_STREAM_WEBSOCKET_UPGRADE);
    ngx_http_push_stream_add_response_header(r, &NGX_HTTP_PUSH_STREAM_HEADER_CONNECTION, &NGX_HTTP_PUSH_STREAM_WEBSOCKET_CONNECTION);
    ngx_http_push_stream_add_response_header(r, &NGX_HTTP_PUSH_STREAM_HEADER_SEC_WEBSOCKET_ACCEPT, sec_accept_header);
    r->headers_out.status_line = NGX_HTTP_PUSH_STREAM_101_STATUS_LINE;

    ngx_http_push_stream_send_only_added_headers(r);

    //get channels ids and backtracks from path
    requested_channels = ngx_http_push_stream_parse_channels_ids_from_path(r, ctx->temp_pool);
    if ((requested_channels == NULL) || ngx_queue_empty(&requested_channels->queue)) {
        ngx_log_error(NGX_LOG_WARN, r->connection->log, 0, "push stream module: the push_stream_channels_path is required but is not set");
        return ngx_http_push_stream_send_only_header_response(r, NGX_HTTP_BAD_REQUEST, &NGX_HTTP_PUSH_STREAM_NO_CHANNEL_ID_MESSAGE);
    }

    //validate channels: name, length and quantity. check if channel exists when authorized_channels_only is on. check if channel is full of subscribers
    if (ngx_http_push_stream_validate_channels(r, requested_channels, &status_code, &explain_error_message) == NGX_ERROR) {
        return ngx_http_push_stream_send_websocket_close_frame(r, status_code, explain_error_message);
    }

    // get control values
    ngx_http_push_stream_get_last_received_message_values(r, &if_modified_since, &tag, &last_event_id);

    // stream access
    if ((worker_subscriber = ngx_http_push_stream_subscriber_prepare_request_to_keep_connected(r)) == NULL) {
        return ngx_http_push_stream_send_websocket_close_frame(r, NGX_HTTP_INTERNAL_SERVER_ERROR, &NGX_HTTP_PUSH_STREAM_EMPTY);
    }

    // sending response content header
    if (ngx_http_push_stream_send_response_content_header(r, cf) == NGX_ERROR) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "push stream module: could not send content header to subscriber");
        return ngx_http_push_stream_send_websocket_close_frame(r, NGX_HTTP_INTERNAL_SERVER_ERROR, &NGX_HTTP_PUSH_STREAM_EMPTY);
    }

    if (ngx_http_push_stream_registry_subscriber(r, worker_subscriber) == NGX_ERROR) {
        return ngx_http_push_stream_send_websocket_close_frame(r, NGX_HTTP_INTERNAL_SERVER_ERROR, &NGX_HTTP_PUSH_STREAM_EMPTY);
    }

    // adding subscriber to channel(s) and send backtrack messages
    for (q = ngx_queue_head(&requested_channels->queue); q != ngx_queue_sentinel(&requested_channels->queue); q = ngx_queue_next(q)) {
        requested_channel = ngx_queue_data(q, ngx_http_push_stream_requested_channel_t, queue);
        if (ngx_http_push_stream_subscriber_assign_channel(mcf, cf, r, requested_channel, if_modified_since, tag, last_event_id, worker_subscriber, ctx->temp_pool) != NGX_OK) {
            return ngx_http_push_stream_send_websocket_close_frame(r, NGX_HTTP_INTERNAL_SERVER_ERROR, &NGX_HTTP_PUSH_STREAM_EMPTY);
        }
    }

    if (ctx->temp_pool != NULL) {
        ngx_destroy_pool(ctx->temp_pool);
        ctx->temp_pool = NULL;
    }
    return NGX_DONE;
}


ngx_str_t *
ngx_http_push_stream_generate_websocket_accept_value(ngx_http_request_t *r, ngx_str_t *sec_key, ngx_pool_t *temp_pool)
{
#if (NGX_HAVE_SHA1)
    ngx_str_t    *sha1_signed, *accept_value;
    ngx_sha1_t   sha1;

    sha1_signed = ngx_http_push_stream_create_str(temp_pool, NGX_HTTP_PUSH_STREAM_WEBSOCKET_SHA1_SIGNED_HASH_LENGTH);
    accept_value = ngx_http_push_stream_create_str(r->pool, ngx_base64_encoded_length(NGX_HTTP_PUSH_STREAM_WEBSOCKET_SHA1_SIGNED_HASH_LENGTH));

    if ((sha1_signed == NULL) || (accept_value == NULL)) {
        return NULL;
    }

    ngx_sha1_init(&sha1);
    ngx_sha1_update(&sha1, sec_key->data, sec_key->len);
    ngx_sha1_update(&sha1, NGX_HTTP_PUSH_STREAM_WEBSOCKET_SIGN_KEY.data, NGX_HTTP_PUSH_STREAM_WEBSOCKET_SIGN_KEY.len);
    ngx_sha1_final(sha1_signed->data, &sha1);

    ngx_encode_base64(accept_value, sha1_signed);

    return accept_value;
#else
    return NULL;
#endif
}


void
ngx_http_push_stream_websocket_reading(ngx_http_request_t *r)
{
    ngx_http_push_stream_loc_conf_t   *cf = ngx_http_get_module_loc_conf(r, ngx_http_push_stream_module);
    ngx_http_push_stream_module_ctx_t *ctx = ngx_http_get_module_ctx(r, ngx_http_push_stream_module);
    ngx_int_t                          rc = NGX_OK;
    ngx_event_t                       *rev;
    ngx_connection_t                  *c;
    uint64_t                           i;
    ngx_buf_t                          buf;
    ngx_queue_t                       *q;

    ngx_http_push_stream_set_buffer(&buf, ctx->frame->header, ctx->frame->last, 8);

    c = r->connection;
    rev = c->read;

    for (;;) {
        switch (ctx->frame->step) {
            case NGX_HTTP_PUSH_STREAM_WEBSOCKET_READ_START_STEP:
                //reading frame header
                if ((rc = ngx_http_push_stream_recv(c, rev, &buf, 2)) != NGX_OK) {
                    goto exit;
                }

                ctx->frame->fin  = (ctx->frame->header[0] >> 7) & 1;
                ctx->frame->rsv1 = (ctx->frame->header[0] >> 6) & 1;
                ctx->frame->rsv2 = (ctx->frame->header[0] >> 5) & 1;
                ctx->frame->rsv3 = (ctx->frame->header[0] >> 4) & 1;
                ctx->frame->opcode = ctx->frame->header[0] & 0xf;

                ctx->frame->mask = (ctx->frame->header[1] >> 7) & 1;
                ctx->frame->payload_len = ctx->frame->header[1] & 0x7f;

                ctx->frame->step = NGX_HTTP_PUSH_STREAM_WEBSOCKET_READ_GET_REAL_SIZE_STEP;

                break;

            case NGX_HTTP_PUSH_STREAM_WEBSOCKET_READ_GET_REAL_SIZE_STEP:

                if (ctx->frame->payload_len == 126) {
                    if ((rc = ngx_http_push_stream_recv(c, rev, &buf, 2)) != NGX_OK) {
                        goto exit;
                    }
                    uint16_t len;
                    ngx_memcpy(&len, ctx->frame->header, 2);
                    ctx->frame->payload_len = ntohs(len);
                } else if (ctx->frame->payload_len == 127) {
                    if ((rc = ngx_http_push_stream_recv(c, rev, &buf, 8)) != NGX_OK) {
                        goto exit;
                    }
                    uint64_t len;
                    ngx_memcpy(&len, ctx->frame->header, 8);
                    ctx->frame->payload_len = ngx_http_push_stream_ntohll(len);
                }

                ctx->frame->step = NGX_HTTP_PUSH_STREAM_WEBSOCKET_READ_GET_MASK_KEY_STEP;

                break;

            case NGX_HTTP_PUSH_STREAM_WEBSOCKET_READ_GET_MASK_KEY_STEP:

                if (ctx->frame->mask) {
                    if ((rc = ngx_http_push_stream_recv(c, rev, &buf, 4)) != NGX_OK) {
                        goto exit;
                    }

                    ngx_memcpy(ctx->frame->mask_key, buf.start, 4);
                }

                ctx->frame->step = NGX_HTTP_PUSH_STREAM_WEBSOCKET_READ_GET_PAYLOAD_STEP;

                break;

            case NGX_HTTP_PUSH_STREAM_WEBSOCKET_READ_GET_PAYLOAD_STEP:

                if (ctx->frame->payload_len > 0) {
                    //create a temporary pool to allocate temporary elements
                    if (ctx->temp_pool == NULL) {
                        if ((ctx->temp_pool = ngx_create_pool(4096, r->connection->log)) == NULL) {
                            ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "push stream module: unable to allocate memory for temporary pool");
                            ngx_http_finalize_request(r, NGX_OK);
                            return;
                        }

                        if ((ctx->frame->payload = ngx_pcalloc(ctx->temp_pool, ctx->frame->payload_len)) == NULL) {
                            ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "push stream module: unable to allocate memory for payload");
                            ngx_http_finalize_request(r, NGX_OK);
                            return;
                        }

                        ctx->frame->last = ctx->frame->payload;
                    }

                    ngx_http_push_stream_set_buffer(&buf, ctx->frame->payload, ctx->frame->last, ctx->frame->payload_len);

                    if ((rc = ngx_http_push_stream_recv(c, rev, &buf, (ssize_t) (buf.end - buf.last))) != NGX_OK) {
                        goto exit;
                    }

                    if (cf->websocket_allow_publish && (ctx->frame->opcode == NGX_HTTP_PUSH_STREAM_WEBSOCKET_TEXT_OPCODE)) {
                        if (ctx->frame->mask) {
                            for (i = 0; i < ctx->frame->payload_len; i++) {
                                ctx->frame->payload[i] = ctx->frame->payload[i] ^ ctx->frame->mask_key[i % 4];
                            }
                        }

                        for (q = ngx_queue_head(&ctx->subscriber->subscriptions); q != ngx_queue_sentinel(&ctx->subscriber->subscriptions); q = ngx_queue_next(q)) {
                            ngx_http_push_stream_subscription_t *subscription = ngx_queue_data(q, ngx_http_push_stream_subscription_t, queue);
                            if (ngx_http_push_stream_add_msg_to_channel(r, subscription->channel, ctx->frame->payload, ctx->frame->payload_len, NULL, NULL, ctx->temp_pool) != NGX_OK) {
                                ngx_http_finalize_request(r, NGX_OK);
                                return;
                            }
                        }
                        ngx_http_push_stream_send_response_text(r, NGX_HTTP_PUSH_STREAM_WEBSOCKET_PING_LAST_FRAME_BYTE, sizeof(NGX_HTTP_PUSH_STREAM_WEBSOCKET_PING_LAST_FRAME_BYTE), 1);
                    }

                    if (ctx->temp_pool != NULL) {
                        ngx_destroy_pool(ctx->temp_pool);
                        ctx->temp_pool = NULL;
                    }
                }

                if (ctx->frame->opcode == NGX_HTTP_PUSH_STREAM_WEBSOCKET_CLOSE_OPCODE) {
                    ngx_http_push_stream_send_response_finalize(r);
                }

                ctx->frame->step = NGX_HTTP_PUSH_STREAM_WEBSOCKET_READ_START_STEP;
                ctx->frame->last = NULL;
                return;

                break;
        }

        ngx_http_push_stream_set_buffer(&buf, ctx->frame->header, NULL, 8);
    }

exit:

    if (rc == NGX_AGAIN) {
        ctx->frame->last = buf.last;
        if (ngx_handle_read_event(c->read, 0) != NGX_OK) {
            ngx_log_error(NGX_LOG_INFO, c->log, ngx_socket_errno, "push stream module: failed to restore read events");

            ngx_http_finalize_request(r, NGX_OK);
        }
        return;
    }

    if (rc == NGX_ERROR) {
        ngx_log_error(NGX_LOG_INFO, c->log, ngx_socket_errno, "push stream module: client closed prematurely connection");

        ngx_http_finalize_request(r, NGX_OK);
        return;
    }
}


ngx_int_t
ngx_http_push_stream_recv(ngx_connection_t *c, ngx_event_t *rev, ngx_buf_t *buf, ssize_t len)
{
    ssize_t n = c->recv(c, buf->last, len);

    if (n == NGX_AGAIN) {
        return NGX_AGAIN;
    }

    if ((n == NGX_ERROR) || (n == 0)) {
        return NGX_ERROR;
    }

    buf->last += n;

    if (n < len) {
        return NGX_AGAIN;
    }

    return NGX_OK;
}


void
ngx_http_push_stream_set_buffer(ngx_buf_t *buf, u_char *start, u_char *last, ssize_t len)
{
    buf->start = start;
    buf->pos = buf->start;
    buf->last = (last != NULL) ? last : start;
    buf->end = buf->start + len;
    buf->temporary = 1;
}
