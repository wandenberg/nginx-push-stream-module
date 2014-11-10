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
 * ngx_http_push_stream_module_ipc.h
 *
 * Modified: Oct 26, 2010
 * Modifications by: Wandenberg Peixoto <wandenberg@gmail.com>, Rogério Carvalho Schneider <stockrt@gmail.com>
 */

#ifndef NGX_HTTP_PUSH_STREAM_MODULE_IPC_H_
#define NGX_HTTP_PUSH_STREAM_MODULE_IPC_H_

#include <ngx_http_push_stream_module.h>
#include <ngx_http_push_stream_module_subscriber.h>

#include <ngx_channel.h>

// constants
static ngx_channel_t NGX_CMD_HTTP_PUSH_STREAM_CHECK_MESSAGES = {49, 0, 0, -1};
static ngx_channel_t NGX_CMD_HTTP_PUSH_STREAM_CENSUS_SUBSCRIBERS = {50, 0, 0, -1};
static ngx_channel_t NGX_CMD_HTTP_PUSH_STREAM_DELETE_CHANNEL = {51, 0, 0, -1};
static ngx_channel_t NGX_CMD_HTTP_PUSH_STREAM_CLEANUP_SHUTTING_DOWN = {52, 0, 0, -1};

// worker processes of the world, unite.
ngx_socket_t    ngx_http_push_stream_socketpairs[NGX_MAX_PROCESSES][2];

static ngx_int_t    ngx_http_push_stream_register_worker_message_handler(ngx_cycle_t *cycle);

static void    ngx_http_push_stream_broadcast(ngx_http_push_stream_channel_t *channel, ngx_http_push_stream_msg_t *msg, ngx_log_t *log, ngx_http_push_stream_main_conf_t *mcf);

static ngx_int_t        ngx_http_push_stream_alert_worker(ngx_pid_t pid, ngx_int_t slot, ngx_log_t *log, ngx_channel_t command);
#define ngx_http_push_stream_alert_worker_check_messages(pid, slot, log) ngx_http_push_stream_alert_worker(pid, slot, log, NGX_CMD_HTTP_PUSH_STREAM_CHECK_MESSAGES)
#define ngx_http_push_stream_alert_worker_census_subscribers(pid, slot, log) ngx_http_push_stream_alert_worker(pid, slot, log, NGX_CMD_HTTP_PUSH_STREAM_CENSUS_SUBSCRIBERS)
#define ngx_http_push_stream_alert_worker_delete_channel(pid, slot, log) ngx_http_push_stream_alert_worker(pid, slot, log, NGX_CMD_HTTP_PUSH_STREAM_DELETE_CHANNEL)
#define ngx_http_push_stream_alert_worker_shutting_down_cleanup(pid, slot, log) ngx_http_push_stream_alert_worker(pid, slot, log, NGX_CMD_HTTP_PUSH_STREAM_CLEANUP_SHUTTING_DOWN)

static ngx_int_t        ngx_http_push_stream_send_worker_message(ngx_http_push_stream_channel_t *channel, ngx_queue_t *subscriptions_sentinel, ngx_pid_t pid, ngx_int_t worker_slot, ngx_http_push_stream_msg_t *msg, ngx_flag_t *queue_was_empty, ngx_log_t *log, ngx_http_push_stream_main_conf_t *mcf);

static ngx_int_t        ngx_http_push_stream_init_ipc(ngx_cycle_t *cycle, ngx_int_t workers);
static void             ngx_http_push_stream_ipc_exit_worker(ngx_cycle_t *cycle);
static ngx_int_t        ngx_http_push_stream_ipc_init_worker(void);
static void             ngx_http_push_stream_clean_worker_data(ngx_http_push_stream_shm_data_t *data);
static void             ngx_http_push_stream_channel_handler(ngx_event_t *ev);
static void             ngx_http_push_stream_alert_shutting_down_workers(void);


static ngx_inline void  ngx_http_push_stream_process_worker_message(void);
static ngx_inline void  ngx_http_push_stream_census_worker_subscribers(void);
static ngx_inline void  ngx_http_push_stream_cleanup_shutting_down_worker(void);

static ngx_int_t    ngx_http_push_stream_respond_to_subscribers(ngx_http_push_stream_channel_t *channel, ngx_queue_t *subscriptions, ngx_http_push_stream_msg_t *msg);

#endif /* NGX_HTTP_PUSH_STREAM_MODULE_IPC_H_ */
