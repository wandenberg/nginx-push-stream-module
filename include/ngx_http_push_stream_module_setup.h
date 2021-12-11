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
 * ngx_http_push_stream_module_setup.h
 *
 * Created: Oct 26, 2010
 * Authors: Wandenberg Peixoto <wandenberg@gmail.com>, Rogério Carvalho Schneider <stockrt@gmail.com>
 */

#ifndef NGX_HTTP_PUSH_STREAM_MODULE_SETUP_H_
#define NGX_HTTP_PUSH_STREAM_MODULE_SETUP_H_

#include <ngx_http_push_stream_module.h>
#include <ngx_http_push_stream_rbtree_util.h>
#include <ngx_http_push_stream_module_utils.h>
#include <ngx_http_push_stream_module_ipc.h>
#include <ngx_http_push_stream_module_publisher.h>
#include <ngx_http_push_stream_module_subscriber.h>
#include <ngx_http_push_stream_module_websocket.h>

#define NGX_HTTP_PUSH_STREAM_MESSAGE_BUFFER_CLEANUP_INTERVAL                5000     // 5 seconds
static time_t NGX_HTTP_PUSH_STREAM_DEFAULT_SHM_MEMORY_CLEANUP_OBJECTS_TTL = 10;      // 10 seconds
static time_t NGX_HTTP_PUSH_STREAM_DEFAULT_SHM_MEMORY_CLEANUP_INTERVAL    = 4000;    // 4 seconds
static time_t NGX_HTTP_PUSH_STREAM_DEFAULT_MESSAGE_TTL                    = 1800;    // 30 minutes
static time_t NGX_HTTP_PUSH_STREAM_DEFAULT_CHANNEL_INACTIVITY_TIME        = 30;      // 30 seconds

#define NGX_HTTP_PUSH_STREAM_DEFAULT_HEADER_TEMPLATE  ""
#define NGX_HTTP_PUSH_STREAM_DEFAULT_MESSAGE_TEMPLATE "~text~"
#define NGX_HTTP_PUSH_STREAM_DEFAULT_FOOTER_TEMPLATE  ""

#define NGX_HTTP_PUSH_STREAM_DEFAULT_ALLOWED_ORIGINS  ""

#define NGX_HTTP_PUSH_STREAM_DEFAULT_PADDING_BY_USER_AGENT  ""

#define NGX_HTTP_PUSH_STREAM_DEFAULT_WILDCARD_CHANNEL_PREFIX ""

#define NGX_HTTP_PUSH_STREAM_DEFAULT_EVENTS_CHANNEL_ID ""

static char *       ngx_http_push_stream_channels_statistics(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);

// publisher
static char *       ngx_http_push_stream_publisher(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);

// subscriber
static char *       ngx_http_push_stream_subscriber(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);

// setup
static char *       ngx_http_push_stream_setup_handler(ngx_conf_t *cf, void *conf, ngx_int_t (*handler) (ngx_http_request_t *));
static ngx_int_t    ngx_http_push_stream_init_module(ngx_cycle_t *cycle);
static ngx_int_t    ngx_http_push_stream_init_worker(ngx_cycle_t *cycle);
static void         ngx_http_push_stream_exit_worker(ngx_cycle_t *cycle);
static void         ngx_http_push_stream_exit_master(ngx_cycle_t *cycle);
static ngx_int_t    ngx_http_push_stream_preconfig(ngx_conf_t *cf);
static ngx_int_t    ngx_http_push_stream_postconfig(ngx_conf_t *cf);
static void *       ngx_http_push_stream_create_main_conf(ngx_conf_t *cf);
static char *       ngx_http_push_stream_init_main_conf(ngx_conf_t *cf, void *parent);
static void *       ngx_http_push_stream_create_loc_conf(ngx_conf_t *cf);
static char *       ngx_http_push_stream_merge_loc_conf(ngx_conf_t *cf, void *parent, void *child);

// shared memory
char *              ngx_http_push_stream_set_shm_size_slot(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
ngx_int_t           ngx_http_push_stream_init_shm_zone(ngx_shm_zone_t *shm_zone, void *data);
ngx_int_t           ngx_http_push_stream_init_global_shm_zone(ngx_shm_zone_t *shm_zone, void *data);

char *              ngx_http_push_stream_set_header_template_from_file(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);

#endif /* NGX_HTTP_PUSH_STREAM_MODULE_SETUP_H_ */
