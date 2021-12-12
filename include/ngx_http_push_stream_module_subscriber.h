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
 * ngx_http_push_stream_module_subscriber.h
 *
 * Created: Oct 26, 2010
 * Authors: Wandenberg Peixoto <wandenberg@gmail.com>, Rogério Carvalho Schneider <stockrt@gmail.com>
 */

#ifndef NGX_HTTP_PUSH_STREAM_MODULE_SUBSCRIBER_H_
#define NGX_HTTP_PUSH_STREAM_MODULE_SUBSCRIBER_H_

static ngx_int_t    ngx_http_push_stream_subscriber_handler(ngx_http_request_t *r);
static ngx_int_t    ngx_http_push_stream_validate_channels(ngx_http_request_t *r, ngx_http_push_stream_requested_channel_t *channels_ids, ngx_int_t *status_code, ngx_str_t **explain_error_message);

#endif /* NGX_HTTP_PUSH_STREAM_MODULE_SUBSCRIBER_H_ */
