#include <ngx_http_push_stream_module.h>


static ngx_int_t
ngx_http_push_stream_subscriber_assign_channel_locked(ngx_slab_pool_t *shpool, ngx_http_push_stream_loc_conf_t *cf, ngx_http_request_t *r, ngx_str_t *id, ngx_uint_t backtrack_messages, ngx_queue_t *messages_to_sent_queue, ngx_http_push_stream_subscription_t *subscriptions_sentinel, ngx_pool_t *temp_pool)
{
    ngx_http_push_stream_pid_queue_t           *sentinel, *cur, *found;
    ngx_http_push_stream_channel_t             *channel;
    ngx_http_push_stream_subscriber_t          *subscriber;
    ngx_http_push_stream_subscriber_t          *subscriber_sentinel;
    ngx_queue_t                                *node;
    ngx_http_push_stream_subscription_t        *subscription;
    ngx_flag_t                                  is_broadcast_channel = 0;


    if ((cf->broadcast_channel_max_qtd > 0) && (cf->broadcast_channel_prefix.len > 0)) {
        u_char      *broad_pos = (u_char *) ngx_strstr(id->data, cf->broadcast_channel_prefix.data);
        if ((broad_pos != NULL) && (broad_pos == id->data)) {
            is_broadcast_channel = 1;
        }
    }

    channel = (((cf->authorize_channel == 1) && (is_broadcast_channel == 0)) ? ngx_http_push_stream_find_channel : ngx_http_push_stream_get_channel) (id, r->connection->log);

    if (channel == NULL) {
        // unable to allocate channel OR channel not found
        ngx_shmtx_unlock(&shpool->mutex);
        if (cf->authorize_channel) {
            ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "push stream module: not authorized to access channel %s", id->data);
            return NGX_HTTP_FORBIDDEN;
        } else {
            ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "push stream module: unable to allocate shared memory for channel %s", id->data);
            return NGX_HTTP_INTERNAL_SERVER_ERROR;
        }
    }

    if ((channel->stored_messages == 0) && !is_broadcast_channel && cf->authorize_channel) {
        ngx_shmtx_unlock(&shpool->mutex);
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "push stream module: not authorized to access channel %s, channel is empty of messages", id->data);
        return NGX_HTTP_FORBIDDEN;
    }

    sentinel = &channel->workers_with_subscribers;
    cur = (ngx_http_push_stream_pid_queue_t *) ngx_queue_head(&sentinel->queue);
    found = NULL;

    while (cur != sentinel) {
        if (cur->pid == ngx_pid) {
            found = cur;
            break;
        }
        cur = (ngx_http_push_stream_pid_queue_t *) ngx_queue_next(&cur->queue);
    }

    if (found == NULL) { // found nothing
        if ((found = ngx_http_push_stream_slab_alloc_locked(sizeof(*found))) == NULL) {
            ngx_shmtx_unlock(&shpool->mutex);
            ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "push stream module: unable to allocate worker subscriber queue marker in shared memory");
            return NGX_HTTP_INTERNAL_SERVER_ERROR;
        }
        // initialize
        ngx_queue_insert_tail(&sentinel->queue, &found->queue);
        found->pid = ngx_pid;
        found->slot = ngx_process_slot;
        found->subscriber_sentinel = NULL;
    }

    // figure out the subscriber sentinel
    subscriber_sentinel = ((ngx_http_push_stream_pid_queue_t *) found)->subscriber_sentinel;
    if (subscriber_sentinel == NULL) {
        // it's perfectly nornal for the sentinel to be NULL
        if ((subscriber_sentinel=ngx_http_push_stream_slab_alloc_locked(sizeof(*subscriber_sentinel))) == NULL) {
            ngx_shmtx_unlock(&shpool->mutex);
            ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "push stream module: unable to allocate channel subscriber sentinel");
            return NGX_HTTP_INTERNAL_SERVER_ERROR;
        }
        ngx_queue_init(&subscriber_sentinel->queue);
        ((ngx_http_push_stream_pid_queue_t *) found)->subscriber_sentinel=subscriber_sentinel;
    }

    if ((subscription = ngx_palloc(r->pool, sizeof(*subscription))) == NULL) {
        ngx_shmtx_unlock(&shpool->mutex);
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "push stream module: unable to allocate subscribed channel reference");
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    if ((subscriber = ngx_palloc(r->pool, sizeof(*subscriber))) == NULL) { // unable to allocate request queue element
        return NGX_ERROR;
    }

    subscriber->request = r;

    subscription->channel = channel;
    subscription->subscriber = subscriber;

    channel->subscribers++; // do this only when we know everything went okay

    // get old messages to send to new subscriber
    if (channel->stored_messages > 0) {
        node = ngx_queue_last(&channel->message_queue->queue);
        ngx_uint_t qtd = (backtrack_messages > channel->stored_messages) ? channel->stored_messages : backtrack_messages;
        while (qtd > 0) {
            ngx_http_push_stream_msg_queue_t    *message = NULL;
            if ((message = ngx_palloc(temp_pool, sizeof(*message))) != NULL) {
                message->msg = (ngx_http_push_stream_msg_t *) node;
                ngx_queue_insert_head(messages_to_sent_queue, &message->queue);
            }
            node = ngx_queue_prev(node);
            qtd--;
        }
    }

    ngx_queue_insert_tail(&subscriptions_sentinel->queue, &subscription->queue);
    ngx_queue_insert_tail(&subscriber_sentinel->queue, &subscriber->queue);

    return NGX_OK;
}


static ngx_int_t
ngx_http_push_stream_subscriber_handler(ngx_http_request_t *r)
{
    ngx_http_push_stream_loc_conf_t                *cf = ngx_http_get_module_loc_conf(r, ngx_http_push_stream_module);
    ngx_slab_pool_t                                *shpool = (ngx_slab_pool_t *)ngx_http_push_stream_shm_zone->shm.addr;
    ngx_str_t                                      *id, *channels_path;
    ngx_http_push_stream_worker_subscriber_t       *worker_subscriber = NULL;
    ngx_http_push_stream_subscriber_cleanup_t      *clndata;
    ngx_pool_cleanup_t                             *cln;
    ngx_pool_t                                     *temp_pool;
    ngx_queue_t                                     messages_to_sent_queue;
    ngx_http_variable_value_t                      *vv_channels_path = ngx_http_get_indexed_variable(r, cf->index_channels_path);


    if (vv_channels_path == NULL || vv_channels_path->not_found || vv_channels_path->len == 0) {
        ngx_http_push_stream_send_response_channel_id_not_provided(r);
        return NGX_HTTP_NOT_FOUND;
    }

    if (r->method != NGX_HTTP_GET) {
        ngx_http_push_stream_add_response_header(r, &NGX_HTTP_PUSH_STREAM_HEADER_ALLOW, &NGX_HTTP_PUSH_STREAM_ALLOW_GET); // valid HTTP for teh win
        return NGX_HTTP_NOT_ALLOWED;
    }

    if ((temp_pool = ngx_create_pool(NGX_CYCLE_POOL_SIZE, r->connection->log)) == NULL) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "push stream module: unable to allocate memory for temporary pool");
        return NGX_ERROR;
    }

    if ((id = ngx_pcalloc(temp_pool, sizeof(*id) + vv_channels_path->len + 1)) == NULL) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "push stream module: unable to allocate memory for channel_id string");
        return NGX_ERROR;
    }

    id->data = (u_char *) (id + 1);

    if ((channels_path = ngx_pcalloc(temp_pool, sizeof(*channels_path) + vv_channels_path->len + 1)) == NULL) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "push stream module: unable to allocate memory for channels_path string");
        return NGX_ERROR;
    }

    channels_path->data = (u_char *) (channels_path + 1);
    channels_path->len = vv_channels_path->len;
    ngx_memcpy(channels_path->data, vv_channels_path->data, vv_channels_path->len);

    if ((worker_subscriber=ngx_palloc(r->pool, sizeof(*worker_subscriber))) == NULL) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "push stream module: unable to allocate worker subscriber");
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    if ((worker_subscriber->subscriptions_sentinel = ngx_palloc(r->pool, sizeof(*worker_subscriber->subscriptions_sentinel))) == NULL) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "push stream module: unable to allocate subscribed channels sentinel");
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    worker_subscriber->request = r;
    worker_subscriber->worker_subscribed_pid = ngx_pid;
    time_t      subscriber_timeout = cf->subscriber_connection_timeout;
    worker_subscriber->expires = ((subscriber_timeout == NGX_CONF_UNSET) || (subscriber_timeout == 0)) ? 0 : (ngx_time() + subscriber_timeout);

    ngx_queue_init(&worker_subscriber->queue);
    ngx_queue_init(&worker_subscriber->subscriptions_sentinel->queue);

    // attach a cleaner to remove the request from the channel
    if ((cln = ngx_pool_cleanup_add(r->pool, sizeof(*clndata))) == NULL) { // make sure we can
        return NGX_ERROR;
    }

    cln->handler = (ngx_pool_cleanup_pt) ngx_http_push_stream_subscriber_cleanup;
    clndata = (ngx_http_push_stream_subscriber_cleanup_t *) cln->data;
    clndata->worker_subscriber = worker_subscriber;
    worker_subscriber->clndata = clndata;

    ngx_queue_init(&messages_to_sent_queue);

    ngx_shmtx_lock(&shpool->mutex);

    u_char         *channel_pos = channels_path->data;
    u_char         *end = NULL, *slash_pos = NULL;
    ngx_uint_t      len = 0;
    ngx_uint_t      backtrack_messages = 0;
    ngx_uint_t      subscribed_channels_qtd = 0;
    ngx_uint_t      subscribed_broadcast_channels_qtd = 0;

    // doing the parser of given channel path
    while (channel_pos != NULL) {
        end = channels_path->data + channels_path->len;

        slash_pos = (u_char *) ngx_strstr(channel_pos, NGX_HTTP_PUSH_STREAM_SLASH.data);
        if (slash_pos != NULL) {
            end = slash_pos;
        }

        backtrack_messages = 0;
        len = end - channel_pos;

        u_char      *backtrack_pos = (u_char *) ngx_strstr(channel_pos, NGX_HTTP_PUSH_STREAM_BACKTRACK_SEP.data);
        if ((backtrack_pos != NULL) && (end > backtrack_pos)) {
            len = backtrack_pos - channel_pos;
            backtrack_pos = backtrack_pos + NGX_HTTP_PUSH_STREAM_BACKTRACK_SEP.len;
            if (end > backtrack_pos) {
                backtrack_messages = ngx_atoi(backtrack_pos, end - backtrack_pos);
            }
        }

        if (len > 0) {
            backtrack_messages = (backtrack_messages > 0) ? backtrack_messages : 0;
            id->len = len;
            ngx_memcpy(id->data, channel_pos, len);
            *(id->data + id->len) = '\0';

            ngx_int_t ret = ngx_http_push_stream_subscriber_assign_channel_locked(shpool, cf, r, id, backtrack_messages, &messages_to_sent_queue, worker_subscriber->subscriptions_sentinel, temp_pool);
            if (ret != NGX_OK) {
                // if get here the shpool already is unlocked
                ngx_destroy_pool(temp_pool);
                return ret;
            }

            subscribed_channels_qtd++;
            if (cf->broadcast_channel_prefix.len > 0) {
                u_char      *broad_pos = (u_char *) ngx_strstr(channel_pos, cf->broadcast_channel_prefix.data);
                if ((broad_pos != NULL) && (broad_pos == channel_pos)) {
                    subscribed_broadcast_channels_qtd++;
                    if (subscribed_broadcast_channels_qtd > (ngx_uint_t)cf->broadcast_channel_max_qtd) {
                        ngx_shmtx_unlock(&shpool->mutex);
                        ngx_destroy_pool(temp_pool);
                        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "push stream module: max subscribed broadcast channels exceeded");
                        return NGX_HTTP_FORBIDDEN;
                    }
                }
            }
        }

        channel_pos = NULL;
        if (slash_pos != NULL) {
            channel_pos = slash_pos + NGX_HTTP_PUSH_STREAM_SLASH.len;
        }
    }
    ngx_shmtx_unlock(&shpool->mutex);

    if ((subscribed_channels_qtd - subscribed_broadcast_channels_qtd) == 0) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "push stream module: subscribe broadcast channel whithout subscribe a common channel");
        ngx_destroy_pool(temp_pool);
        return NGX_HTTP_FORBIDDEN;
    }

    r->read_event_handler = ngx_http_test_reading;
    r->write_event_handler = ngx_http_request_empty_handler;
    r->discard_body = 1;

    r->headers_out.content_type = cf->content_type;
    r->headers_out.status = NGX_HTTP_OK;
    r->headers_out.content_length_n = -1;

    ngx_http_push_stream_worker_data_t      *workers_data = ((ngx_http_push_stream_shm_data_t *) ngx_http_push_stream_shm_zone->data)->ipc;
    ngx_http_push_stream_worker_data_t      *thisworker_data = workers_data + ngx_process_slot;

    ngx_http_send_header(r);
#if defined nginx_version && nginx_version >= 8053
    r->keepalive = 1;
#else
    r->keepalive = 0;
#endif
    ngx_http_push_stream_send_body_header(r, cf);

    ngx_shmtx_lock(&shpool->mutex);
    ngx_queue_insert_tail(&thisworker_data->worker_subscribers_sentinel->queue, &worker_subscriber->queue);
    ngx_shmtx_unlock(&shpool->mutex);

    ngx_http_push_stream_ping_timer_set(cf);
    ngx_http_push_stream_disconnect_timer_set(cf);

    // send old messages to subscriber
    if (&messages_to_sent_queue !=  ngx_queue_next(&messages_to_sent_queue)) {
        ngx_chain_t     *chain = NULL;
        ngx_int_t rc = NGX_OK;
        NGX_HTTP_PUSH_STREAM_MAKE_IN_MEMORY_CHAIN(chain, temp_pool, "push stream module: unable to allocate chain to send old messages to new subscriber");

        ngx_queue_t     *message = ngx_queue_next(&messages_to_sent_queue);
        while (&messages_to_sent_queue != message) {
            ngx_http_push_stream_msg_t      *msg = ((ngx_http_push_stream_msg_queue_t *) message)->msg;
            chain->buf->pos = msg->buf->pos;
            chain->buf->last = msg->buf->last;
            chain->buf->start = msg->buf->start;
            chain->buf->end = msg->buf->end;

            rc = ngx_http_output_filter(r, chain);

            if (rc == NGX_OK) {
                rc = ngx_http_send_special(r, NGX_HTTP_FLUSH);
            }

            if (rc != NGX_OK) {
                break;
            }

            message = ngx_queue_next(message);
        }
    }

    ngx_destroy_pool(temp_pool);

    return NGX_DONE;
}


static ngx_int_t
ngx_http_push_stream_broadcast_locked(ngx_http_push_stream_channel_t *channel, ngx_http_push_stream_msg_t *msg, ngx_int_t status_code, const ngx_str_t *status_line, ngx_log_t *log, ngx_slab_pool_t *shpool)
{
    // subscribers are queued up in a local pool. Queue heads, however, are located
    // in shared memory, identified by pid.
    ngx_http_push_stream_pid_queue_t       *sentinel = &channel->workers_with_subscribers;
    ngx_http_push_stream_pid_queue_t       *cur = sentinel;
    ngx_int_t                               received;


    received = channel->subscribers > 0 ? NGX_HTTP_PUSH_STREAM_MESSAGE_RECEIVED : NGX_HTTP_PUSH_STREAM_MESSAGE_QUEUED;

    if ((msg != NULL) && (received == NGX_HTTP_PUSH_STREAM_MESSAGE_RECEIVED)) {
        ngx_http_push_stream_reserve_message_locked(channel, msg);
    }

    while ((cur = (ngx_http_push_stream_pid_queue_t *) ngx_queue_next(&cur->queue)) != sentinel) {
        pid_t                                   worker_pid  = cur->pid;
        ngx_int_t                               worker_slot = cur->slot;
        ngx_http_push_stream_subscriber_t      *subscriber_sentinel= cur->subscriber_sentinel;

        ngx_shmtx_unlock(&shpool->mutex);

        // interprocess communication breakdown
        if (ngx_http_push_stream_send_worker_message(channel, subscriber_sentinel, worker_pid, worker_slot, msg, status_code, log) != NGX_ERROR) {
            ngx_http_push_stream_alert_worker(worker_pid, worker_slot, log);
        } else {
            ngx_log_error(NGX_LOG_ERR, log, 0, "push stream module: error communicating with some other worker process");
        }

        ngx_shmtx_lock(&shpool->mutex);
    }

    return received;
}


static ngx_int_t
ngx_http_push_stream_respond_to_subscribers(ngx_http_push_stream_channel_t *channel, ngx_http_push_stream_subscriber_t *sentinel, ngx_http_push_stream_msg_t *msg, ngx_int_t status_code, const ngx_str_t *status_line)
{
    ngx_slab_pool_t                        *shpool = ngx_http_push_stream_shpool;
    ngx_http_push_stream_subscriber_t      *cur, *next;
    ngx_int_t                               responded_subscribers = 0;


    if (sentinel == NULL) {
        return NGX_OK;
    }

    cur = (ngx_http_push_stream_subscriber_t *) ngx_queue_head(&sentinel->queue);

    if (msg != NULL) {
        // copy everything we need first
        ngx_chain_t             *chain;
        ngx_http_request_t      *r;
        ngx_buf_t               *buffer;
        u_char                  *pos;
        ngx_pool_t              *temp_pool;

        ngx_shmtx_lock(&shpool->mutex);

        if ((temp_pool = ngx_create_pool(NGX_CYCLE_POOL_SIZE, ngx_http_push_stream_pool->log)) == NULL) {
            ngx_shmtx_unlock(&shpool->mutex);
            ngx_log_error(NGX_LOG_ERR, ngx_http_push_stream_pool->log, 0, "push stream module: unable to allocate memory for temporary pool");
            return NGX_ERROR;
        }

        // preallocate output chain. yes, same one for every waiting subscriber
        if ((chain = ngx_http_push_stream_create_output_chain_locked(msg->buf, temp_pool, ngx_cycle->log, shpool)) == NULL) {
            ngx_shmtx_unlock(&shpool->mutex);
            ngx_log_error(NGX_LOG_ERR, ngx_cycle->log, 0, "push stream module: unable to create output chain while responding to several subscriber request");
            ngx_destroy_pool(temp_pool);
            return NGX_ERROR;
        }

        buffer = chain->buf;
        pos = buffer->pos;

        ngx_shmtx_unlock(&shpool->mutex);
        buffer->last_buf = 0;

        // now let's respond to some requests!
        while (cur != sentinel) {
            next = (ngx_http_push_stream_subscriber_t *) ngx_queue_next(&cur->queue);

            // in this block, nothing in shared memory should be dereferenced
            r = cur->request;

            r->discard_body = 0; // hacky hacky!
            chain->buf->flush = 1;

            ngx_http_output_filter(r, chain);
            ngx_http_send_special(r, NGX_HTTP_FLUSH);

            responded_subscribers++;

            // rewind the buffer, please
            buffer->pos = pos;
            buffer->last_buf = 0;

            cur = next;
        }

        // free everything relevant
        if (buffer->file) {
            ngx_close_file(buffer->file->fd);
        }

        if (responded_subscribers && !msg->persistent) {
            ngx_shmtx_lock(&shpool->mutex);
            // message deletion
            ngx_http_push_stream_release_message_locked(channel, msg);
            ngx_shmtx_unlock(&shpool->mutex);
        }
        ngx_destroy_pool(temp_pool);
    } else {
        // headers only probably
        ngx_http_request_t      *r;

        while (cur != sentinel) {
            next = (ngx_http_push_stream_subscriber_t *) ngx_queue_next(&cur->queue);
            r = cur->request;

            // cleanup oughtn't dequeue anything. or decrement the subscriber count, for that matter
            cur->clndata->worker_subscriber = NULL;
            ngx_http_push_stream_respond_status_only(r, status_code, status_line);

            cur = next;
        }
    }

    return NGX_OK;
}


static void
ngx_http_push_stream_subscriber_cleanup(ngx_http_push_stream_subscriber_cleanup_t *data)
{
    if (data->worker_subscriber != NULL) {
        ngx_shmtx_lock(&ngx_http_push_stream_shpool->mutex);
        ngx_http_push_stream_worker_subscriber_cleanup_locked(data->worker_subscriber);
        ngx_shmtx_unlock(&ngx_http_push_stream_shpool->mutex);
    }
}
