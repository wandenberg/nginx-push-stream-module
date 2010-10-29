#include <ngx_http_push_stream_module.h>


static ngx_int_t
ngx_http_push_stream_publisher_handler(ngx_http_request_t *r)
{
    ngx_int_t                           rc;


    /*
     * Instruct ngx_http_read_subscriber_request_body to store the request
     * body entirely in a memory buffer or in a file.
     */
    r->request_body_in_single_buf = 1;
    r->request_body_in_persistent_file = 1;
    r->request_body_in_clean_file = 0;
    r->request_body_file_log_level = 0;
    r->keepalive = 0;

    rc = ngx_http_read_client_request_body(r, ngx_http_push_stream_publisher_body_handler);
    if (rc >= NGX_HTTP_SPECIAL_RESPONSE) {
        return rc;
    }

    return NGX_DONE;
}


static void
ngx_http_push_stream_publisher_body_handler(ngx_http_request_t *r)
{
    ngx_str_t                              *id;
    ngx_http_push_stream_loc_conf_t        *cf = ngx_http_get_module_loc_conf(r, ngx_http_push_stream_module);
    ngx_slab_pool_t                        *shpool = (ngx_slab_pool_t *) ngx_http_push_stream_shm_zone->shm.addr;
    ngx_buf_t                              *buf = NULL, *buf_copy, *buf_msg = NULL;
    ngx_http_push_stream_channel_t         *channel;
    ngx_uint_t                              method = r->method;
    ngx_uint_t                              subscribers = 0;
    ngx_uint_t                              published_messages = 0;
    ngx_uint_t                              stored_messages = 0;


    if ((id = ngx_http_push_stream_get_channel_id(r, cf)) == NULL) {
        ngx_http_finalize_request(r, r->headers_out.status ? NGX_OK : NGX_HTTP_INTERNAL_SERVER_ERROR);
        return;
    }

    ngx_shmtx_lock(&shpool->mutex);
    // POST requests will need a channel created if it doesn't yet exist.
    if ((ngx_strstr(id->data, NGX_HTTP_PUSH_STREAM_ALL_CHANNELS_INFO_ID.data) != (const char *) id->data) && (method == NGX_HTTP_POST || method == NGX_HTTP_PUT)) {
        channel = ngx_http_push_stream_get_channel(id, r->connection->log);
        NGX_HTTP_PUSH_STREAM_PUBLISHER_CHECK_LOCKED(channel, NULL, r, "push stream module: unable to allocate memory for new channel", shpool);
    } else { // no other request method needs that.
        // just find the channel. if it's not there, NULL.
        channel = ngx_http_push_stream_find_channel(id, r->connection->log);
    }

    if (channel != NULL) {
        subscribers = channel->subscribers;
        published_messages = channel->last_message_id;
        stored_messages = channel->stored_messages;
    } else if ((method != NGX_HTTP_GET) || (ngx_strstr(id->data, NGX_HTTP_PUSH_STREAM_ALL_CHANNELS_INFO_ID.data) != (const char *) id->data)) {
        // 404!
        ngx_shmtx_unlock(&shpool->mutex);
        r->headers_out.status = NGX_HTTP_NOT_FOUND;

        // just the headers, please. we don't care to describe the situation or
        // respond with an html page
        r->headers_out.content_length_n = 0;
        r->header_only = 1;

        ngx_http_finalize_request(r, ngx_http_send_header(r));
        return;
    }
    ngx_shmtx_unlock(&shpool->mutex);

    switch (method) {
        ngx_http_push_stream_msg_t      *msg;
        ngx_http_push_stream_msg_t      *sentinel;

        case NGX_HTTP_POST:
            // first off, we'll want to extract the body buffer

            // note: this works mostly because of r->request_body_in_single_buf = 1;
            // which, i suppose, makes this module a little slower than it could be.
            // this block is a little hacky. might be a thorn for forward-compatibility.
            if (r->headers_in.content_length_n == -1 || r->headers_in.content_length_n == 0) {
                buf = ngx_create_temp_buf(r->pool, 0);
                // this buffer will get copied to shared memory in a few lines,
                // so it does't matter what pool we make it in.
            } else if (r->request_body->bufs->buf != NULL) { // everything in the first buffer, please
                buf = r->request_body->bufs->buf;
            } else if (r->request_body->bufs->next != NULL) {
                buf = r->request_body->bufs->next->buf;
            } else {
                ngx_log_error(NGX_LOG_ERR, (r)->connection->log, 0, "push stream module: unexpected publisher message request body buffer location. please report this to the push stream module developers.");
                ngx_http_finalize_request(r, NGX_HTTP_INTERNAL_SERVER_ERROR);
                return;
            }

            if ((r->headers_in.content_length_n > 0) && (buf != NULL)) {
                buf->last = buf->pos + r->headers_in.content_length_n;
                *buf->last = '\0';
            }

            NGX_HTTP_PUSH_STREAM_PUBLISHER_CHECK(buf, NULL, r, "push stream module: can't find or allocate publisher request body buffer");

            ngx_shmtx_lock(&shpool->mutex);

            buf_msg = ngx_http_push_stream_get_formatted_message_locked(cf, channel, buf, r->pool);

            // create a buffer copy in shared mem
            msg = ngx_http_push_stream_slab_alloc_locked(sizeof(*msg));
            NGX_HTTP_PUSH_STREAM_PUBLISHER_CHECK_LOCKED(msg, NULL, r, "push stream module: unable to allocate message in shared memory", shpool);

            buf_copy = ngx_http_push_stream_slab_alloc_locked(NGX_HTTP_PUSH_STREAM_BUF_ALLOC_SIZE(buf_msg));
            NGX_HTTP_PUSH_STREAM_PUBLISHER_CHECK_LOCKED(buf_copy, NULL, r, "push stream module: unable to allocate buffer in shared memory", shpool) // magic nullcheck
            ngx_http_push_stream_copy_preallocated_buffer(buf_msg, buf_copy);

            msg->buf = buf_copy;

            channel->last_message_id++;

            if (cf->store_messages) {
                ngx_queue_insert_tail(&channel->message_queue->queue, &msg->queue);
                channel->stored_messages++;
            }

            // set message expiration time
            time_t      message_timeout = cf->buffer_timeout;
            msg->expires = (message_timeout == 0 ? 0 : (ngx_time() + message_timeout));
            msg->persistent = (message_timeout == 0 ? 1 : 0);

            msg->delete_oldest_received_min_messages = cf->delete_oldest_received_message ? (ngx_uint_t) cf->min_messages : NGX_MAX_UINT32_VALUE;
            // NGX_MAX_UINT32_VALUE to disable, otherwise = min_message_buffer_size of the publisher location from whence the message came

            // FMI (For My Information): shm is still locked.
            switch (ngx_http_push_stream_broadcast_message_locked(channel, msg, r->connection->log, shpool)) {
                case NGX_HTTP_PUSH_STREAM_MESSAGE_QUEUED:
                    // message was queued successfully, but there were no
                    // subscribers to receive it.
                    r->headers_out.status = NGX_HTTP_ACCEPTED;
                    r->headers_out.status_line.len = sizeof("202 Accepted") - 1;
                    r->headers_out.status_line.data = (u_char *) "202 Accepted";
                    break;

                case NGX_HTTP_PUSH_STREAM_MESSAGE_RECEIVED:
                    // message was queued successfully, and it was already sent
                    // to at least one subscriber
                    r->headers_out.status = NGX_HTTP_CREATED;
                    r->headers_out.status_line.len = sizeof("201 Created") - 1;
                    r->headers_out.status_line.data = (u_char *) "201 Created";

                    // update the number of times the message was received.
                    // in the interest of premature optimization, I assume all
                    // current subscribers have received the message successfully.
                    break;

                case NGX_ERROR:
                    // WTF?
                    ngx_shmtx_unlock(&shpool->mutex);
                    ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "push stream module: error broadcasting message to workers");
                    ngx_http_finalize_request(r, NGX_HTTP_INTERNAL_SERVER_ERROR);
                    return;

                default:
                    // for debugging, mostly. I don't expect this branch to be
                    // hit during regular operation
                    ngx_shmtx_unlock(&shpool->mutex);
                    ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "push stream module: TOTALLY UNEXPECTED error broadcasting message to workers");
                    ngx_http_finalize_request(r, NGX_HTTP_INTERNAL_SERVER_ERROR);
                    return;
            }
            // shm is still locked I hope.

            if (buf->file != NULL) {
                // future subscribers won't be able to use this file descriptor --
                // it will be closed once the publisher request is finalized.
                // (That's about to happen a handful of lines below.)
                msg->buf->file->fd = NGX_INVALID_FILE;
            }

            // now see if the queue is too big
            if (channel->stored_messages > (ngx_uint_t) cf->max_messages) {
                // exceeeds max queue size. force-delete oldest message
                ngx_http_push_stream_force_delete_message_locked(channel, ngx_http_push_stream_get_oldest_message_locked(channel), shpool);
            }
            if (channel->stored_messages > (ngx_uint_t) cf->min_messages) {
                // exceeeds min queue size. maybe delete the oldest message
                ngx_http_push_stream_msg_t      *oldest_msg = ngx_http_push_stream_get_oldest_message_locked(channel);
                NGX_HTTP_PUSH_STREAM_PUBLISHER_CHECK_LOCKED(oldest_msg, NULL, r, "push stream module: oldest message not found", shpool);
            }
            published_messages = channel->last_message_id;
            stored_messages = channel->stored_messages;

            ngx_shmtx_unlock(&shpool->mutex);
            ngx_http_finalize_request(r, ngx_http_push_stream_channel_info(r, channel->id, published_messages, stored_messages, subscribers));
            return;

        case NGX_HTTP_PUT:
        case NGX_HTTP_GET:
            r->headers_out.status = NGX_HTTP_OK;
            if (ngx_strstr(id->data, NGX_HTTP_PUSH_STREAM_ALL_CHANNELS_INFO_ID.data) == (const char *) id->data) {
                ngx_http_finalize_request(r, ngx_http_push_stream_all_channels_info(r));
            } else {
                ngx_http_finalize_request(r, ngx_http_push_stream_channel_info(r, channel->id, published_messages, stored_messages, subscribers));
            }
            return;

        case NGX_HTTP_DELETE:
            ngx_shmtx_lock(&shpool->mutex);
            sentinel = channel->message_queue;
            msg = sentinel;

            while ((msg = (ngx_http_push_stream_msg_t *) ngx_queue_next(&msg->queue)) != sentinel) {
                // force-delete all the messages
                ngx_http_push_stream_force_delete_message_locked(NULL, msg, shpool);
            }
            channel->last_message_id = 0;
            channel->stored_messages = 0;
            published_messages = channel->last_message_id;
            stored_messages = channel->stored_messages;

            // 410 gone
            NGX_HTTP_PUSH_STREAM_PUBLISHER_CHECK_LOCKED(ngx_http_push_stream_broadcast_status_locked(channel, NGX_HTTP_GONE, &NGX_HTTP_PUSH_STREAM_HTTP_STATUS_410, r->connection->log, shpool), NGX_ERROR, r, "push stream module: unable to send current subscribers a 410 Gone response", shpool);
            ngx_http_push_stream_delete_channel_locked(channel);
            ngx_shmtx_unlock(&shpool->mutex);
            // done.
            r->headers_out.status = NGX_HTTP_OK;
            ngx_http_finalize_request(r, ngx_http_push_stream_channel_info(r, channel->id, published_messages, stored_messages, subscribers));
            return;

        default:
            // some other weird request method
            ngx_http_push_stream_add_response_header(r, &NGX_HTTP_PUSH_STREAM_HEADER_ALLOW, &NGX_HTTP_PUSH_STREAM_ALLOW_GET_POST_PUT_DELETE);
            ngx_http_finalize_request(r, NGX_HTTP_NOT_ALLOWED);
            return;
    }
}
