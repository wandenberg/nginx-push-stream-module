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
 * ngx_http_push_stream_rbtree_util.c
 *
 * Modified: Oct 26, 2010
 * Modifications by: Wandenberg Peixoto <wandenberg@gmail.com>, Rogério Carvalho Schneider <stockrt@gmail.com>
 */

#include <ngx_http_push_stream_rbtree_util.h>

static ngx_http_push_stream_channel_t *
ngx_http_push_stream_find_channel_on_tree(ngx_str_t *id, ngx_log_t *log, ngx_rbtree_t *tree)
{
    uint32_t                            hash;
    ngx_rbtree_node_t                  *node, *sentinel;
    ngx_int_t                           rc;
    ngx_http_push_stream_channel_t     *channel = NULL;

    hash = ngx_crc32_short(id->data, id->len);

    node = tree->root;
    sentinel = tree->sentinel;

    while ((node != NULL) && (node != sentinel)) {
        if (hash < node->key) {
            node = node->left;
            continue;
        }

        if (hash > node->key) {
            node = node->right;
            continue;
        }

        /* hash == node->key */

        channel = (ngx_http_push_stream_channel_t *) node;

        rc = ngx_memn2cmp(id->data, channel->id.data, id->len, channel->id.len);
        if (rc == 0) {
            return channel;
        }

        node = (rc < 0) ? node->left : node->right;
    }

    return NULL;
}


static ngx_http_push_stream_channel_t *
ngx_http_push_stream_find_channel(ngx_str_t *id, ngx_log_t *log, ngx_http_push_stream_main_conf_t *mcf)
{
    ngx_http_push_stream_shm_data_t    *data = mcf->shm_data;
    ngx_http_push_stream_channel_t     *channel = NULL;

    if (id == NULL) {
        ngx_log_error(NGX_LOG_ERR, log, 0, "push stream module: tried to find a channel with a null id");
        return NULL;
    }

    ngx_shmtx_lock(&data->channels_queue_mutex);
    channel = ngx_http_push_stream_find_channel_on_tree(id, log, &data->tree);
    ngx_shmtx_unlock(&data->channels_queue_mutex);

    return channel;
}


// find a channel by id. if channel not found, make one, insert it, and return that.
static ngx_http_push_stream_channel_t *
ngx_http_push_stream_get_channel(ngx_str_t *id, ngx_log_t *log, ngx_http_push_stream_main_conf_t *mcf)
{
    ngx_http_push_stream_shm_data_t       *data = mcf->shm_data;
    ngx_http_push_stream_channel_t        *channel;
    ngx_slab_pool_t                       *shpool = mcf->shpool;
    ngx_flag_t                             is_wildcard_channel = 0;

    if (id == NULL) {
        ngx_log_error(NGX_LOG_ERR, log, 0, "push stream module: tried to create a channel with a null id");
        return NULL;
    }

    ngx_shmtx_lock(&data->channels_queue_mutex);

    // check again to see if any other worker didn't create the channel
    channel = ngx_http_push_stream_find_channel_on_tree(id, log, &data->tree);
    if (channel != NULL) { // we found our channel
        ngx_shmtx_unlock(&data->channels_queue_mutex);
        return channel;
    }

    if ((mcf->wildcard_channel_prefix.len > 0) && (ngx_strncmp(id->data, mcf->wildcard_channel_prefix.data, mcf->wildcard_channel_prefix.len) == 0)) {
        is_wildcard_channel = 1;
    }

    if (((!is_wildcard_channel) && (mcf->max_number_of_channels != NGX_CONF_UNSET_UINT) && (mcf->max_number_of_channels == data->channels)) ||
        ((is_wildcard_channel) && (mcf->max_number_of_wildcard_channels != NGX_CONF_UNSET_UINT) && (mcf->max_number_of_wildcard_channels == data->wildcard_channels))) {
        ngx_shmtx_unlock(&data->channels_queue_mutex);
        ngx_log_error(NGX_LOG_ERR, log, 0, "push stream module: number of channels were exceeded");
        return NGX_HTTP_PUSH_STREAM_NUMBER_OF_CHANNELS_EXCEEDED;
    }

    if ((channel = ngx_slab_alloc(shpool, sizeof(ngx_http_push_stream_channel_t))) == NULL) {
        ngx_shmtx_unlock(&data->channels_queue_mutex);
        ngx_log_error(NGX_LOG_ERR, log, 0, "push stream module: unable to allocate memory for new channel");
        return NULL;
    }

    if ((channel->id.data = ngx_slab_alloc(shpool, id->len + 1)) == NULL) {
        ngx_slab_free(shpool, channel);
        ngx_shmtx_unlock(&data->channels_queue_mutex);
        ngx_log_error(NGX_LOG_ERR, log, 0, "push stream module: unable to allocate memory for new channel id");
        return NULL;
    }

    channel->id.len = id->len;
    ngx_memcpy(channel->id.data, id->data, channel->id.len);
    channel->id.data[channel->id.len] = '\0';

    channel->wildcard = is_wildcard_channel;
    channel->channel_deleted_message = NULL;
    channel->last_message_id = 0;
    channel->last_message_time = 0;
    channel->last_message_tag = 0;
    channel->stored_messages = 0;
    channel->subscribers = 0;
    channel->deleted = 0;
    channel->for_events = ((mcf->events_channel_id.len > 0) && (channel->id.len == mcf->events_channel_id.len) && (ngx_strncmp(channel->id.data, mcf->events_channel_id.data, mcf->events_channel_id.len) == 0));
    channel->expires = ngx_time() + mcf->channel_inactivity_time;

    ngx_queue_init(&channel->message_queue);
    ngx_queue_init(&channel->workers_with_subscribers);

    channel->node.key = ngx_crc32_short(channel->id.data, channel->id.len);
    ngx_rbtree_insert(&data->tree, &channel->node);
    ngx_queue_insert_tail(&data->channels_queue, &channel->queue);
    (channel->wildcard) ? data->wildcard_channels++ : data->channels++;

    channel->mutex = &data->channels_mutex[data->mutex_round_robin++ % 10];

    ngx_shmtx_unlock(&data->channels_queue_mutex);

    ngx_http_push_stream_send_event(mcf, log, channel, &NGX_HTTP_PUSH_STREAM_EVENT_TYPE_CHANNEL_CREATED, NULL);

    return channel;
}


static void
ngx_rbtree_generic_insert(ngx_rbtree_node_t *temp, ngx_rbtree_node_t *node, ngx_rbtree_node_t *sentinel, int (*compare) (const ngx_rbtree_node_t *left, const ngx_rbtree_node_t *right))
{
    ngx_rbtree_node_t       **p;

    for (;;) {
        if (node->key < temp->key) {
            p = &temp->left;
        } else if (node->key > temp->key) {
            p = &temp->right;
        } else { /* node->key == temp->key */
            p = (compare(node, temp) < 0) ? &temp->left : &temp->right;
        }

        if (*p == sentinel) {
            break;
        }

        temp = *p;
    }

    *p = node;
    node->parent = temp;
    node->left = sentinel;
    node->right = sentinel;
    ngx_rbt_red(node);
}


static void
ngx_http_push_stream_rbtree_insert(ngx_rbtree_node_t *temp, ngx_rbtree_node_t *node, ngx_rbtree_node_t *sentinel)
{
    ngx_rbtree_generic_insert(temp, node, sentinel, ngx_http_push_stream_compare_rbtree_node);
}


static int
ngx_http_push_stream_compare_rbtree_node(const ngx_rbtree_node_t *v_left, const ngx_rbtree_node_t *v_right)
{
    ngx_http_push_stream_channel_t *left = (ngx_http_push_stream_channel_t *) v_left, *right = (ngx_http_push_stream_channel_t *) v_right;

    return ngx_memn2cmp(left->id.data, right->id.data, left->id.len, right->id.len);
}
