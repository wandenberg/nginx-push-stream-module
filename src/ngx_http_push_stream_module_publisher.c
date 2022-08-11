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
 * ngx_http_push_stream_module_publisher.c
 *
 * Created: Oct 26, 2010
 * Authors: Wandenberg Peixoto <wandenberg@gmail.com>, Rogério Carvalho Schneider <stockrt@gmail.com>
 */

#include <ngx_http_push_stream_module_publisher.h>
#include <ngx_http_push_stream_module_version.h>

static ngx_int_t    ngx_http_push_stream_publisher_handle_after_read_body(ngx_http_request_t *r, ngx_http_client_body_handler_pt post_handler);

static ngx_int_t
ngx_http_push_stream_publisher_handler(ngx_http_request_t *r)
{
    ngx_http_push_stream_main_conf_t   *mcf = ngx_http_get_module_main_conf(r, ngx_http_push_stream_module);
    ngx_http_push_stream_loc_conf_t    *cf = ngx_http_get_module_loc_conf(r, ngx_http_push_stream_module);
    ngx_http_push_stream_module_ctx_t  *ctx;

    ngx_http_push_stream_requested_channel_t       *requested_channels, *requested_channel;
    ngx_str_t                                       vv_allowed_origins = ngx_null_string;
    ngx_queue_t                                     *q;

    ngx_http_push_stream_set_expires(r, NGX_HTTP_PUSH_STREAM_EXPIRES_EPOCH, 0);


    if (cf->allowed_origins != NULL) {
        ngx_http_push_stream_complex_value(r, cf->allowed_origins, &vv_allowed_origins);
    }

    if (vv_allowed_origins.len > 0) {
        ngx_http_push_stream_add_response_header(r, &NGX_HTTP_PUSH_STREAM_HEADER_ACCESS_CONTROL_ALLOW_ORIGIN, &vv_allowed_origins);
        const ngx_str_t *header_value = (cf->location_type == NGX_HTTP_PUSH_STREAM_PUBLISHER_MODE_ADMIN) ? &NGX_HTTP_PUSH_STREAM_ALLOW_GET_POST_PUT_DELETE_METHODS : &NGX_HTTP_PUSH_STREAM_ALLOW_GET_POST_PUT_METHODS;
        ngx_http_push_stream_add_response_header(r, &NGX_HTTP_PUSH_STREAM_HEADER_ACCESS_CONTROL_ALLOW_METHODS, header_value);
        ngx_http_push_stream_add_response_header(r, &NGX_HTTP_PUSH_STREAM_HEADER_ACCESS_CONTROL_ALLOW_HEADERS, &NGX_HTTP_PUSH_STREAM_ALLOWED_HEADERS);
    }

    if (r->method & NGX_HTTP_OPTIONS) {
        return ngx_http_push_stream_send_only_header_response(r, NGX_HTTP_OK, NULL);
    }

    // only accept GET, POST, PUT and DELETE methods if enable publisher administration
    if ((cf->location_type == NGX_HTTP_PUSH_STREAM_PUBLISHER_MODE_ADMIN) && !(r->method & (NGX_HTTP_GET|NGX_HTTP_POST|NGX_HTTP_PUT|NGX_HTTP_DELETE))) {
        ngx_http_push_stream_add_response_header(r, &NGX_HTTP_PUSH_STREAM_HEADER_ALLOW, &NGX_HTTP_PUSH_STREAM_ALLOW_GET_POST_PUT_DELETE_METHODS);
        return ngx_http_push_stream_send_only_header_response(r, NGX_HTTP_NOT_ALLOWED, NULL);
    }

    // only accept GET, POST and PUT methods if NOT enable publisher administration
    if ((cf->location_type != NGX_HTTP_PUSH_STREAM_PUBLISHER_MODE_ADMIN) && !(r->method & (NGX_HTTP_GET|NGX_HTTP_POST|NGX_HTTP_PUT))) {
        ngx_http_push_stream_add_response_header(r, &NGX_HTTP_PUSH_STREAM_HEADER_ALLOW, &NGX_HTTP_PUSH_STREAM_ALLOW_GET_POST_PUT_METHODS);
        return ngx_http_push_stream_send_only_header_response(r, NGX_HTTP_NOT_ALLOWED, NULL);
    }

    if ((ctx = ngx_http_push_stream_add_request_context(r)) == NULL) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "push stream module: unable to create request context");
        return ngx_http_push_stream_send_only_header_response(r, NGX_HTTP_INTERNAL_SERVER_ERROR, NULL);
    }

    //get channels ids
    requested_channels = ngx_http_push_stream_parse_channels_ids_from_path(r, r->pool);
    if ((requested_channels == NULL) || ngx_queue_empty(&requested_channels->queue)) {
        ngx_log_error(NGX_LOG_WARN, r->connection->log, 0, "push stream module: the push_stream_channels_path is required but is not set");
        return ngx_http_push_stream_send_only_header_response(r, NGX_HTTP_BAD_REQUEST, &NGX_HTTP_PUSH_STREAM_NO_CHANNEL_ID_MESSAGE);
    }

    for (q = ngx_queue_head(&requested_channels->queue); q != ngx_queue_sentinel(&requested_channels->queue); q = ngx_queue_next(q)) {
        requested_channel = ngx_queue_data(q, ngx_http_push_stream_requested_channel_t, queue);

        // check if channel id isn't equals to ALL or contain wildcard
        if ((ngx_memn2cmp(requested_channel->id->data, NGX_HTTP_PUSH_STREAM_ALL_CHANNELS_INFO_ID.data, requested_channel->id->len, NGX_HTTP_PUSH_STREAM_ALL_CHANNELS_INFO_ID.len) == 0) || (ngx_strchr(requested_channel->id->data, '*') != NULL)) {
            return ngx_http_push_stream_send_only_header_response(r, NGX_HTTP_FORBIDDEN, &NGX_HTTP_PUSH_STREAM_CHANNEL_ID_NOT_AUTHORIZED_MESSAGE);
        }

        // could not have a large size
        if ((mcf->max_channel_id_length != NGX_CONF_UNSET_UINT) && (requested_channel->id->len > mcf->max_channel_id_length)) {
            ngx_log_error(NGX_LOG_WARN, r->connection->log, 0, "push stream module: channel id is larger than allowed %d", requested_channel->id->len);
            return ngx_http_push_stream_send_only_header_response(r, NGX_HTTP_BAD_REQUEST, &NGX_HTTP_PUSH_STREAM_TOO_LARGE_CHANNEL_ID_MESSAGE);
        }

        if (r->method & (NGX_HTTP_POST|NGX_HTTP_PUT)) {
            // create the channel if doesn't exist
            requested_channel->channel = ngx_http_push_stream_get_channel(requested_channel->id, r->connection->log, mcf);
            if (requested_channel->channel == NULL) {
                return ngx_http_push_stream_send_only_header_response(r, NGX_HTTP_INTERNAL_SERVER_ERROR, NULL);
            }

            if (requested_channel->channel == NGX_HTTP_PUSH_STREAM_NUMBER_OF_CHANNELS_EXCEEDED) {
                ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "push stream module: number of channels were exceeded");
                return ngx_http_push_stream_send_only_header_response(r, NGX_HTTP_FORBIDDEN, &NGX_HTTP_PUSH_STREAM_NUMBER_OF_CHANNELS_EXCEEDED_MESSAGE);
            }
        } else {
            requested_channel->channel = ngx_http_push_stream_find_channel(requested_channel->id, r->connection->log, mcf);
        }

        if ((r->method != NGX_HTTP_GET) && (requested_channel->channel != NULL) && requested_channel->channel->for_events) {
            ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "push stream module: only internal routines can change events channel");
            return ngx_http_push_stream_send_only_header_response(r, NGX_HTTP_FORBIDDEN, &NGX_HTTP_PUSH_STREAM_INTERNAL_ONLY_EVENTS_CHANNEL_MESSAGE);
        }
    }

    ctx->requested_channels = requested_channels;

    if (r->method & (NGX_HTTP_POST|NGX_HTTP_PUT)) {
        return ngx_http_push_stream_publisher_handle_after_read_body(r, ngx_http_push_stream_publisher_body_handler);
    }

    if ((cf->location_type == NGX_HTTP_PUSH_STREAM_PUBLISHER_MODE_ADMIN) && (r->method == NGX_HTTP_DELETE)) {
        return ngx_http_push_stream_publisher_handle_after_read_body(r, ngx_http_push_stream_publisher_delete_handler);
    }

    return ngx_http_push_stream_send_response_channels_info_detailed(r, requested_channels);
}

static ngx_int_t
ngx_http_push_stream_publisher_handle_after_read_body(ngx_http_request_t *r, ngx_http_client_body_handler_pt post_handler)
{
    ngx_int_t                           rc;

    /*
     * Instruct ngx_http_read_subscriber_request_body to store the request
     * body entirely in a memory buffer or in a file.
     */
    r->request_body_in_single_buf = 0;
    r->request_body_in_persistent_file = 1;
    r->request_body_in_clean_file = 0;
    r->request_body_file_log_level = 0;

    // parse the body message and return
    rc = ngx_http_read_client_request_body(r, post_handler);
    if (rc >= NGX_HTTP_SPECIAL_RESPONSE) {
        return rc;
    }

    return NGX_DONE;
}

static ngx_buf_t *
ngx_http_push_stream_read_request_body_to_buffer(ngx_http_request_t *r)
{
    ngx_buf_t                              *buf = NULL;
    ngx_chain_t                            *chain;
    ssize_t                                 n;
    off_t                                   len;

    buf = ngx_create_temp_buf(r->pool, r->headers_in.content_length_n + 1);
    if (buf != NULL) {
        buf->memory = 1;
        buf->temporary = 0;
        ngx_memset(buf->start, '\0', r->headers_in.content_length_n + 1);

        chain = r->request_body->bufs;
        while ((chain != NULL) && (chain->buf != NULL)) {
            len = ngx_buf_size(chain->buf);
            // if buffer is equal to content length all the content is in this buffer
            if (len >= r->headers_in.content_length_n) {
                buf->start = buf->pos;
                buf->last = buf->pos;
                len = r->headers_in.content_length_n;
            }

            if (chain->buf->in_file) {
                n = ngx_read_file(chain->buf->file, buf->start, len, 0);
                if (n == NGX_FILE_ERROR) {
                    ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "push stream module: cannot read file with request body");
                    return NULL;
                }
                buf->last = buf->last + len;
                ngx_delete_file(chain->buf->file->name.data);
                chain->buf->file->fd = NGX_INVALID_FILE;
            } else {
                buf->last = ngx_copy(buf->start, chain->buf->pos, len);
            }

            chain = chain->next;
            buf->start = buf->last;
        }
    }
    return buf;
}

static void
ngx_http_push_stream_publisher_delete_handler(ngx_http_request_t *r)
{
    ngx_http_push_stream_main_conf_t       *mcf = ngx_http_get_module_main_conf(r, ngx_http_push_stream_module);
    ngx_http_push_stream_module_ctx_t      *ctx = ngx_http_get_module_ctx(r, ngx_http_push_stream_module);
    ngx_buf_t                              *buf = NULL;
    u_char                                 *text = mcf->channel_deleted_message_text.data;
    size_t                                  len = mcf->channel_deleted_message_text.len;
    ngx_uint_t                              qtd_channels = 0;
    ngx_int_t                               rc;

    ngx_http_push_stream_requested_channel_t       *requested_channel;
    ngx_queue_t                                    *q;

    if (r->headers_in.content_length_n > 0) {

        // get and check if has access to request body
        NGX_HTTP_PUSH_STREAM_CHECK_AND_FINALIZE_REQUEST_ON_ERROR(r->request_body->bufs, NULL, r, "push stream module: unexpected publisher message request body buffer location. please report this to the push stream module developers.");

        buf = ngx_http_push_stream_read_request_body_to_buffer(r);
        NGX_HTTP_PUSH_STREAM_CHECK_AND_FINALIZE_REQUEST_ON_ERROR(buf, NULL, r, "push stream module: cannot allocate memory for read the message");

        text = buf->pos;
        len = ngx_buf_size(buf);
    }

    for (q = ngx_queue_head(&ctx->requested_channels->queue); q != ngx_queue_sentinel(&ctx->requested_channels->queue); q = ngx_queue_next(q)) {
        requested_channel = ngx_queue_data(q, ngx_http_push_stream_requested_channel_t, queue);
        rc = ngx_http_push_stream_delete_channel(mcf, requested_channel->channel, text, len, r->pool);

        if (rc == -1) {
            ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "push stream module: error while deleting channel '%V'", &requested_channel->channel->id);
            ngx_http_push_stream_send_only_header_response_and_finalize(r, NGX_HTTP_INTERNAL_SERVER_ERROR, NULL);
            return;
        }

        if (rc > 0) {
            qtd_channels++;
        }
    }

    if (qtd_channels == 0) {
        ngx_http_push_stream_send_only_header_response_and_finalize(r, NGX_HTTP_NOT_FOUND, NULL);
    } else {
        ngx_http_push_stream_send_only_header_response_and_finalize(r, NGX_HTTP_OK, &NGX_HTTP_PUSH_STREAM_CHANNEL_DELETED);
    }
}

static void
ngx_http_push_stream_publisher_body_handler(ngx_http_request_t *r)
{
    ngx_str_t                              *event_id, *event_type;
    ngx_http_push_stream_module_ctx_t      *ctx = ngx_http_get_module_ctx(r, ngx_http_push_stream_module);
    ngx_http_push_stream_main_conf_t       *mcf = ngx_http_get_module_main_conf(r, ngx_http_push_stream_module);
    ngx_http_push_stream_loc_conf_t        *cf = ngx_http_get_module_loc_conf(r, ngx_http_push_stream_module);
    ngx_buf_t                              *buf = NULL;

    ngx_http_push_stream_requested_channel_t       *requested_channel;
    ngx_queue_t                                    *q;

    // check if body message wasn't empty
    if (r->headers_in.content_length_n <= 0) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "push stream module: Post request was sent with no message");
        ngx_http_push_stream_send_only_header_response_and_finalize(r, NGX_HTTP_BAD_REQUEST, &NGX_HTTP_PUSH_STREAM_EMPTY_POST_REQUEST_MESSAGE);
        return;
    }

    // get and check if has access to request body
    NGX_HTTP_PUSH_STREAM_CHECK_AND_FINALIZE_REQUEST_ON_ERROR(r->request_body->bufs, NULL, r, "push stream module: unexpected publisher message request body buffer location. please report this to the push stream module developers.");


    // copy request body to a memory buffer
    buf = ngx_http_push_stream_read_request_body_to_buffer(r);
    NGX_HTTP_PUSH_STREAM_CHECK_AND_FINALIZE_REQUEST_ON_ERROR(buf, NULL, r, "push stream module: cannot allocate memory for read the message");

    event_id = ngx_http_push_stream_get_header(r, &NGX_HTTP_PUSH_STREAM_HEADER_EVENT_ID);
    event_type = ngx_http_push_stream_get_header(r, &NGX_HTTP_PUSH_STREAM_HEADER_EVENT_TYPE);

    for (q = ngx_queue_head(&ctx->requested_channels->queue); q != ngx_queue_sentinel(&ctx->requested_channels->queue); q = ngx_queue_next(q)) {
        requested_channel = ngx_queue_data(q, ngx_http_push_stream_requested_channel_t, queue);

        if (ngx_http_push_stream_add_msg_to_channel(mcf, r->connection->log, requested_channel->channel, buf->pos, ngx_buf_size(buf), event_id, event_type, cf->store_messages, r->pool) != NGX_OK) {
            ngx_http_finalize_request(r, NGX_HTTP_INTERNAL_SERVER_ERROR);
            return;
        }
    }

    if (cf->channel_info_on_publish) {
        ngx_http_push_stream_send_response_channels_info_detailed(r, ctx->requested_channels);
        ngx_http_finalize_request(r, NGX_OK);
    } else {
        ngx_http_push_stream_send_only_header_response_and_finalize(r, NGX_HTTP_OK, NULL);
    }
}

static ngx_int_t
ngx_http_push_stream_channels_statistics_handler(ngx_http_request_t *r)
{
    ngx_http_push_stream_main_conf_t   *mcf = ngx_http_get_module_main_conf(r, ngx_http_push_stream_module);
    char                               *pos = NULL;

    ngx_http_push_stream_requested_channel_t       *requested_channels, *requested_channel;
    ngx_queue_t                                     *q;


    ngx_http_push_stream_set_expires(r, NGX_HTTP_PUSH_STREAM_EXPIRES_EPOCH, 0);

    // only accept GET method
    if (!(r->method & NGX_HTTP_GET)) {
        ngx_http_push_stream_add_response_header(r, &NGX_HTTP_PUSH_STREAM_HEADER_ALLOW, &NGX_HTTP_PUSH_STREAM_ALLOW_GET);
        return ngx_http_push_stream_send_only_header_response(r, NGX_HTTP_NOT_ALLOWED, NULL);
    }

    ngx_http_push_stream_add_response_header(r, &NGX_HTTP_PUSH_STREAM_HEADER_TAG, &NGX_HTTP_PUSH_STREAM_TAG);
    ngx_http_push_stream_add_response_header(r, &NGX_HTTP_PUSH_STREAM_HEADER_COMMIT, &NGX_HTTP_PUSH_STREAM_COMMIT);

    //get channels ids
    requested_channels = ngx_http_push_stream_parse_channels_ids_from_path(r, r->pool);

    // if not specify a channel id, get info about all channels in a resumed way
    if ((requested_channels == NULL) || ngx_queue_empty(&requested_channels->queue)) {
        return ngx_http_push_stream_send_response_all_channels_info_summarized(r);
    }

    for (q = ngx_queue_head(&requested_channels->queue); q != ngx_queue_sentinel(&requested_channels->queue); q = ngx_queue_next(q)) {
        requested_channel = ngx_queue_data(q, ngx_http_push_stream_requested_channel_t, queue);

        // could not have a large size
        if ((mcf->max_channel_id_length != NGX_CONF_UNSET_UINT) && (requested_channel->id->len > mcf->max_channel_id_length)) {
            ngx_log_error(NGX_LOG_WARN, r->connection->log, 0, "push stream module: channel id is larger than allowed %d", requested_channel->id->len);
            return ngx_http_push_stream_send_only_header_response(r, NGX_HTTP_BAD_REQUEST, &NGX_HTTP_PUSH_STREAM_TOO_LARGE_CHANNEL_ID_MESSAGE);
        }

        if ((pos = ngx_strchr(requested_channel->id->data, '*')) != NULL) {
            ngx_str_t *aux = NULL;
            if (pos != (char *) requested_channel->id->data) {
                *pos = '\0';
                requested_channel->id->len  = ngx_strlen(requested_channel->id->data);
                aux = requested_channel->id;
            }
            return ngx_http_push_stream_send_response_all_channels_info_detailed(r, aux);
        }

        // if specify a channel id equals to ALL, get info about all channels in a detailed way
        if (ngx_memn2cmp(requested_channel->id->data, NGX_HTTP_PUSH_STREAM_ALL_CHANNELS_INFO_ID.data, requested_channel->id->len, NGX_HTTP_PUSH_STREAM_ALL_CHANNELS_INFO_ID.len) == 0) {
            return ngx_http_push_stream_send_response_all_channels_info_detailed(r, NULL);
        }

        requested_channel->channel = ngx_http_push_stream_find_channel(requested_channel->id, r->connection->log, mcf);
    }

    // if specify a channels ids != ALL, get info about specified channels if they exists
    return ngx_http_push_stream_send_response_channels_info_detailed(r, requested_channels);
}
