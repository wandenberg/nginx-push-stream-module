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
 * ngx_http_push_stream_rbtree_util.h
 *
 * Created: Oct 26, 2010
 * Authors: Wandenberg Peixoto <wandenberg@gmail.com>, Rogério Carvalho Schneider <stockrt@gmail.com>
 */

#ifndef NGX_HTTP_PUSH_STREAM_RBTREE_UTIL_H_
#define NGX_HTTP_PUSH_STREAM_RBTREE_UTIL_H_

static ngx_http_push_stream_channel_t *     ngx_http_push_stream_get_channel(ngx_str_t *id, ngx_log_t *log, ngx_http_push_stream_loc_conf_t *cf);
static ngx_http_push_stream_channel_t *     ngx_http_push_stream_find_channel(ngx_str_t *id, ngx_log_t *log);

static void         ngx_rbtree_generic_insert(ngx_rbtree_node_t *temp, ngx_rbtree_node_t *node, ngx_rbtree_node_t *sentinel, int (*compare) (const ngx_rbtree_node_t *left, const ngx_rbtree_node_t *right));
static void         ngx_http_push_stream_rbtree_insert(ngx_rbtree_node_t *temp, ngx_rbtree_node_t *node, ngx_rbtree_node_t *sentinel);
static int          ngx_http_push_stream_compare_rbtree_node(const ngx_rbtree_node_t *v_left, const ngx_rbtree_node_t *v_right);

#endif /* NGX_HTTP_PUSH_STREAM_RBTREE_UTIL_H_ */
