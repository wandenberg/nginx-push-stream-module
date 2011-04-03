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

        do {
            channel = (ngx_http_push_stream_channel_t *) node;

            rc = ngx_memn2cmp(id->data, channel->id.data, id->len, channel->id.len);
            if (rc == 0) {
                return channel;
            }

            node = (rc < 0) ? node->left : node->right;

        } while (node != sentinel && hash == node->key);

        break;
    }

    return NULL;
}

static ngx_http_push_stream_channel_t *
ngx_http_push_stream_find_channel(ngx_str_t *id, ngx_log_t *log)
{
    ngx_http_push_stream_shm_data_t    *data = (ngx_http_push_stream_shm_data_t *) ngx_http_push_stream_shm_zone->data;
    ngx_slab_pool_t                    *shpool = (ngx_slab_pool_t *) ngx_http_push_stream_shm_zone->shm.addr;
    ngx_http_push_stream_channel_t     *channel = NULL;

    if (id == NULL) {
        ngx_log_error(NGX_LOG_ERR, log, 0, "push stream module: tried to find a channel with a null id");
        return NULL;
    }

    channel = ngx_http_push_stream_find_channel_on_tree(id, log, &data->tree);
    if ((channel == NULL) || channel->deleted) {
        ngx_shmtx_lock(&shpool->mutex);
        channel = ngx_http_push_stream_find_channel_on_tree(id, log, &data->channels_to_delete);
        if (channel != NULL) {
            channel->deleted = 0;
            channel->expires = 0;
            (channel->broadcast) ? data->broadcast_channels++ : data->channels++;

            // reinitialize queues
            ngx_queue_init(&channel->message_queue.queue);
            ngx_queue_init(&channel->workers_with_subscribers.queue);

            ngx_rbtree_delete(&data->channels_to_delete, (ngx_rbtree_node_t *) channel);
            channel->node.key = ngx_crc32_short(channel->id.data, channel->id.len);
            ngx_rbtree_insert(&data->tree, (ngx_rbtree_node_t *) channel);
        }
        ngx_shmtx_unlock(&shpool->mutex);
    }

    return channel;
}


// find a channel by id. if channel not found, make one, insert it, and return that.
static ngx_http_push_stream_channel_t *
ngx_http_push_stream_get_channel(ngx_str_t *id, ngx_log_t *log, ngx_http_push_stream_loc_conf_t *cf)
{
    ngx_http_push_stream_shm_data_t       *data = (ngx_http_push_stream_shm_data_t *) ngx_http_push_stream_shm_zone->data;
    ngx_http_push_stream_channel_t        *channel;
    ngx_slab_pool_t                       *shpool = (ngx_slab_pool_t *) ngx_http_push_stream_shm_zone->shm.addr;
    ngx_flag_t                             is_broadcast_channel = 0;

    channel = ngx_http_push_stream_find_channel(id, log);
    if (channel != NULL) { // we found our channel
        return channel;
    }

    ngx_shmtx_lock(&shpool->mutex);
    if ((cf->broadcast_channel_prefix.len > 0) && (ngx_strncmp(id->data, cf->broadcast_channel_prefix.data, cf->broadcast_channel_prefix.len) == 0)) {
        is_broadcast_channel = 1;
    }

    if (((!is_broadcast_channel) && (cf->max_number_of_channels != NGX_CONF_UNSET_UINT) && (cf->max_number_of_channels == data->channels)) ||
        ((is_broadcast_channel) && (cf->max_number_of_broadcast_channels != NGX_CONF_UNSET_UINT) && (cf->max_number_of_broadcast_channels == data->broadcast_channels))) {
        ngx_shmtx_unlock(&shpool->mutex);
        return NGX_HTTP_PUSH_STREAM_NUMBER_OF_CHANNELS_EXCEEDED;
    }

    if ((channel = ngx_slab_alloc_locked(shpool, sizeof(ngx_http_push_stream_channel_t) + id->len + 1)) == NULL) {
        ngx_shmtx_unlock(&shpool->mutex);
        return NULL;
    }

    ngx_memset(channel, '\0', sizeof(ngx_http_push_stream_channel_t) + id->len + 1);
    channel->id.data = (u_char *) (channel + 1);

    channel->id.len = id->len;
    ngx_memcpy(channel->id.data, id->data, channel->id.len);
    channel->node.key = ngx_crc32_short(id->data, id->len);

    channel->last_message_id = 0;
    channel->stored_messages = 0;
    channel->subscribers = 0;

    channel->broadcast = is_broadcast_channel;

    channel->message_queue.deleted = 0;
    channel->deleted = 0;

    // initialize queues
    ngx_queue_init(&channel->message_queue.queue);
    ngx_queue_init(&channel->workers_with_subscribers.queue);

    ngx_rbtree_insert(&data->tree, (ngx_rbtree_node_t *) channel);
    (is_broadcast_channel) ? data->broadcast_channels++ : data->channels++;

    ngx_shmtx_unlock(&shpool->mutex);
    return channel;
}


static void
ngx_rbtree_generic_insert(ngx_rbtree_node_t *temp, ngx_rbtree_node_t *node, ngx_rbtree_node_t *sentinel, int (*compare) (const ngx_rbtree_node_t *left, const ngx_rbtree_node_t *right))
{
    for (;;) {
        if (node->key < temp->key) {
            if (temp->left == sentinel) {
                temp->left = node;
                break;
            }
            temp = temp->left;
        } else if (node->key > temp->key) {
            if (temp->right == sentinel) {
                temp->right = node;
                break;
            }
            temp = temp->right;
        } else { /* node->key == temp->key */
            if (compare(node, temp) < 0) {
                if (temp->left == sentinel) {
                    temp->left = node;
                    break;
                }
                temp = temp->left;
            } else {
                if (temp->right == sentinel) {
                    temp->right = node;
                    break;
                }
                temp = temp->right;
            }
        }
    }

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
