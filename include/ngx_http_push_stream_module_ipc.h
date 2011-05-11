/*
 * Copyright (C) 2010-2011 Wandenberg Peixoto <wandenberg@gmail.com>, Rogério Carvalho Schneider <stockrt@gmail.com>
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
 * ngx_http_push_stream_module_ipc.h
 *
 * Created: Oct 26, 2010
 * Authors: Wandenberg Peixoto <wandenberg@gmail.com>, Rogério Carvalho Schneider <stockrt@gmail.com>
 */

#ifndef NGX_HTTP_PUSH_STREAM_MODULE_IPC_H_
#define NGX_HTTP_PUSH_STREAM_MODULE_IPC_H_

#include <ngx_http_push_stream_module.h>
#include <ngx_http_push_stream_module_subscriber.h>

#include <ngx_channel.h>

// constants
static ngx_channel_t NGX_CMD_HTTP_PUSH_STREAM_CHECK_MESSAGES = {49, 0, 0, -1};
static ngx_channel_t NGX_CMD_HTTP_PUSH_STREAM_SEND_PING = {50, 0, 0, -1};
static ngx_channel_t NGX_CMD_HTTP_PUSH_STREAM_DISCONNECT_SUBSCRIBERS = {51, 0, 0, -1};
static ngx_channel_t NGX_CMD_HTTP_PUSH_STREAM_CENSUS_SUBSCRIBERS = {52, 0, 0, -1};

// worker processes of the world, unite.
ngx_socket_t    ngx_http_push_stream_socketpairs[NGX_MAX_PROCESSES][2];

static void    ngx_http_push_stream_broadcast(ngx_http_push_stream_channel_t *channel, ngx_http_push_stream_msg_t *msg, ngx_log_t *log);

static ngx_int_t        ngx_http_push_stream_alert_worker(ngx_pid_t pid, ngx_int_t slot, ngx_log_t *log, ngx_channel_t command);
#define ngx_http_push_stream_alert_worker_check_messages(pid, slot, log) ngx_http_push_stream_alert_worker(pid, slot, log, NGX_CMD_HTTP_PUSH_STREAM_CHECK_MESSAGES);
#define ngx_http_push_stream_alert_worker_send_ping(pid, slot, log) ngx_http_push_stream_alert_worker(pid, slot, log, NGX_CMD_HTTP_PUSH_STREAM_SEND_PING);
#define ngx_http_push_stream_alert_worker_disconnect_subscribers(pid, slot, log) ngx_http_push_stream_alert_worker(pid, slot, log, NGX_CMD_HTTP_PUSH_STREAM_DISCONNECT_SUBSCRIBERS);
#define ngx_http_push_stream_alert_worker_census_subscribers(pid, slot, log) ngx_http_push_stream_alert_worker(pid, slot, log, NGX_CMD_HTTP_PUSH_STREAM_CENSUS_SUBSCRIBERS);

static ngx_int_t        ngx_http_push_stream_send_worker_message(ngx_http_push_stream_channel_t *channel, ngx_http_push_stream_subscriber_t *subscriber_sentinel, ngx_pid_t pid, ngx_int_t worker_slot, ngx_http_push_stream_msg_t *msg, ngx_log_t *log);

static ngx_int_t        ngx_http_push_stream_init_ipc(ngx_cycle_t *cycle, ngx_int_t workers);
static void             ngx_http_push_stream_ipc_exit_worker(ngx_cycle_t *cycle);
static ngx_int_t        ngx_http_push_stream_init_ipc_shm(ngx_int_t workers);
static void             ngx_http_push_stream_channel_handler(ngx_event_t *ev);

static ngx_inline void  ngx_http_push_stream_process_worker_message(void);
static ngx_inline void  ngx_http_push_stream_send_worker_ping_message(void);
static ngx_inline void  ngx_http_push_stream_disconnect_worker_subscribers(ngx_flag_t force_disconnect);
static ngx_inline void  ngx_http_push_stream_census_worker_subscribers(void);

static ngx_int_t    ngx_http_push_stream_respond_to_subscribers(ngx_http_push_stream_channel_t *channel, ngx_http_push_stream_subscriber_t *sentinel, ngx_http_push_stream_msg_t *msg);

#endif /* NGX_HTTP_PUSH_STREAM_MODULE_IPC_H_ */
