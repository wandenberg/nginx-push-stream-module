#include <ngx_http_push_stream_module_ipc.h>


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
    ngx_http_push_stream_shm_data_t        *data = (ngx_http_push_stream_shm_data_t *) ngx_http_push_stream_shm_zone->data;
    ngx_http_push_stream_worker_data_t     *workers_data;
    int                                     i;


    ngx_shmtx_lock(&shpool->mutex);

    if (data->ipc != NULL) {
        // already initialized... reset channel subscribers counters and census subscribers
        ngx_http_push_stream_worker_data_t          *worker_data = NULL;
        ngx_http_push_stream_worker_data_t          *thisworker_data = data->ipc + ngx_process_slot;
        ngx_http_push_stream_worker_subscriber_t    *sentinel = &thisworker_data->worker_subscribers_sentinel;

        ngx_queue_init(&sentinel->queue);

        for(i=0; i<workers; i++) {
            worker_data = data->ipc + i;
            worker_data->subscribers = 0;
        }
        data->subscribers = 0;
        ngx_http_push_stream_walk_rbtree(ngx_http_push_stream_reset_channel_subscribers_count_locked);

        ngx_shmtx_unlock(&shpool->mutex);

        for(i=0; i<workers; i++) {
            ngx_http_push_stream_alert_worker_census_subscribers(ngx_pid, i, ngx_cycle->log);
        }

        return NGX_OK;
    }

    // initialize worker message queues
    if ((workers_data = ngx_slab_alloc_locked(shpool, sizeof(ngx_http_push_stream_worker_data_t)*workers)) == NULL) {
        ngx_shmtx_unlock(&shpool->mutex);
        return NGX_ERROR;
    }

    for(i=0; i<workers; i++) {
        ngx_queue_init(&workers_data[i].messages_queue.queue);
        ngx_queue_init(&workers_data[i].worker_subscribers_sentinel.queue);
    }

    data->ipc = workers_data;
    ngx_queue_init(&data->messages_to_delete.queue);

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

        if (ch.command == NGX_CMD_HTTP_PUSH_STREAM_CHECK_MESSAGES.command) {
            ngx_http_push_stream_process_worker_message();
        } else if (ch.command == NGX_CMD_HTTP_PUSH_STREAM_SEND_PING.command) {
            ngx_http_push_stream_send_worker_ping_message();
        } else if (ch.command == NGX_CMD_HTTP_PUSH_STREAM_DISCONNECT_SUBSCRIBERS.command) {
            // disconnect only expired subscribers (force_disconnect = 0)
            ngx_http_push_stream_disconnect_worker_subscribers(0);
        } else if (ch.command == NGX_CMD_HTTP_PUSH_STREAM_CENSUS_SUBSCRIBERS.command) {
            ngx_http_push_stream_census_worker_subscribers();
        }
    }
}


static ngx_int_t
ngx_http_push_stream_alert_worker(ngx_pid_t pid, ngx_int_t slot, ngx_log_t *log, ngx_channel_t command)
{
    return ngx_write_channel(ngx_http_push_stream_socketpairs[slot][0], &command, sizeof(ngx_channel_t), log);
}


static ngx_inline void
ngx_http_push_stream_census_worker_subscribers(void)
{
    ngx_slab_pool_t                             *shpool = (ngx_slab_pool_t *) ngx_http_push_stream_shm_zone->shm.addr;
    ngx_http_push_stream_shm_data_t             *data = (ngx_http_push_stream_shm_data_t *) ngx_http_push_stream_shm_zone->data;
    ngx_http_push_stream_worker_data_t          *workers_data = data->ipc;
    ngx_http_push_stream_worker_data_t          *thisworker_data = workers_data + ngx_process_slot;
    ngx_http_push_stream_worker_subscriber_t    *cur, *sentinel;
    ngx_http_push_stream_subscription_t         *cur_subscription, *sentinel_subscription;

    ngx_shmtx_lock(&shpool->mutex);

    sentinel = &thisworker_data->worker_subscribers_sentinel;
    cur = sentinel;
    while ((cur = (ngx_http_push_stream_worker_subscriber_t *) ngx_queue_next(&cur->queue)) != sentinel) {
        sentinel_subscription = &cur->subscriptions_sentinel;
        cur_subscription = sentinel_subscription;
        while ((cur_subscription = (ngx_http_push_stream_subscription_t *) ngx_queue_next(&cur_subscription->queue)) != sentinel_subscription) {
            cur_subscription->channel->subscribers++;
        }
        data->subscribers++;
        thisworker_data->subscribers++;
    }

    ngx_shmtx_unlock(&shpool->mutex);
}


static ngx_inline void
ngx_http_push_stream_disconnect_worker_subscribers(ngx_flag_t force_disconnect)
{
    ngx_http_push_stream_worker_data_t          *workers_data = ((ngx_http_push_stream_shm_data_t *) ngx_http_push_stream_shm_zone->data)->ipc;
    ngx_http_push_stream_worker_data_t          *thisworker_data = workers_data + ngx_process_slot;
    ngx_http_push_stream_worker_subscriber_t    *sentinel = &thisworker_data->worker_subscribers_sentinel;

    ngx_http_push_stream_worker_subscriber_t     *cur = sentinel;

    time_t now = ngx_time();

    while ((cur =  (ngx_http_push_stream_worker_subscriber_t *) ngx_queue_next(&sentinel->queue)) != sentinel) {
        if ((cur->request != NULL) && ((force_disconnect == 1) || ((cur->expires != 0) && (now > cur->expires)))) {
            ngx_http_push_stream_worker_subscriber_cleanup(cur);
            cur->request->keepalive = 0;
            ngx_http_send_special(cur->request, NGX_HTTP_LAST | NGX_HTTP_FLUSH);
            ngx_http_finalize_request(cur->request, NGX_HTTP_OK);
        } else {
            break;
        }
    }
}


static ngx_inline void
ngx_http_push_stream_send_worker_ping_message(void)
{
    ngx_http_push_stream_worker_data_t          *workers_data = ((ngx_http_push_stream_shm_data_t *) ngx_http_push_stream_shm_zone->data)->ipc;
    ngx_http_push_stream_worker_data_t          *thisworker_data = workers_data + ngx_process_slot;
    ngx_http_push_stream_worker_subscriber_t    *sentinel = &thisworker_data->worker_subscribers_sentinel;
    ngx_http_push_stream_worker_subscriber_t    *cur = sentinel;

    if ((ngx_http_push_stream_ping_buf != NULL) && (!ngx_queue_empty(&sentinel->queue))) {
        uint len = ngx_buf_size(ngx_http_push_stream_ping_buf);
        while ((cur = (ngx_http_push_stream_worker_subscriber_t *) ngx_queue_next(&cur->queue)) != sentinel) {
            if (cur->request != NULL) {
                ngx_http_push_stream_send_response_chunk(cur->request, ngx_http_push_stream_ping_buf->pos, len, 0);
            }
        }
    }
}


static ngx_inline void
ngx_http_push_stream_process_worker_message(void)
{
    ngx_http_push_stream_worker_msg_t      *prev_worker_msg, *worker_msg, *sentinel;
    ngx_slab_pool_t                        *shpool = (ngx_slab_pool_t *) ngx_http_push_stream_shm_zone->shm.addr;
    ngx_http_push_stream_worker_data_t     *workers_data = ((ngx_http_push_stream_shm_data_t *) ngx_http_push_stream_shm_zone->data)->ipc;
    ngx_http_push_stream_worker_data_t     *thisworker_data = workers_data + ngx_process_slot;


    sentinel = (ngx_http_push_stream_worker_msg_t *) &thisworker_data->messages_queue;
    worker_msg = (ngx_http_push_stream_worker_msg_t *) ngx_queue_next(&sentinel->queue);
    while (worker_msg != sentinel) {
        if (worker_msg->pid == ngx_pid) {
            // everything is okay
            ngx_http_push_stream_respond_to_subscribers(worker_msg->channel, worker_msg->subscriber_sentinel, worker_msg->msg);
        } else {
            // that's quite bad you see. a previous worker died with an undelivered message.
            // but all its subscribers' connections presumably got canned, too. so it's not so bad after all.

            ngx_http_push_stream_pid_queue_t     *channel_worker_sentinel = &worker_msg->channel->workers_with_subscribers;
            ngx_http_push_stream_pid_queue_t     *channel_worker_cur = channel_worker_sentinel;

            ngx_log_error(NGX_LOG_ERR, ngx_cycle->log, 0, "push stream module: worker %i intercepted a message intended for another worker process (%i) that probably died", ngx_pid, worker_msg->pid);

            // delete that invalid sucker
            while ((channel_worker_cur != NULL) && (channel_worker_cur = (ngx_http_push_stream_pid_queue_t *) ngx_queue_next(&channel_worker_cur->queue)) != channel_worker_sentinel) {
                if (channel_worker_cur->pid == worker_msg->pid) {
                    ngx_log_error(NGX_LOG_INFO, ngx_cycle->log, 0, "push stream module: reference to worker %i will be removed", worker_msg->pid);
                    ngx_shmtx_lock(&shpool->mutex);
                    ngx_queue_remove(&channel_worker_cur->queue);
                    ngx_slab_free_locked(shpool, channel_worker_cur);
                    ngx_shmtx_unlock(&shpool->mutex);
                    channel_worker_cur = NULL;
                    break;
                }
            }
        }

        prev_worker_msg = worker_msg;
        worker_msg = (ngx_http_push_stream_worker_msg_t *) ngx_queue_next(&worker_msg->queue);

        // free worker_msg already sent
        ngx_shmtx_lock(&shpool->mutex);
        ngx_queue_remove(&prev_worker_msg->queue);
        ngx_slab_free_locked(shpool, prev_worker_msg);
        ngx_shmtx_unlock(&shpool->mutex);
    }
}


static ngx_int_t
ngx_http_push_stream_send_worker_message(ngx_http_push_stream_channel_t *channel, ngx_http_push_stream_subscriber_t *subscriber_sentinel, ngx_pid_t pid, ngx_int_t worker_slot, ngx_http_push_stream_msg_t *msg, ngx_log_t *log)
{
    ngx_slab_pool_t                         *shpool = (ngx_slab_pool_t *) ngx_http_push_stream_shm_zone->shm.addr;
    ngx_http_push_stream_worker_data_t      *workers_data = ((ngx_http_push_stream_shm_data_t *) ngx_http_push_stream_shm_zone->data)->ipc;
    ngx_http_push_stream_worker_data_t      *thisworker_data = workers_data + worker_slot;
    ngx_http_push_stream_worker_msg_t       *newmessage;


    ngx_shmtx_lock(&shpool->mutex);

    if ((newmessage = ngx_slab_alloc_locked(shpool, sizeof(ngx_http_push_stream_worker_msg_t))) == NULL) {
        ngx_shmtx_unlock(&shpool->mutex);
        ngx_log_error(NGX_LOG_ERR, log, 0, "push stream module: unable to allocate worker message");
        return NGX_ERROR;
    }

    newmessage->msg = msg;
    newmessage->pid = pid;
    newmessage->subscriber_sentinel = subscriber_sentinel;
    newmessage->channel = channel;
    ngx_queue_insert_tail(&thisworker_data->messages_queue.queue, &newmessage->queue);

    ngx_shmtx_unlock(&shpool->mutex);

    return NGX_OK;
}


static void
ngx_http_push_stream_broadcast(ngx_http_push_stream_channel_t *channel, ngx_http_push_stream_msg_t *msg, ngx_log_t *log)
{
    // subscribers are queued up in a local pool. Queue heads, however, are located
    // in shared memory, identified by pid.
    ngx_http_push_stream_pid_queue_t        *sentinel = &channel->workers_with_subscribers;
    ngx_http_push_stream_pid_queue_t        *cur = sentinel;

    while ((cur = (ngx_http_push_stream_pid_queue_t *) ngx_queue_next(&cur->queue)) != sentinel) {
        pid_t                                   worker_pid  = cur->pid;
        ngx_int_t                               worker_slot = cur->slot;
        ngx_http_push_stream_subscriber_t      *subscriber_sentinel = &cur->subscriber_sentinel;

        // interprocess communication breakdown
        if (ngx_http_push_stream_send_worker_message(channel, subscriber_sentinel, worker_pid, worker_slot, msg, log) != NGX_ERROR) {
            ngx_http_push_stream_alert_worker_check_messages(worker_pid, worker_slot, log);
        } else {
            ngx_log_error(NGX_LOG_ERR, log, 0, "push stream module: error communicating with some other worker process");
        }
    }
}

static ngx_int_t
ngx_http_push_stream_respond_to_subscribers(ngx_http_push_stream_channel_t *channel, ngx_http_push_stream_subscriber_t *sentinel, ngx_http_push_stream_msg_t *msg)
{
    ngx_http_push_stream_subscriber_t      *cur;

    if (sentinel == NULL) {
        return NGX_ERROR;
    }

    cur = sentinel;

    if (msg != NULL) {
        uint len = ngx_buf_size(msg->buf);

        // now let's respond to some requests!
        while ((cur = (ngx_http_push_stream_subscriber_t *) ngx_queue_next(&cur->queue)) != sentinel) {
            ngx_http_push_stream_send_response_chunk(cur->request, msg->buf->pos, len, 0);
        }
    }

    return NGX_OK;
}
