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

    while (node != sentinel) {
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

static void
ngx_http_push_stream_initialize_channel(ngx_http_push_stream_channel_t *channel)
{
    ngx_http_push_stream_shm_data_t    *data = (ngx_http_push_stream_shm_data_t *) ngx_http_push_stream_shm_zone->data;

    channel->channel_deleted_message = NULL;
    channel->last_message_id = 0;
    channel->last_message_time = 0;
    channel->last_message_tag = 0;
    channel->stored_messages = 0;
    channel->subscribers = 0;
    channel->deleted = 0;
    channel->expires = 0;
    channel->last_activity_time = ngx_time();

    ngx_queue_init(&channel->message_queue.queue);
    channel->message_queue.deleted = 0;

    channel->node.key = ngx_crc32_short(channel->id.data, channel->id.len);
    ngx_rbtree_insert(&data->tree, &channel->node);
    ngx_queue_insert_tail(&data->channels_queue, &channel->queue);
    channel->queue_sentinel = &data->channels_queue;
    (channel->broadcast) ? data->broadcast_channels++ : data->channels++;
}

static ngx_http_push_stream_channel_t *
ngx_http_push_stream_find_channel(ngx_str_t *id, ngx_log_t *log)
{
    ngx_http_push_stream_shm_data_t    *data = (ngx_http_push_stream_shm_data_t *) ngx_http_push_stream_shm_zone->data;
    ngx_http_push_stream_channel_t     *channel = NULL;

    if (id == NULL) {
        ngx_log_error(NGX_LOG_ERR, log, 0, "push stream module: tried to find a channel with a null id");
        return NULL;
    }

    channel = ngx_http_push_stream_find_channel_on_tree(id, log, &data->tree);
    if ((channel == NULL) || channel->deleted) {
        return NULL;
    }

    return channel;
}


// find a channel by id. if channel not found, make one, insert it, and return that.
static ngx_http_push_stream_channel_t *
ngx_http_push_stream_get_channel(ngx_str_t *id, ngx_log_t *log, ngx_http_push_stream_loc_conf_t *cf)
{
    ngx_http_push_stream_main_conf_t      *mcf = ngx_http_push_stream_module_main_conf;
    ngx_http_push_stream_shm_data_t       *data = (ngx_http_push_stream_shm_data_t *) ngx_http_push_stream_shm_zone->data;
    ngx_http_push_stream_channel_t        *channel;
    ngx_slab_pool_t                       *shpool = (ngx_slab_pool_t *) ngx_http_push_stream_shm_zone->shm.addr;
    ngx_flag_t                             is_broadcast_channel = 0;

    channel = ngx_http_push_stream_find_channel(id, log);
    if (channel != NULL) { // we found our channel
        return channel;
    }

    ngx_shmtx_lock(&shpool->mutex);

    // check again to see if any other worker didn't create the channel
    channel = ngx_http_push_stream_find_channel(id, log);
    if (channel != NULL) { // we found our channel
        ngx_shmtx_unlock(&shpool->mutex);
        return channel;
    }

    if ((mcf->broadcast_channel_prefix.len > 0) && (ngx_strncmp(id->data, mcf->broadcast_channel_prefix.data, mcf->broadcast_channel_prefix.len) == 0)) {
        is_broadcast_channel = 1;
    }

    if (((!is_broadcast_channel) && (mcf->max_number_of_channels != NGX_CONF_UNSET_UINT) && (mcf->max_number_of_channels == data->channels)) ||
        ((is_broadcast_channel) && (mcf->max_number_of_broadcast_channels != NGX_CONF_UNSET_UINT) && (mcf->max_number_of_broadcast_channels == data->broadcast_channels))) {
        ngx_shmtx_unlock(&shpool->mutex);
        return NGX_HTTP_PUSH_STREAM_NUMBER_OF_CHANNELS_EXCEEDED;
    }

    if ((channel = ngx_slab_alloc_locked(shpool, sizeof(ngx_http_push_stream_channel_t))) == NULL) {
        ngx_shmtx_unlock(&shpool->mutex);
        return NULL;
    }

    if ((channel->id.data = ngx_slab_alloc_locked(shpool, id->len + 1)) == NULL) {
        ngx_slab_free_locked(shpool, channel);
        ngx_shmtx_unlock(&shpool->mutex);
        return NULL;
    }

    channel->id.len = id->len;
    ngx_memcpy(channel->id.data, id->data, channel->id.len);
    channel->id.data[channel->id.len] = '\0';

    channel->broadcast = is_broadcast_channel;

    ngx_http_push_stream_initialize_channel(channel);

    // initialize workers_with_subscribers queues only when a channel is created
    ngx_queue_init(&channel->workers_with_subscribers.queue);

    ngx_shmtx_unlock(&shpool->mutex);
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


#define ngx_http_push_stream_walk_rbtree(apply) \
    ngx_http_push_stream_rbtree_walker(&((ngx_http_push_stream_shm_data_t *) ngx_http_push_stream_shm_zone->data)->tree, (ngx_slab_pool_t *) ngx_http_push_stream_shm_zone->shm.addr, apply, ((ngx_http_push_stream_shm_data_t *) ngx_http_push_stream_shm_zone->data)->tree.root)


static void
ngx_http_push_stream_rbtree_walker(ngx_rbtree_t *tree, ngx_slab_pool_t *shpool, ngx_int_t (*apply) (ngx_http_push_stream_channel_t *channel, ngx_slab_pool_t *shpool), ngx_rbtree_node_t *node)
{
    ngx_rbtree_node_t           *sentinel = tree->sentinel;


    if (node != sentinel) {
        apply((ngx_http_push_stream_channel_t *) node, shpool);
        if (node->left != NULL) {
            ngx_http_push_stream_rbtree_walker(tree, shpool, apply, node->left);
        }
        if (node->right != NULL) {
            ngx_http_push_stream_rbtree_walker(tree, shpool, apply, node->right);
        }
    }
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
