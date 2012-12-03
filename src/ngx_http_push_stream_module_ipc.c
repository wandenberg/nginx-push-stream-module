/*
 * This file is distributed under the MIT License.
 *
 * Copyright (c) 2009 Leo Ponomarev
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 *
 * ngx_http_push_stream_module_ipc.c
 *
 * Modified: Oct 26, 2010
 * Modifications by: Wandenberg Peixoto <wandenberg@gmail.com>, Rogério Carvalho Schneider <stockrt@gmail.com>
 */

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
ngx_http_push_stream_ipc_init_worker()
{
    ngx_slab_pool_t                        *shpool = (ngx_slab_pool_t *) ngx_http_push_stream_shm_zone->shm.addr;
    ngx_http_push_stream_shm_data_t        *data = (ngx_http_push_stream_shm_data_t *) ngx_http_push_stream_shm_zone->data;
    int                                     i;

    // cleanning old content if worker die and another one is set on same slot
    ngx_http_push_stream_clean_worker_data();

    ngx_shmtx_lock(&shpool->mutex);

    if ((data->ipc[ngx_process_slot].messages_queue == NULL) && ((data->ipc[ngx_process_slot].messages_queue = ngx_slab_alloc_locked(shpool, sizeof(ngx_http_push_stream_worker_msg_t))) == NULL)) {
        ngx_shmtx_unlock(&shpool->mutex);
        return NGX_ERROR;
    }

    if ((data->ipc[ngx_process_slot].subscribers_sentinel == NULL) && ((data->ipc[ngx_process_slot].subscribers_sentinel = ngx_slab_alloc_locked(shpool, sizeof(ngx_http_push_stream_queue_elem_t))) == NULL)) {
        ngx_shmtx_unlock(&shpool->mutex);
        return NGX_ERROR;
    }

    data->ipc[ngx_process_slot].pid = ngx_pid;
    data->ipc[ngx_process_slot].startup = ngx_time();
    ngx_queue_init(&data->ipc[ngx_process_slot].messages_queue->queue);
    ngx_queue_init(&data->ipc[ngx_process_slot].subscribers_sentinel->queue);

    data->subscribers = 0;
    ngx_http_push_stream_walk_rbtree(ngx_http_push_stream_reset_channel_subscribers_count_locked);

    ngx_shmtx_unlock(&shpool->mutex);

    for(i = 0; i < NGX_MAX_PROCESSES; i++) {
        if (data->ipc[i].pid > 0) {
            data->ipc[i].subscribers = 0;
            ngx_http_push_stream_alert_worker_census_subscribers(ngx_pid, i, ngx_cycle->log);
        }
    }

    return NGX_OK;
}


static ngx_int_t
ngx_http_push_stream_unsubscribe_worker_locked(ngx_http_push_stream_channel_t *channel, ngx_slab_pool_t *shpool)
{
    ngx_http_push_stream_pid_queue_t        *sentinel = &channel->workers_with_subscribers;
    ngx_http_push_stream_pid_queue_t        *cur = sentinel;

    while ((cur = (ngx_http_push_stream_pid_queue_t *) ngx_queue_next(&cur->queue)) != sentinel) {
        if ((cur->pid == ngx_pid) || (cur->slot == ngx_process_slot)) {
            ngx_queue_remove(&cur->queue);
            ngx_slab_free_locked(shpool, cur);
            break;
        }
    }

    return NGX_OK;
}


static void
ngx_http_push_stream_clean_worker_data()
{
    ngx_slab_pool_t                        *shpool = (ngx_slab_pool_t *) ngx_http_push_stream_shm_zone->shm.addr;
    ngx_http_push_stream_shm_data_t        *data = (ngx_http_push_stream_shm_data_t *) ngx_http_push_stream_shm_zone->data;

    ngx_http_push_stream_worker_msg_t      *cur_msg;

    ngx_shmtx_lock(&shpool->mutex);
    if (data->ipc[ngx_process_slot].messages_queue != NULL) {
        while ((cur_msg = (ngx_http_push_stream_worker_msg_t *) ngx_queue_next(&data->ipc[ngx_process_slot].messages_queue->queue)) != data->ipc[ngx_process_slot].messages_queue) {
            ngx_http_push_stream_free_worker_message_memory_locked(shpool, cur_msg);
        }
    }

    if (data->ipc[ngx_process_slot].subscribers_sentinel != NULL) {
        ngx_queue_init(&data->ipc[ngx_process_slot].subscribers_sentinel->queue);
    }

    ngx_http_push_stream_walk_rbtree(ngx_http_push_stream_unsubscribe_worker_locked);

    ngx_shmtx_unlock(&shpool->mutex);

    data->ipc[ngx_process_slot].pid = -1;
    data->ipc[ngx_process_slot].subscribers = 0;
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
        } else if (ch.command == NGX_CMD_HTTP_PUSH_STREAM_CENSUS_SUBSCRIBERS.command) {
            ngx_http_push_stream_census_worker_subscribers();
        } else if (ch.command == NGX_CMD_HTTP_PUSH_STREAM_DELETE_CHANNEL.command) {
            ngx_http_push_stream_delete_worker_channel();
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
    ngx_http_push_stream_queue_elem_t           *cur;
    ngx_http_push_stream_subscription_t         *cur_subscription;

    ngx_shmtx_lock(&shpool->mutex);

    cur = thisworker_data->subscribers_sentinel;
    while ((cur = (ngx_http_push_stream_queue_elem_t *) ngx_queue_next(&cur->queue)) != thisworker_data->subscribers_sentinel) {
        ngx_http_push_stream_subscriber_t *subscriber = (ngx_http_push_stream_subscriber_t *) cur->value;
        cur_subscription = &subscriber->subscriptions_sentinel;
        while ((cur_subscription = (ngx_http_push_stream_subscription_t *) ngx_queue_next(&cur_subscription->queue)) != &subscriber->subscriptions_sentinel) {
            cur_subscription->channel->subscribers++;
        }
        data->subscribers++;
        thisworker_data->subscribers++;
    }

    ngx_shmtx_unlock(&shpool->mutex);
}


static ngx_inline void
ngx_http_push_stream_process_worker_message(void)
{
    ngx_http_push_stream_worker_msg_t      *worker_msg, *sentinel;
    ngx_slab_pool_t                        *shpool = (ngx_slab_pool_t *) ngx_http_push_stream_shm_zone->shm.addr;
    ngx_http_push_stream_worker_data_t     *workers_data = ((ngx_http_push_stream_shm_data_t *) ngx_http_push_stream_shm_zone->data)->ipc;
    ngx_http_push_stream_worker_data_t     *thisworker_data = workers_data + ngx_process_slot;


    sentinel = thisworker_data->messages_queue;
    while ((worker_msg = (ngx_http_push_stream_worker_msg_t *) ngx_queue_next(&sentinel->queue)) != sentinel) {
        if (worker_msg->pid == ngx_pid) {
            // everything is okay
            ngx_http_push_stream_respond_to_subscribers(worker_msg->channel, worker_msg->subscribers_sentinel, worker_msg->msg);
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

        // free worker_msg already sent
        ngx_shmtx_lock(&shpool->mutex);
        ngx_http_push_stream_free_worker_message_memory_locked(shpool, worker_msg);
        ngx_shmtx_unlock(&shpool->mutex);
    }
}


static ngx_int_t
ngx_http_push_stream_send_worker_message_locked(ngx_http_push_stream_channel_t *channel, ngx_http_push_stream_queue_elem_t *subscribers_sentinel, ngx_pid_t pid, ngx_int_t worker_slot, ngx_http_push_stream_msg_t *msg, ngx_flag_t *queue_was_empty, ngx_log_t *log)
{
    ngx_slab_pool_t                         *shpool = (ngx_slab_pool_t *) ngx_http_push_stream_shm_zone->shm.addr;
    ngx_http_push_stream_worker_data_t      *workers_data = ((ngx_http_push_stream_shm_data_t *) ngx_http_push_stream_shm_zone->data)->ipc;
    ngx_http_push_stream_worker_data_t      *thisworker_data = workers_data + worker_slot;
    ngx_http_push_stream_worker_msg_t       *newmessage;

    if ((newmessage = ngx_slab_alloc_locked(shpool, sizeof(ngx_http_push_stream_worker_msg_t))) == NULL) {
        ngx_log_error(NGX_LOG_ERR, log, 0, "push stream module: unable to allocate worker message, pid: %P, slot: %d", pid, worker_slot);
        return NGX_ERROR;
    }

    msg->workers_ref_count++;
    newmessage->msg = msg;
    newmessage->pid = pid;
    newmessage->subscribers_sentinel = subscribers_sentinel;
    newmessage->channel = channel;
    *queue_was_empty = ngx_queue_empty(&thisworker_data->messages_queue->queue);
    ngx_queue_insert_tail(&thisworker_data->messages_queue->queue, &newmessage->queue);

    return NGX_OK;
}


static void
ngx_http_push_stream_broadcast(ngx_http_push_stream_channel_t *channel, ngx_http_push_stream_msg_t *msg, ngx_log_t *log)
{
    // subscribers are queued up in a local pool. Queue heads, however, are located
    // in shared memory, identified by pid.
    ngx_http_push_stream_pid_queue_t        *sentinel = &channel->workers_with_subscribers;
    ngx_http_push_stream_pid_queue_t        *cur = sentinel;
    ngx_slab_pool_t                         *shpool = (ngx_slab_pool_t *) ngx_http_push_stream_shm_zone->shm.addr;
    ngx_flag_t                               queue_was_empty[NGX_MAX_PROCESSES];

    ngx_shmtx_lock(&shpool->mutex);
    while ((cur = (ngx_http_push_stream_pid_queue_t *) ngx_queue_next(&cur->queue)) != sentinel) {
        ngx_http_push_stream_send_worker_message_locked(channel, &cur->subscribers_sentinel, cur->pid, cur->slot, msg, &queue_was_empty[cur->slot], log);
    }
    ngx_shmtx_unlock(&shpool->mutex);

    cur = sentinel;
    while ((cur = (ngx_http_push_stream_pid_queue_t *) ngx_queue_next(&cur->queue)) != sentinel) {
        // interprocess communication breakdown
        if (queue_was_empty[cur->slot] && (ngx_http_push_stream_alert_worker_check_messages(cur->pid, cur->slot, log) != NGX_OK)) {
            ngx_log_error(NGX_LOG_ERR, log, 0, "push stream module: error communicating with worker process, pid: %P, slot: %d", cur->pid, cur->slot);
        }
    }

    if ((msg->queue.prev == NULL) && (msg->queue.next == NULL)) {
        ngx_shmtx_lock(&shpool->mutex);
        ngx_http_push_stream_mark_message_to_delete_locked(msg);
        ngx_shmtx_unlock(&shpool->mutex);
    }
}

static ngx_int_t
ngx_http_push_stream_respond_to_subscribers(ngx_http_push_stream_channel_t *channel, ngx_http_push_stream_queue_elem_t *subscribers_sentinel, ngx_http_push_stream_msg_t *msg)
{
    ngx_http_push_stream_queue_elem_t      *cur = subscribers_sentinel;

    if (subscribers_sentinel == NULL) {
        return NGX_ERROR;
    }

    if (msg != NULL) {

        // now let's respond to some requests!
        while ((cur = (ngx_http_push_stream_queue_elem_t *) ngx_queue_next(&cur->queue)) != subscribers_sentinel) {
            ngx_http_push_stream_subscriber_t *subscriber = (ngx_http_push_stream_subscriber_t *) cur->value;
            if (subscriber->longpolling) {
                ngx_http_push_stream_queue_elem_t *prev = (ngx_http_push_stream_queue_elem_t *) ngx_queue_prev(&cur->queue);

                ngx_http_push_stream_add_response_header(subscriber->request, &NGX_HTTP_PUSH_STREAM_HEADER_TRANSFER_ENCODING, &NGX_HTTP_PUSH_STREAM_HEADER_CHUNCKED);
                ngx_http_push_stream_add_polling_headers(subscriber->request, msg->time, msg->tag, subscriber->request->pool);
                ngx_http_send_header(subscriber->request);

                ngx_http_push_stream_send_response_content_header(subscriber->request, ngx_http_get_module_loc_conf(subscriber->request, ngx_http_push_stream_module));
                ngx_http_push_stream_send_response_message(subscriber->request, channel, msg, 1, 0);
                ngx_http_push_stream_send_response_finalize(subscriber->request);

                cur = prev;
            } else {
                if (ngx_http_push_stream_send_response_message(subscriber->request, channel, msg, 0, 0) != NGX_OK) {
                    ngx_http_push_stream_queue_elem_t *prev = (ngx_http_push_stream_queue_elem_t *) ngx_queue_prev(&cur->queue);
                    ngx_http_push_stream_send_response_finalize(subscriber->request);
                    cur = prev;
                } else {
                    ngx_http_push_stream_subscriber_ctx_t *ctx = ngx_http_get_module_ctx(subscriber->request, ngx_http_push_stream_module);
                    ngx_http_push_stream_loc_conf_t       *pslcf = ngx_http_get_module_loc_conf(subscriber->request, ngx_http_push_stream_module);
                    ngx_http_push_stream_timer_reset(pslcf->ping_message_interval, ctx->ping_timer);
                }
            }
        }
    }

    return NGX_OK;
}
