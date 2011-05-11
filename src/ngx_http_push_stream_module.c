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
 * ngx_http_push_stream_module.c
 *
 * Created: Oct 26, 2010
 * Authors: Wandenberg Peixoto <wandenberg@gmail.com>, Rogério Carvalho Schneider <stockrt@gmail.com>
 */

#include <ngx_http_push_stream_module.h>
#include <ngx_http_push_stream_rbtree_util.c>
#include <ngx_http_push_stream_module_utils.c>
#include <ngx_http_push_stream_module_ipc.c>
#include <ngx_http_push_stream_module_setup.c>
#include <ngx_http_push_stream_module_publisher.c>
#include <ngx_http_push_stream_module_subscriber.c>

static ngx_str_t *
ngx_http_push_stream_get_channel_id(ngx_http_request_t *r, ngx_http_push_stream_loc_conf_t *cf)
{
    ngx_http_variable_value_t      *vv = ngx_http_get_indexed_variable(r, cf->index_channel_id);
    ngx_str_t                      *id;

    if (vv == NULL || vv->not_found || vv->len == 0) {
        return NGX_HTTP_PUSH_STREAM_UNSET_CHANNEL_ID;
    }

    // maximum length limiter for channel id
    if ((cf->max_channel_id_length != NGX_CONF_UNSET_UINT) && (vv->len > cf->max_channel_id_length)) {
        ngx_log_error(NGX_LOG_WARN, r->connection->log, 0, "push stream module: channel id is larger than allowed %d", vv->len);
        return NGX_HTTP_PUSH_STREAM_TOO_LARGE_CHANNEL_ID;
    }

    if ((id = ngx_pcalloc(r->pool, sizeof(ngx_str_t) + vv->len + 1)) == NULL) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "push stream module: unable to allocate memory for $push_stream_channel_id string");
        return NULL;
    }

    id->data = (u_char *) (id + 1);
    id->len = vv->len;
    ngx_memset(id->data, '\0', vv->len + 1);
    ngx_memcpy(id->data, vv->data, vv->len);

    return id;
}


static ngx_buf_t *
ngx_http_push_stream_channel_info_formatted(ngx_pool_t *pool, const ngx_str_t *format, ngx_str_t *id, ngx_uint_t published_messages, ngx_uint_t stored_messages, ngx_uint_t subscribers)
{
    ngx_buf_t      *b;
    ngx_uint_t      len;

    if ((format == NULL) || (id == NULL)) {
        return NULL;
    }

    len = 3*NGX_INT_T_LEN + format->len + id->len - 11;// minus 11 sprintf

    if ((b = ngx_create_temp_buf(pool, len)) == NULL) {
        return NULL;
    }

    ngx_memset(b->start, '\0', len);
    b->last = ngx_sprintf(b->start, (char *) format->data, id->data, published_messages, stored_messages, subscribers);
    b->memory = 1;

    return b;
}

static ngx_int_t
ngx_http_push_stream_send_buf_response(ngx_http_request_t *r, ngx_buf_t *buf, const ngx_str_t *content_type, ngx_int_t status_code)
{
    ngx_chain_t             *chain;
    ngx_int_t                rc;

    if ((r == NULL) || (buf == NULL) || (content_type == NULL)) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    r->headers_out.content_type.len = content_type->len;
    r->headers_out.content_type.data = content_type->data;
    r->headers_out.content_length_n = ngx_buf_size(buf);

    if ((chain = ngx_pcalloc(r->pool, sizeof(ngx_chain_t))) == NULL) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "push stream module: unable to allocate memory for send buf response");
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    chain->buf = buf;
    chain->next = NULL;

    buf->memory = 1;
    buf->last_buf = 1;

    r->keepalive = 0;
    r->headers_out.status = status_code;

    ngx_http_discard_request_body(r);
    r->discard_body = 1;
    rc = ngx_http_send_header(r);

    if (rc == NGX_ERROR || rc > NGX_OK || r->header_only) {
        return rc;
    }
    rc = ngx_http_output_filter(r, chain);
    return rc;
}

// print information about a channel
static ngx_int_t
ngx_http_push_stream_send_response_channel_info(ngx_http_request_t *r, ngx_http_push_stream_channel_t *channel)
{
    ngx_buf_t                                   *b;
    ngx_http_push_stream_content_subtype_t      *subtype;

    subtype = ngx_http_push_stream_match_channel_info_format_and_content_type(r, 1);

    b = ngx_http_push_stream_channel_info_formatted(r->pool, subtype->format_item, &channel->id, channel->last_message_id, channel->stored_messages, channel->subscribers);
    if (b == NULL) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "Failed to allocate response buffer.");
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    return ngx_http_push_stream_send_buf_response(r, b, subtype->content_type, NGX_HTTP_OK);
}

static ngx_int_t
ngx_http_push_stream_send_response_all_channels_info_summarized(ngx_http_request_t *r) {

    ngx_buf_t                                   *b;
    ngx_uint_t                                   len;
    ngx_str_t                                   *currenttime, *hostname, *format;
    u_char                                      *subscribers_by_workers, *start;
    int                                          i;
    ngx_http_push_stream_shm_data_t             *shm_data;
    ngx_http_push_stream_worker_data_t          *worker_data;
    ngx_http_push_stream_content_subtype_t      *subtype;

    subtype = ngx_http_push_stream_match_channel_info_format_and_content_type(r, 1);
    currenttime = ngx_http_push_stream_get_formatted_current_time(r->pool);
    hostname = ngx_http_push_stream_get_formatted_hostname(r->pool);

    shm_data = (ngx_http_push_stream_shm_data_t *) ngx_http_push_stream_shm_zone->data;

    len = (subtype->format_summarized_worker_item->len > subtype->format_summarized_worker_last_item->len) ? subtype->format_summarized_worker_item->len : subtype->format_summarized_worker_last_item->len;
    len = ngx_http_push_stream_worker_processes * (2*NGX_INT_T_LEN + len - 5); //minus 5 sprintf
    subscribers_by_workers = ngx_pcalloc(r->pool, len);
    ngx_memset(subscribers_by_workers, '\0', len);
    start = subscribers_by_workers;
    for (i = 0; i < ngx_http_push_stream_worker_processes; i++) {
        format = (i < ngx_http_push_stream_worker_processes - 1) ? subtype->format_summarized_worker_item : subtype->format_summarized_worker_last_item;
        worker_data = shm_data->ipc + i;
        start = ngx_sprintf(start, (char *) format->data, worker_data->pid, worker_data->subscribers);
    }

    len = 3*NGX_INT_T_LEN + subtype->format_summarized->len + hostname->len + currenttime->len + ngx_strlen(subscribers_by_workers) - 18;// minus 18 sprintf

    if ((b = ngx_create_temp_buf(r->pool, len)) == NULL) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "Failed to allocate response buffer.");
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    ngx_memset(b->start, '\0', len);
    b->last = ngx_sprintf(b->start, (char *) subtype->format_summarized->data, hostname->data, currenttime->data, shm_data->channels, shm_data->broadcast_channels, shm_data->published_messages, shm_data->subscribers, subscribers_by_workers);

    return ngx_http_push_stream_send_buf_response(r, b, subtype->content_type, NGX_HTTP_OK);
}

static void
ngx_http_push_stream_rbtree_walker_channel_info_locked(ngx_rbtree_t *tree, ngx_pool_t *pool, ngx_rbtree_node_t *node, ngx_queue_t *queue_channel_info)
{
    ngx_rbtree_node_t   *sentinel = tree->sentinel;


    if (node != sentinel) {
        ngx_http_push_stream_channel_t *channel = (ngx_http_push_stream_channel_t *) node;
        ngx_http_push_stream_channel_info_t *channel_info;

        if ((channel_info = ngx_pcalloc(pool, sizeof(ngx_http_push_stream_channel_info_t))) == NULL) {
            return;
        }

        channel_info->id.data = channel->id.data;
        channel_info->id.len = channel->id.len;
        channel_info->published_messages = channel->last_message_id;
        channel_info->stored_messages = channel->stored_messages;
        channel_info->subscribers = channel->subscribers;

        ngx_queue_insert_tail(queue_channel_info, &channel_info->queue);

        if (node->left != NULL) {
            ngx_http_push_stream_rbtree_walker_channel_info_locked(tree, pool, node->left, queue_channel_info);
        }

        if (node->right != NULL) {
            ngx_http_push_stream_rbtree_walker_channel_info_locked(tree, pool, node->right, queue_channel_info);
        }
    }
}

static ngx_int_t
ngx_http_push_stream_send_response_all_channels_info_detailed(ngx_http_request_t *r) {
    ngx_int_t                                 rc;
    ngx_chain_t                              *chain;
    ngx_str_t                                *currenttime, *hostname;
    ngx_str_t                                 header_response;
    ngx_queue_t                               queue_channel_info;
    ngx_queue_t                              *cur, *next;
    ngx_http_push_stream_shm_data_t          *shm_data;
    ngx_slab_pool_t                          *shpool;

    ngx_http_push_stream_content_subtype_t   *subtype;

    subtype = ngx_http_push_stream_match_channel_info_format_and_content_type(r, 1);

    const ngx_str_t *format;
    const ngx_str_t    *head = subtype->format_group_head;
    const ngx_str_t *tail = subtype->format_group_tail;

    shpool = (ngx_slab_pool_t *) ngx_http_push_stream_shm_zone->shm.addr;
    shm_data = (ngx_http_push_stream_shm_data_t *) ngx_http_push_stream_shm_zone->data;

    r->headers_out.content_type.len = subtype->content_type->len;
    r->headers_out.content_type.data = subtype->content_type->data;
    r->headers_out.content_length_n = -1;
    r->headers_out.status = NGX_HTTP_OK;

    ngx_http_discard_request_body(r);
    r->discard_body = 1;

    rc = ngx_http_send_header(r);
    if (rc == NGX_ERROR || rc > NGX_OK || r->header_only) {
        return rc;
    }

    // this method send the response as a streaming cause the content could be very big
    r->keepalive = 1;

    ngx_queue_init(&queue_channel_info);

    ngx_shmtx_lock(&shpool->mutex);
    ngx_http_push_stream_rbtree_walker_channel_info_locked(&shm_data->tree, r->pool, shm_data->tree.root, &queue_channel_info);
    ngx_shmtx_unlock(&shpool->mutex);

    // get formatted current time
    currenttime = ngx_http_push_stream_get_formatted_current_time(r->pool);

    // get formatted hostname
    hostname = ngx_http_push_stream_get_formatted_hostname(r->pool);

    // send content header
    if ((header_response.data = ngx_pcalloc(r->pool, head->len + hostname->len + currenttime->len + 1)) == NULL) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "push stream module: unable to allocate memory for response channels info");
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    ngx_memset(header_response.data, '\0', head->len + hostname->len + currenttime->len + 1);
    ngx_sprintf(header_response.data, (char *) head->data, hostname->data, currenttime->data, shm_data->channels, shm_data->broadcast_channels);
    header_response.len = ngx_strlen(header_response.data);
    ngx_http_push_stream_send_response_chunk(r, header_response.data, header_response.len,0);

    // send content body
    cur = ngx_queue_head(&queue_channel_info);
    while (cur != &queue_channel_info) {
        next = ngx_queue_next(cur);
        ngx_http_push_stream_channel_info_t *channel_info = (ngx_http_push_stream_channel_info_t *) cur;
        if ((chain = ngx_pcalloc(r->pool, sizeof(ngx_chain_t))) == NULL) {
            ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "push stream module: unable to allocate memory for response channels info");
            return NGX_HTTP_INTERNAL_SERVER_ERROR;
        }

        format = (next != &queue_channel_info) ? subtype->format_group_item : subtype->format_group_last_item;

        chain->buf = ngx_http_push_stream_channel_info_formatted(r->pool, format, &channel_info->id, channel_info->published_messages, channel_info->stored_messages, channel_info->subscribers);
        chain->buf->last_buf = 0;
        chain->buf->flush = 1;
        ngx_http_output_filter(r, chain);

        cur = next;
    }

    r->keepalive = 0;

    // send content tail
    ngx_http_push_stream_send_response_chunk(r, tail->data, tail->len, 1);

    return NGX_DONE;
}
