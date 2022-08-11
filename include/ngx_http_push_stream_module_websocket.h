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
 * ngx_http_push_stream_module_websocket.h
 *
 * Created: Oct 20, 2011
 * Authors: Wandenberg Peixoto <wandenberg@gmail.com>, Rogério Carvalho Schneider <stockrt@gmail.com>
 */

#ifndef NGX_HTTP_PUSH_STREAM_MODULE_WEBSOCKET_H_
#define NGX_HTTP_PUSH_STREAM_MODULE_WEBSOCKET_H_

#if (NGX_HAVE_SHA1)
#include <ngx_sha1.h>
#endif

#include <ngx_http_push_stream_module_utils.h>
#include <ngx_http_push_stream_module_subscriber.h>

static ngx_int_t    ngx_http_push_stream_websocket_handler(ngx_http_request_t *r);

#define NGX_HTTP_PUSH_STREAM_WEBSOCKET_READ_START_STEP           0
#define NGX_HTTP_PUSH_STREAM_WEBSOCKET_READ_GET_REAL_SIZE_STEP   1
#define NGX_HTTP_PUSH_STREAM_WEBSOCKET_READ_GET_MASK_KEY_STEP    2
#define NGX_HTTP_PUSH_STREAM_WEBSOCKET_READ_GET_PAYLOAD_STEP     3

#endif /* NGX_HTTP_PUSH_STREAM_MODULE_WEBSOCKET_H_ */
