#include <ngx_http_push_stream_module_publisher.h>

static ngx_int_t
ngx_http_push_stream_publisher_handler(ngx_http_request_t *r)
{
    ngx_int_t                           rc;
    ngx_str_t                          *id = NULL;
    ngx_http_push_stream_channel_t     *channel = NULL;
    ngx_http_push_stream_loc_conf_t    *cf = ngx_http_get_module_loc_conf(r, ngx_http_push_stream_module);

    // Publisher never do a keep alive connection
    r->keepalive = 0;

    // only accept GET and POST methods
    if (!(r->method & (NGX_HTTP_GET|NGX_HTTP_POST))) {
        ngx_http_push_stream_add_response_header(r, &NGX_HTTP_PUSH_STREAM_HEADER_ALLOW, &NGX_HTTP_PUSH_STREAM_ALLOWED_METHODS);
        return ngx_http_push_stream_send_only_header_response(r, NGX_HTTP_NOT_ALLOWED, NULL);
    }

    // channel id is required
    id = ngx_http_push_stream_get_channel_id(r, cf);
    if ((id == NULL) || (id == NGX_HTTP_PUSH_STREAM_UNSET_CHANNEL_ID) || (id == NGX_HTTP_PUSH_STREAM_TOO_LARGE_CHANNEL_ID)) {
        if (id == NGX_HTTP_PUSH_STREAM_UNSET_CHANNEL_ID) {
            ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "push stream module: the $push_stream_channel_id variable is required but is not set");
            return ngx_http_push_stream_send_only_header_response(r, NGX_HTTP_BAD_REQUEST, &NGX_HTTP_PUSH_STREAM_NO_CHANNEL_ID_MESSAGE);
        }
        if (id == NGX_HTTP_PUSH_STREAM_TOO_LARGE_CHANNEL_ID) {
            return ngx_http_push_stream_send_only_header_response(r, NGX_HTTP_BAD_REQUEST, &NGX_HTTP_PUSH_STREAM_TOO_LARGE_CHANNEL_ID_MESSAGE);
        }
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    // search for a existing channel with this id
    channel = ngx_http_push_stream_find_channel(id, r->connection->log);

    if (r->method == NGX_HTTP_POST) {
        // check if channel id isn't equals to ALL
        if (ngx_memn2cmp(id->data, NGX_HTTP_PUSH_STREAM_ALL_CHANNELS_INFO_ID.data, id->len, NGX_HTTP_PUSH_STREAM_ALL_CHANNELS_INFO_ID.len) == 0) {
            return ngx_http_push_stream_send_only_header_response(r, NGX_HTTP_FORBIDDEN, &NGX_HTTP_PUSH_STREAM_NO_CHANNEL_ID_NOT_AUTHORIZED_MESSAGE);
        }

        // create the channel if doesn't exist
        channel = ngx_http_push_stream_get_channel(id, r->connection->log, cf);
        if (channel == NULL) {
            ngx_log_error(NGX_LOG_ERR, (r)->connection->log, 0, "push stream module: unable to allocate memory for new channel");
            return ngx_http_push_stream_send_only_header_response(r, NGX_HTTP_INTERNAL_SERVER_ERROR, NULL);
        }

        if (channel == NGX_HTTP_PUSH_STREAM_NUMBER_OF_CHANNELS_EXCEEDED) {
            ngx_log_error(NGX_LOG_ERR, (r)->connection->log, 0, "push stream module: number of channels were exceeded");
            return ngx_http_push_stream_send_only_header_response(r, NGX_HTTP_FORBIDDEN, &NGX_HTTP_PUSH_STREAM_NUMBER_OF_CHANNELS_EXCEEDED_MESSAGE);
        }

        /*
         * Instruct ngx_http_read_subscriber_request_body to store the request
         * body entirely in a memory buffer or in a file.
         */
        r->request_body_in_single_buf = 0;
        r->request_body_in_persistent_file = 1;
        r->request_body_in_clean_file = 0;
        r->request_body_file_log_level = 0;

        // parse the body message and return
        rc = ngx_http_read_client_request_body(r, ngx_http_push_stream_publisher_body_handler);
        if (rc >= NGX_HTTP_SPECIAL_RESPONSE) {
            return rc;
        }

        // turn on timer to cleanup memory of old messages and channels
        ngx_http_push_stream_memory_cleanup_timer_set(cf);

        return NGX_OK;
    }

    // GET only make sense with a previous existing channel
    if (channel == NULL) {
        return ngx_http_push_stream_send_only_header_response(r, NGX_HTTP_NOT_FOUND, NULL);
    }

    return ngx_http_push_stream_send_response_channel_info(r, channel);
}

static void
ngx_http_push_stream_publisher_body_handler(ngx_http_request_t *r)
{
    ngx_str_t                              *id;
    ngx_http_push_stream_loc_conf_t        *cf = ngx_http_get_module_loc_conf(r, ngx_http_push_stream_module);
    ngx_slab_pool_t                        *shpool = (ngx_slab_pool_t *) ngx_http_push_stream_shm_zone->shm.addr;
    ngx_buf_t                              *buf = NULL, *buf_msg = NULL;
    ngx_chain_t                            *chain;
    ngx_http_push_stream_channel_t         *channel;
    ssize_t                                 n;
    off_t                                   len;
    ngx_http_push_stream_msg_t             *msg;

    // check if body message wasn't empty
    if (r->headers_in.content_length_n <= 0) {
        ngx_log_error(NGX_LOG_ERR, (r)->connection->log, 0, "push stream module: Post request was sent with no message");
        ngx_http_push_stream_send_only_header_response(r, NGX_HTTP_BAD_REQUEST, &NGX_HTTP_PUSH_STREAM_EMPTY_POST_REQUEST_MESSAGE);
        return;
    }

    // get and check if has access to request body
    NGX_HTTP_PUSH_STREAM_CHECK_AND_FINALIZE_REQUEST_ON_ERROR(r->request_body->bufs, NULL, r, "push stream module: unexpected publisher message request body buffer location. please report this to the push stream module developers.");

    // get and check channel id value
    id = ngx_http_push_stream_get_channel_id(r, cf);
    NGX_HTTP_PUSH_STREAM_CHECK_AND_FINALIZE_REQUEST_ON_ERROR(id, NULL, r, "push stream module: something goes very wrong, arrived on ngx_http_push_stream_publisher_body_handler without channel id");
    NGX_HTTP_PUSH_STREAM_CHECK_AND_FINALIZE_REQUEST_ON_ERROR(id, NGX_HTTP_PUSH_STREAM_UNSET_CHANNEL_ID, r, "push stream module: something goes very wrong, arrived on ngx_http_push_stream_publisher_body_handler without channel id");
    NGX_HTTP_PUSH_STREAM_CHECK_AND_FINALIZE_REQUEST_ON_ERROR(id, NGX_HTTP_PUSH_STREAM_TOO_LARGE_CHANNEL_ID, r, "push stream module: something goes very wrong, arrived on ngx_http_push_stream_publisher_body_handler with channel id too large");

    // just find the channel. if it's not there, NULL and return error.
    channel = ngx_http_push_stream_find_channel(id, r->connection->log);
    if (channel == NULL) {
        ngx_log_error(NGX_LOG_ERR, (r)->connection->log, 0, "push stream module: something goes very wrong, arrived on ngx_http_push_stream_publisher_body_handler without created channel %s", id->data);
        ngx_http_finalize_request(r, NGX_HTTP_INTERNAL_SERVER_ERROR);
        return;
    }

    // copy request body to a memory buffer
    buf = ngx_create_temp_buf(r->pool, r->headers_in.content_length_n + 1);
    NGX_HTTP_PUSH_STREAM_CHECK_AND_FINALIZE_REQUEST_ON_ERROR(buf, NULL, r, "push stream module: cannot allocate memory for read the message");
    ngx_memset(buf->start, '\0', r->headers_in.content_length_n + 1);

    chain = r->request_body->bufs;
    while ((chain != NULL) && (chain->buf != NULL)) {
        len = ngx_buf_size(chain->buf);
        // if buffer is equal to content length all the content is in this buffer
        if (len == r->headers_in.content_length_n) {
            buf->start = buf->pos;
            buf->last = buf->pos;
        }

        if (chain->buf->in_file) {
            n = ngx_read_file(chain->buf->file, buf->start, len, 0);
            if (n == NGX_FILE_ERROR) {
                ngx_log_error(NGX_LOG_ERR, (r)->connection->log, 0, "push stream module: cannot read file with request body");
                ngx_http_finalize_request(r, NGX_HTTP_INTERNAL_SERVER_ERROR);
                return;
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

    // discard request body it is no longer needed
    ngx_http_discard_request_body(r);
    r->discard_body = 1;

    // format message
    buf_msg = ngx_http_push_stream_get_formatted_message(cf, channel, buf, r->pool);
    NGX_HTTP_PUSH_STREAM_CHECK_AND_FINALIZE_REQUEST_ON_ERROR(buf_msg, NULL, r, "push stream module: unable to format message");

    ngx_shmtx_lock(&shpool->mutex);

    // create a buffer copy in shared mem
    msg = ngx_http_push_stream_convert_buffer_to_msg_on_shared_locked(buf_msg);
    if (msg == NULL) {
        ngx_shmtx_unlock(&(shpool)->mutex);
        ngx_log_error(NGX_LOG_ERR, (r)->connection->log, 0, "push stream module: unable to allocate message in shared memory");
        ngx_http_finalize_request(r, NGX_HTTP_INTERNAL_SERVER_ERROR);
        return;
    }

    channel->last_message_id++;
    ((ngx_http_push_stream_shm_data_t *) ngx_http_push_stream_shm_zone->data)->published_messages++;

    // put messages on the queue
    if (cf->store_messages) {
        // set message expiration time
        msg->expires = (cf->buffer_timeout == NGX_CONF_UNSET ? 0 : (ngx_time() + cf->buffer_timeout));
        ngx_queue_insert_tail(&channel->message_queue.queue, &msg->queue);
        channel->stored_messages++;
    }

    // now see if the queue is too big
    ngx_http_push_stream_ensure_qtd_of_messages_locked(channel, cf->max_messages, 0, cf->memory_cleanup_timeout);

    ngx_shmtx_unlock(&shpool->mutex);

    // send an alert to workers
    ngx_http_push_stream_broadcast(channel, msg, r->connection->log);

    // turn on timer to cleanup memory of old messages an channels
    ngx_http_push_stream_memory_cleanup_timer_set(cf);

    ngx_http_push_stream_send_response_channel_info(r, channel);
    return;
}

static ngx_int_t
push_stream_channels_statistics_handler(ngx_http_request_t *r)
{
    ngx_str_t                          *id = NULL;
    ngx_http_push_stream_channel_t     *channel = NULL;
    ngx_http_push_stream_loc_conf_t    *cf = ngx_http_get_module_loc_conf(r, ngx_http_push_stream_module);

    // only accept GET method
    if (!(r->method & NGX_HTTP_GET)) {
        ngx_http_push_stream_add_response_header(r, &NGX_HTTP_PUSH_STREAM_HEADER_ALLOW, &NGX_HTTP_PUSH_STREAM_ALLOW_GET);
        return ngx_http_push_stream_send_only_header_response(r, NGX_HTTP_NOT_ALLOWED, NULL);
    }

    // get and check channel id value
    id = ngx_http_push_stream_get_channel_id(r, cf);
    if ((id == NULL) || (id == NGX_HTTP_PUSH_STREAM_TOO_LARGE_CHANNEL_ID)) {
        if (id == NGX_HTTP_PUSH_STREAM_TOO_LARGE_CHANNEL_ID) {
            return ngx_http_push_stream_send_only_header_response(r, NGX_HTTP_BAD_REQUEST, &NGX_HTTP_PUSH_STREAM_TOO_LARGE_CHANNEL_ID_MESSAGE);
        }
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    // if not specify a channel id, get info about all channels in a resumed way
    if (id == NGX_HTTP_PUSH_STREAM_UNSET_CHANNEL_ID) {
        return ngx_http_push_stream_send_response_all_channels_info_summarized(r);
    }

    // if specify a channel id equals to ALL, get info about all channels in a detailed way
    if (ngx_memn2cmp(id->data, NGX_HTTP_PUSH_STREAM_ALL_CHANNELS_INFO_ID.data, id->len, NGX_HTTP_PUSH_STREAM_ALL_CHANNELS_INFO_ID.len) == 0) {
        return ngx_http_push_stream_send_response_all_channels_info_detailed(r);
    }

    // if specify a channel id != ALL, get info about specified channel if it exists
    // search for a existing channel with this id
    channel = ngx_http_push_stream_find_channel(id, r->connection->log);

    if (channel == NULL) {
        return ngx_http_push_stream_send_only_header_response(r, NGX_HTTP_NOT_FOUND, NULL);
    }

    return ngx_http_push_stream_send_response_channel_info(r, channel);
}
