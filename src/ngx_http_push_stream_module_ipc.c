#include <ngx_http_push_stream_module.h>


// worker processes of the world, unite.
ngx_socket_t    ngx_http_push_stream_socketpairs[NGX_MAX_PROCESSES][2];


static ngx_int_t
ngx_http_push_stream_init_ipc(ngx_cycle_t *cycle, ngx_int_t workers)
{
    int         i, s = 0, on = 1;
    ngx_int_t   last_expected_process = ngx_last_process;


    /*
     * here's the deal: we have no control over fork()ing, nginx's internal
     * socketpairs are unusable for our purposes (as of nginx 0.8 -- check the
     * code to see why), and the module initialization callbacks occur before
     * any workers are spawned. Rather than futzing around with existing
     * socketpairs, we populate our own socketpairs array.
     * Trouble is, ngx_spawn_process() creates them one-by-one, and we need to
     * do it all at once. So we must guess all the workers' ngx_process_slots in
     * advance. Meaning the spawning logic must be copied to the T.
     */

    for(i=0; i<workers; i++) {
        while (s < last_expected_process && ngx_processes[s].pid != -1) {
            // find empty existing slot
            s++;
        }

        // copypaste from os/unix/ngx_process.c (ngx_spawn_process)
        ngx_socket_t    *socks = ngx_http_push_stream_socketpairs[s];
        if (socketpair(AF_UNIX, SOCK_STREAM, 0, socks) == -1) {
            ngx_log_error(NGX_LOG_ALERT, cycle->log, ngx_errno, "socketpair() failed on socketpair while initializing push stream module");
            return NGX_ERROR;
        }
        if (ngx_nonblocking(socks[0]) == -1) {
            ngx_log_error(NGX_LOG_ALERT, cycle->log, ngx_errno, ngx_nonblocking_n " failed on socketpair while initializing push stream module");
            ngx_close_channel(socks, cycle->log);
            return NGX_ERROR;
        }
        if (ngx_nonblocking(socks[1]) == -1) {
            ngx_log_error(NGX_LOG_ALERT, cycle->log, ngx_errno, ngx_nonblocking_n " failed on socketpair while initializing push stream module");
            ngx_close_channel(socks, cycle->log);
            return NGX_ERROR;
        }
        if (ioctl(socks[0], FIOASYNC, &on) == -1) {
            ngx_log_error(NGX_LOG_ALERT, cycle->log, ngx_errno, "ioctl(FIOASYNC) failed on socketpair while initializing push stream module");
            ngx_close_channel(socks, cycle->log);
            return NGX_ERROR;
        }
        if (fcntl(socks[0], F_SETOWN, ngx_pid) == -1) {
            ngx_log_error(NGX_LOG_ALERT, cycle->log, ngx_errno, "fcntl(F_SETOWN) failed on socketpair while initializing push stream module");
            ngx_close_channel(socks, cycle->log);
            return NGX_ERROR;
        }
        if (fcntl(socks[0], F_SETFD, FD_CLOEXEC) == -1) {
            ngx_log_error(NGX_LOG_ALERT, cycle->log, ngx_errno, "fcntl(FD_CLOEXEC) failed on socketpair while initializing push stream module");
            ngx_close_channel(socks, cycle->log);
            return NGX_ERROR;
        }
        if (fcntl(socks[1], F_SETFD, FD_CLOEXEC) == -1) {
            ngx_log_error(NGX_LOG_ALERT, cycle->log, ngx_errno, "fcntl(FD_CLOEXEC) failed while initializing push stream module");
            ngx_close_channel(socks, cycle->log);
            return NGX_ERROR;
        }

        s++; // NEXT!!
    }

    return NGX_OK;
}


static void
ngx_http_push_stream_ipc_exit_worker(ngx_cycle_t *cycle)
{
    ngx_close_channel((ngx_socket_t *) ngx_http_push_stream_socketpairs[ngx_process_slot], cycle->log);
}


static ngx_int_t
ngx_http_push_stream_reset_channel_subscribers_count_locked(ngx_http_push_stream_channel_t *channel, ngx_slab_pool_t *shpool)
{
    channel->subscribers = 0;

    return NGX_OK;
}


// will be called many times
static ngx_int_t
ngx_http_push_stream_init_ipc_shm(ngx_int_t workers)
{
    ngx_slab_pool_t                        *shpool = (ngx_slab_pool_t *) ngx_http_push_stream_shm_zone->shm.addr;
    ngx_http_push_stream_shm_data_t        *d = (ngx_http_push_stream_shm_data_t *) ngx_http_push_stream_shm_zone->data;
    ngx_http_push_stream_worker_data_t     *workers_data;
    int                                     i;


    ngx_shmtx_lock(&shpool->mutex);

    if (d->ipc != NULL) {
        // already initialized... reset channel subscribers counters and census subscribers
        ngx_http_push_stream_worker_data_t          *workers_data = ((ngx_http_push_stream_shm_data_t *) ngx_http_push_stream_shm_zone->data)->ipc;
        ngx_http_push_stream_worker_data_t          *thisworker_data = workers_data + ngx_process_slot;
        ngx_http_push_stream_worker_subscriber_t    *sentinel = thisworker_data->worker_subscribers_sentinel;

        ngx_queue_init(&sentinel->queue);

        ngx_http_push_stream_walk_rbtree(ngx_http_push_stream_reset_channel_subscribers_count_locked);

        ngx_shmtx_unlock(&shpool->mutex);

        for(i=0; i<workers; i++) {
            ngx_http_push_stream_alert_worker_census_subscribers(ngx_pid, i, ngx_http_push_stream_pool->log);
        }

        return NGX_OK;
    }

    // initialize worker message queues
    if ((workers_data = ngx_slab_alloc_locked(shpool, sizeof(*workers_data)*workers)) == NULL) {
        ngx_shmtx_unlock(&shpool->mutex);
        return NGX_ERROR;
    }

    for(i=0; i<workers; i++) {
        ngx_queue_init(&workers_data[i].messages_queue);
        if ((workers_data[i].worker_subscribers_sentinel = ngx_slab_alloc_locked(shpool, sizeof(*workers_data[i].worker_subscribers_sentinel))) == NULL) {
            ngx_shmtx_unlock(&shpool->mutex);
            return NGX_ERROR;
        }
        ngx_queue_init(&((ngx_http_push_stream_worker_subscriber_t *) workers_data[i].worker_subscribers_sentinel)->queue);
    }

    d->ipc = workers_data;

    ngx_shmtx_unlock(&shpool->mutex);

    return NGX_OK;
}


static ngx_int_t
ngx_http_push_stream_register_worker_message_handler(ngx_cycle_t *cycle)
{
    if (ngx_add_channel_event(cycle, ngx_http_push_stream_socketpairs[ngx_process_slot][1], NGX_READ_EVENT, ngx_http_push_stream_channel_handler) == NGX_ERROR) {
        ngx_log_error(NGX_LOG_ALERT, cycle->log, ngx_errno, "failed to register channel handler while initializing push stream module worker");
        return NGX_ERROR;
    }

    return NGX_OK;
}


static void
ngx_http_push_stream_channel_handler(ngx_event_t *ev)
{
    // copypaste from os/unix/ngx_process_cycle.c (ngx_channel_handler)
    ngx_int_t           n;
    ngx_channel_t       ch;
    ngx_connection_t   *c;


    if (ev->timedout) {
        ev->timedout = 0;
        return;
    }
    c = ev->data;

    while (1) {
        n = ngx_read_channel(c->fd, &ch, sizeof(ch), ev->log);
        if (n == NGX_ERROR) {
            if (ngx_event_flags & NGX_USE_EPOLL_EVENT) {
                ngx_del_conn(c, 0);
            }
            ngx_close_connection(c);
            return;
        }

        if ((ngx_event_flags & NGX_USE_EVENTPORT_EVENT) && (ngx_add_event(ev, NGX_READ_EVENT, 0) == NGX_ERROR)) {
            return;
        }

        if (n == NGX_AGAIN) {
            return;
        }
        //ngx_log_debug1(NGX_LOG_DEBUG_CORE, ev->log, 0, "push stream module: channel command: %d", ch.command);

        if (ch.command == NGX_CMD_HTTP_PUSH_STREAM_CHECK_MESSAGES) {
            ngx_http_push_stream_process_worker_message();
        } else if (ch.command == NGX_CMD_HTTP_PUSH_STREAM_SEND_PING) {
            ngx_http_push_stream_send_worker_ping_message();
        } else if (ch.command == NGX_CMD_HTTP_PUSH_STREAM_DISCONNECT_SUBSCRIBERS) {
            // disconnect only expired subscribers (force_disconnect = 0)
            ngx_http_push_stream_disconnect_worker_subscribers(0);
        } else if (ch.command == NGX_CMD_HTTP_PUSH_STREAM_CENSUS_SUBSCRIBERS) {
            ngx_http_push_stream_census_worker_subscribers();
        }
    }
}


static ngx_int_t
ngx_http_push_stream_alert_worker(ngx_pid_t pid, ngx_int_t slot, ngx_log_t *log)
{
    // seems ch doesn't need to have fd set. odd, but roll with it. pid and process slot also unnecessary.
    static ngx_channel_t    ch = {NGX_CMD_HTTP_PUSH_STREAM_CHECK_MESSAGES, 0, 0, -1};

    return ngx_write_channel(ngx_http_push_stream_socketpairs[slot][0], &ch, sizeof(ngx_channel_t), log);
}


static ngx_int_t
ngx_http_push_stream_alert_worker_send_ping(ngx_pid_t pid, ngx_int_t slot, ngx_log_t *log)
{
    // seems ch doesn't need to have fd set. odd, but roll with it. pid and process slot also unnecessary.
    static ngx_channel_t    ch = {NGX_CMD_HTTP_PUSH_STREAM_SEND_PING, 0, 0, -1};

    return ngx_write_channel(ngx_http_push_stream_socketpairs[slot][0], &ch, sizeof(ngx_channel_t), log);
}


static ngx_int_t
ngx_http_push_stream_alert_worker_disconnect_subscribers(ngx_pid_t pid, ngx_int_t slot, ngx_log_t *log)
{
    // seems ch doesn't need to have fd set. odd, but roll with it. pid and process slot also unnecessary.
    static ngx_channel_t    ch = {NGX_CMD_HTTP_PUSH_STREAM_DISCONNECT_SUBSCRIBERS, 0, 0, -1};

    return ngx_write_channel(ngx_http_push_stream_socketpairs[slot][0], &ch, sizeof(ngx_channel_t), log);
}


static ngx_int_t
ngx_http_push_stream_alert_worker_census_subscribers(ngx_pid_t pid, ngx_int_t slot, ngx_log_t *log)
{
    // seems ch doesn't need to have fd set. odd, but roll with it. pid and process slot also unnecessary.
    static ngx_channel_t    ch = {NGX_CMD_HTTP_PUSH_STREAM_CENSUS_SUBSCRIBERS, 0, 0, -1};

    return ngx_write_channel(ngx_http_push_stream_socketpairs[slot][0], &ch, sizeof(ngx_channel_t), log);
}


static ngx_inline void
ngx_http_push_stream_census_worker_subscribers(void)
{
    ngx_slab_pool_t                             *shpool = (ngx_slab_pool_t *) ngx_http_push_stream_shm_zone->shm.addr;
    ngx_http_push_stream_worker_data_t          *workers_data = ((ngx_http_push_stream_shm_data_t *) ngx_http_push_stream_shm_zone->data)->ipc;
    ngx_http_push_stream_worker_data_t          *thisworker_data = workers_data + ngx_process_slot;
    ngx_http_push_stream_worker_subscriber_t    *sentinel = thisworker_data->worker_subscribers_sentinel;

    ngx_http_push_stream_worker_subscriber_t     *cur, *next;


    ngx_shmtx_lock(&shpool->mutex);

    cur = (ngx_http_push_stream_worker_subscriber_t *) ngx_queue_next(&sentinel->queue);
    while (cur != sentinel) {
        next = (ngx_http_push_stream_worker_subscriber_t *) ngx_queue_next(&cur->queue);
        ngx_http_push_stream_subscription_t *cur_subscription, *sentinel_subscription;
        sentinel_subscription = cur->subscriptions_sentinel;
        cur_subscription = (ngx_http_push_stream_subscription_t *) ngx_queue_head(&sentinel_subscription->queue);
        while (cur_subscription != sentinel_subscription) {
            cur_subscription->channel->subscribers++;
            cur_subscription = (ngx_http_push_stream_subscription_t *) ngx_queue_next(&cur_subscription->queue);
        }
        cur = next;
    }

    ngx_shmtx_unlock(&shpool->mutex);
}


static ngx_inline void
ngx_http_push_stream_disconnect_worker_subscribers(ngx_flag_t force_disconnect)
{
    ngx_slab_pool_t                             *shpool = (ngx_slab_pool_t *) ngx_http_push_stream_shm_zone->shm.addr;
    ngx_http_push_stream_worker_data_t          *workers_data = ((ngx_http_push_stream_shm_data_t *) ngx_http_push_stream_shm_zone->data)->ipc;
    ngx_http_push_stream_worker_data_t          *thisworker_data = workers_data + ngx_process_slot;
    ngx_http_push_stream_worker_subscriber_t    *sentinel = thisworker_data->worker_subscribers_sentinel;

    ngx_http_push_stream_worker_subscriber_t     *cur, *next;


    ngx_shmtx_lock(&shpool->mutex);

    time_t now = ngx_time();
    cur =  (ngx_http_push_stream_worker_subscriber_t *) ngx_queue_next(&sentinel->queue);
    while (cur != sentinel) {
        next = (ngx_http_push_stream_worker_subscriber_t *) ngx_queue_next(&cur->queue);
        // in this block, nothing in shared memory should be dereferenced.
        ngx_http_request_t *r = cur->request;
        if (r != NULL) {
            if ((force_disconnect == 1) || ((cur->expires != 0) && (now > cur->expires))) {
                ngx_http_push_stream_worker_subscriber_cleanup_locked(cur);
                r->keepalive = 0;
                ngx_http_finalize_request(r, NGX_HTTP_OK);
            } else {
                break;
            }
        }
        cur = next;
    }

    ngx_shmtx_unlock(&shpool->mutex);
}


static ngx_inline void
ngx_http_push_stream_send_worker_ping_message(void)
{
    ngx_slab_pool_t                             *shpool = (ngx_slab_pool_t *) ngx_http_push_stream_shm_zone->shm.addr;
    ngx_http_push_stream_worker_data_t          *workers_data = ((ngx_http_push_stream_shm_data_t *) ngx_http_push_stream_shm_zone->data)->ipc;
    ngx_http_push_stream_worker_data_t          *thisworker_data = workers_data + ngx_process_slot;
    ngx_http_push_stream_worker_subscriber_t    *sentinel = thisworker_data->worker_subscribers_sentinel;

    // copy everything we need first
    ngx_chain_t                                 *chain;
    ngx_http_request_t                          *r;
    ngx_buf_t                                   *buffer;
    u_char                                      *pos;
    ngx_http_push_stream_worker_subscriber_t    *cur, *next;
    ngx_pool_t                                  *temp_pool;


    ngx_shmtx_lock(&shpool->mutex);

    if ((temp_pool = ngx_create_pool(NGX_CYCLE_POOL_SIZE, ngx_http_push_stream_pool->log)) == NULL) {
        ngx_shmtx_unlock(&shpool->mutex);
        ngx_log_error(NGX_LOG_ERR, ngx_http_push_stream_pool->log, 0, "push stream module: unable to allocate memory for temporary pool");
        return;
    }

    // preallocate output chain. yes, same one for every waiting subscriber
    if ((chain = ngx_http_push_stream_create_output_chain_locked(ngx_http_push_stream_ping_msg->buf, temp_pool, ngx_http_push_stream_pool->log, shpool)) == NULL) {
        ngx_shmtx_unlock(&shpool->mutex);
        ngx_log_error(NGX_LOG_ERR, ngx_cycle->log, 0, "push stream module: unable to create output chain while responding to several subscriber request");
        ngx_destroy_pool(temp_pool);
        return;
    }

    buffer = chain->buf;
    pos = buffer->pos;

    ngx_shmtx_unlock(&shpool->mutex);

    buffer->last_buf = 0;

    cur = (ngx_http_push_stream_worker_subscriber_t *) ngx_queue_next(&sentinel->queue);

    // now let's respond to some requests!
    while (cur != sentinel) {
        next = (ngx_http_push_stream_worker_subscriber_t *) ngx_queue_next(&cur->queue);

        // in this block, nothing in shared memory should be dereferenced.
        r = cur->request;
        if (r != NULL) {
            r->discard_body = 0; // hacky hacky!

            ngx_http_output_filter(r, chain);
            ngx_http_send_special(r, NGX_HTTP_FLUSH);

            // rewind the buffer, please
            buffer->pos = pos;
            buffer->last_buf = 0;
        }
        cur = next;
    }

    ngx_destroy_pool(temp_pool);
}


static ngx_inline void
ngx_http_push_stream_process_worker_message(void)
{
    ngx_http_push_stream_worker_msg_t      *prev_worker_msg, *worker_msg, *sentinel;
    const ngx_str_t                        *status_line = NULL;
    ngx_http_push_stream_channel_t         *channel;
    ngx_slab_pool_t                        *shpool = (ngx_slab_pool_t *) ngx_http_push_stream_shm_zone->shm.addr;
    ngx_http_push_stream_subscriber_t      *subscriber_sentinel;


    ngx_shmtx_lock(&shpool->mutex);


    ngx_http_push_stream_worker_data_t     *workers_data = ((ngx_http_push_stream_shm_data_t *) ngx_http_push_stream_shm_zone->data)->ipc;
    ngx_http_push_stream_worker_data_t     *thisworker_data = workers_data + ngx_process_slot;
    ngx_int_t                               status_code;
    ngx_http_push_stream_msg_t             *msg;


    sentinel = (ngx_http_push_stream_worker_msg_t *) &thisworker_data->messages_queue;
    worker_msg = (ngx_http_push_stream_worker_msg_t *) ngx_queue_next(&sentinel->queue);
    while (worker_msg != sentinel) {
        if (worker_msg->pid == ngx_pid) {
            // everything is okay
            status_code = worker_msg->status_code;
            msg = worker_msg->msg;
            channel = worker_msg->channel;
            subscriber_sentinel = worker_msg->subscriber_sentinel;
            if (msg == NULL) {
                // just a status line, is all
                // status code only
                switch (status_code) {
                    case NGX_HTTP_CONFLICT:
                        status_line = &NGX_HTTP_PUSH_STREAM_HTTP_STATUS_409;
                        break;

                    case NGX_HTTP_GONE:
                        status_line = &NGX_HTTP_PUSH_STREAM_HTTP_STATUS_410;
                        break;

                    case 0:
                        ngx_shmtx_unlock(&shpool->mutex);
                        ngx_log_error(NGX_LOG_ERR, ngx_cycle->log, 0, "push stream module: worker message contains neither a channel message nor a status code");
                        ngx_shmtx_lock(&shpool->mutex);
                        // let's let the subscribers know that something went wrong and they might've missed a message
                        status_code = NGX_HTTP_INTERNAL_SERVER_ERROR;
                        // intentional fall-through

                    default:
                        status_line = NULL;
                }
            }
            ngx_shmtx_unlock(&shpool->mutex);
            ngx_http_push_stream_respond_to_subscribers(channel, subscriber_sentinel, msg, status_code, status_line);
            ngx_shmtx_lock(&shpool->mutex);
        } else {
            // that's quite bad you see. a previous worker died with an undelivered message.
            // but all its subscribers' connections presumably got canned, too. so it's not so bad after all.

            ngx_http_push_stream_pid_queue_t     *channel_worker_sentinel = &worker_msg->channel->workers_with_subscribers;
            ngx_http_push_stream_pid_queue_t     *channel_worker_cur = channel_worker_sentinel;

            ngx_shmtx_unlock(&shpool->mutex);
            ngx_log_error(NGX_LOG_ERR, ngx_cycle->log, 0, "push stream module: worker %i intercepted a message intended for another worker process (%i) that probably died", ngx_pid, worker_msg->pid);
            ngx_shmtx_lock(&shpool->mutex);

            // delete that invalid sucker
            while ((channel_worker_cur = (ngx_http_push_stream_pid_queue_t *) ngx_queue_next(&channel_worker_cur->queue)) != channel_worker_sentinel) {
                if (channel_worker_cur->pid == worker_msg->pid) {
                    ngx_queue_remove(&channel_worker_cur->queue);
                    ngx_slab_free_locked(shpool, channel_worker_cur->subscriber_sentinel);
                    ngx_slab_free_locked(shpool, channel_worker_cur);
                    break;
                }
            }
        }
        // It may be worth it to memzero worker_msg for debugging purposes.
        prev_worker_msg = worker_msg;
        worker_msg = (ngx_http_push_stream_worker_msg_t *) ngx_queue_next(&worker_msg->queue);
        ngx_slab_free_locked(shpool, prev_worker_msg);
    }

    ngx_queue_init(&thisworker_data->messages_queue); // reset the worker message sentinel

    ngx_shmtx_unlock(&shpool->mutex);

    return;
}


static ngx_int_t
ngx_http_push_stream_send_worker_message(ngx_http_push_stream_channel_t *channel, ngx_http_push_stream_subscriber_t *subscriber_sentinel, ngx_pid_t pid, ngx_int_t worker_slot, ngx_http_push_stream_msg_t *msg, ngx_int_t status_code, ngx_log_t *log)
{
    ngx_slab_pool_t                         *shpool = (ngx_slab_pool_t *) ngx_http_push_stream_shm_zone->shm.addr;
    ngx_http_push_stream_worker_data_t      *workers_data = ((ngx_http_push_stream_shm_data_t *) ngx_http_push_stream_shm_zone->data)->ipc;
    ngx_http_push_stream_worker_data_t      *thisworker_data = workers_data + worker_slot;
    ngx_http_push_stream_worker_msg_t       *newmessage;


    ngx_shmtx_lock(&shpool->mutex);

    if ((newmessage = ngx_slab_alloc_locked(shpool, sizeof(*newmessage))) == NULL) {
        ngx_shmtx_unlock(&shpool->mutex);
        ngx_log_error(NGX_LOG_ERR, log, 0, "push stream module: unable to allocate worker message");
        return NGX_ERROR;
    }

    ngx_queue_insert_tail(&thisworker_data->messages_queue, &newmessage->queue);
    newmessage->msg = msg;
    newmessage->status_code = status_code;
    newmessage->pid = pid;
    newmessage->subscriber_sentinel = subscriber_sentinel;
    newmessage->channel = channel;

    ngx_shmtx_unlock(&shpool->mutex);

    return NGX_OK;
}
