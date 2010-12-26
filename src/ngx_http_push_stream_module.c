#include <ngx_http_push_stream_module.h>
#include <ngx_http_push_stream_rbtree_util.c>
#include <ngx_http_push_stream_module_utils.c>
#include <ngx_http_push_stream_module_ipc.c>
#include <ngx_http_push_stream_module_setup.c>
#include <ngx_http_push_stream_module_publisher.c>
#include <ngx_http_push_stream_module_subscriber.c>


static void
ngx_http_push_stream_send_response_channel_id_not_provided(ngx_http_request_t *r)
{
    ngx_buf_t       *buf = ngx_create_temp_buf(r->pool, 0);
    ngx_chain_t     *chain;


    if (buf != NULL) {
        buf->pos = (u_char *) NGX_HTTP_PUSH_STREAM_NO_CHANNEL_ID_MESSAGE;
        buf->last = buf->pos + sizeof(NGX_HTTP_PUSH_STREAM_NO_CHANNEL_ID_MESSAGE) - 1;
        buf->start = buf->pos;
        buf->end = buf->last;
        chain = ngx_http_push_stream_create_output_chain(buf, r->pool, r->connection->log);
        chain->buf->last_buf = 1;
        r->headers_out.content_length_n = ngx_buf_size(buf);
        r->headers_out.status = NGX_HTTP_NOT_FOUND;
        r->headers_out.content_type = NGX_HTTP_PUSH_STREAM_CONTENT_TYPE_TEXT_PLAIN;
        ngx_http_send_header(r);
        ngx_http_output_filter(r, chain);
        ngx_log_error(NGX_LOG_WARN, r->connection->log, 0, "push stream module: the $push_stream_channel_id variable is required but is not set");
    }
}


static ngx_str_t *
ngx_http_push_stream_get_channel_id(ngx_http_request_t *r, ngx_http_push_stream_loc_conf_t *cf)
{
    ngx_http_variable_value_t      *vv = ngx_http_get_indexed_variable(r, cf->index_channel_id);
    size_t                          len;
    ngx_str_t                      *id;


    if (vv == NULL || vv->not_found || vv->len == 0) {
        ngx_http_push_stream_send_response_channel_id_not_provided(r);
        return NULL;
    }

    // maximum length limiter for channel id
    len = vv->len <= cf->max_channel_id_length ? vv->len : cf->max_channel_id_length;

    if ((id = ngx_pcalloc(r->pool, sizeof(*id) + len + 1)) == NULL) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "push stream module: unable to allocate memory for $push_stream_channel_id string");
        return NULL;
    }

    id->len = len;
    id->data = (u_char *) (id + 1);
    ngx_memcpy(id->data, vv->data, len);

    return id;
}


static void
ngx_http_push_stream_match_channel_info_subtype(size_t off, u_char *cur, size_t rem, u_char **priority, const ngx_str_t **format, ngx_str_t *content_type)
{
    static ngx_http_push_stream_content_subtype_t subtypes[] = {
        { "json"  , 4, &NGX_HTTP_PUSH_STREAM_CHANNEL_INFO_JSON },
        { "yaml"  , 4, &NGX_HTTP_PUSH_STREAM_CHANNEL_INFO_YAML },
        { "xml"   , 3, &NGX_HTTP_PUSH_STREAM_CHANNEL_INFO_XML  },
        { "x-json", 6, &NGX_HTTP_PUSH_STREAM_CHANNEL_INFO_JSON },
        { "x-yaml", 6, &NGX_HTTP_PUSH_STREAM_CHANNEL_INFO_YAML }
    };
    u_char         *start = cur + off;
    ngx_uint_t      i;


    for(i=0; i<(sizeof(subtypes) / sizeof(ngx_http_push_stream_content_subtype_t)); i++) {
        if (ngx_strncmp(start, subtypes[i].subtype, rem<subtypes[i].len ? rem : subtypes[i].len) == 0) {
            if (*priority > start) {
                *format = subtypes[i].format;
                *priority = start;
                content_type->data = cur;
                content_type->len = off + 1 + subtypes[i].len;
            }
        }
    }
}


static ngx_buf_t *
ngx_http_push_stream_channel_info_formatted(ngx_pool_t *pool, ngx_str_t channelId, ngx_uint_t published_messages, ngx_uint_t stored_messages, ngx_uint_t subscribers, const ngx_str_t *format)
{
    ngx_buf_t      *b;
    ngx_uint_t      len;


    len = channelId.len + 3*NGX_INT_T_LEN + format->len - 8; // minus 8 sprintf

    if ((b = ngx_create_temp_buf(pool, len)) == NULL) {
        return NULL;
    }

    ngx_memset(b->start, '\0', len);
    b->last = ngx_sprintf(b->start, (char *) format->data, channelId.data, published_messages, stored_messages, subscribers);

    return b;
}


// print information about a channel
static ngx_int_t
ngx_http_push_stream_channel_info(ngx_http_request_t *r, ngx_str_t channelId, ngx_uint_t published_messages, ngx_uint_t stored_messages, ngx_uint_t subscribers)
{
    ngx_buf_t              *b;
    ngx_str_t               content_type = NGX_HTTP_PUSH_STREAM_CONTENT_TYPE_TEXT_PLAIN;
    const ngx_str_t        *format = &NGX_HTTP_PUSH_STREAM_CHANNEL_INFO_PLAIN;
    ngx_int_t               rc;
    ngx_chain_t            *chain;


    if (r->headers_in.accept) {
        // lame content-negotiation (without regard for qvalues)
        u_char     *accept = r->headers_in.accept->value.data;
        size_t      len = r->headers_in.accept->value.len;
        size_t      rem;
        u_char     *cur = accept;
        u_char     *priority = &accept[len - 1];

        for(rem=len; (cur = ngx_strnstr(cur, "text/", rem)) != NULL; cur += sizeof("text/") - 1) {
            rem = len - ((size_t) (cur-accept) + sizeof("text/") - 1);

            if (ngx_strncmp(cur + sizeof("text/") - 1, "plain", rem < 5 ? rem : 5) == 0) {
                if (priority) {
                    format = &NGX_HTTP_PUSH_STREAM_CHANNEL_INFO_PLAIN;
                    priority = cur + sizeof("text/") - 1;
                    // content-type is already set by default
                }
            }

            ngx_http_push_stream_match_channel_info_subtype(sizeof("text/") - 1, cur, rem, &priority, &format, &content_type);
        }

        cur = accept;

        for(rem=len; (cur = ngx_strnstr(cur, "application/", rem)) != NULL; cur += sizeof("application/") - 1) {
            rem = len - ((size_t) (cur-accept) + sizeof("application/") - 1);
            ngx_http_push_stream_match_channel_info_subtype(sizeof("application/") - 1, cur, rem, &priority, &format, &content_type);
        }
    }

    r->headers_out.content_type.len = content_type.len;
    r->headers_out.content_type.data = content_type.data;

    if ((b = ngx_http_push_stream_channel_info_formatted(r->pool, channelId, published_messages, stored_messages, subscribers, format)) == NULL) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    // lastly, set the content-length, because if the status code isn't 200, nginx may not do so automatically
    r->headers_out.content_length_n = ngx_buf_size(b);

    if (ngx_http_send_header(r) > NGX_HTTP_SPECIAL_RESPONSE) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    chain = ngx_http_push_stream_create_output_chain(b, r->pool, r->connection->log);
    rc = ngx_http_output_filter(r, chain);

    return rc;
}


static void
ngx_http_push_stream_rbtree_walker_channel_info_locked(ngx_rbtree_t *tree, ngx_pool_t *pool, ngx_rbtree_node_t *node, ngx_queue_t *queue_channel_info, const ngx_str_t *format)
{
    ngx_rbtree_node_t   *sentinel = tree->sentinel;


    if (node != sentinel) {
        ngx_http_push_stream_channel_t *channel = (ngx_http_push_stream_channel_t *) node;
        ngx_http_push_stream_msg_t *msg;

        if ((msg = ngx_pcalloc(pool, sizeof(ngx_http_push_stream_msg_t))) == NULL) {
            return;
        }

        if ((msg->buf = ngx_http_push_stream_channel_info_formatted(pool, channel->id, channel->last_message_id, channel->stored_messages, channel->subscribers, format)) == NULL) {
            return;
        }

        ngx_queue_insert_tail(queue_channel_info, &msg->queue);

        if (node->left != NULL) {
            ngx_http_push_stream_rbtree_walker_channel_info_locked(tree, pool, node->left, queue_channel_info, format);
        }

        if (node->right != NULL) {
            ngx_http_push_stream_rbtree_walker_channel_info_locked(tree, pool, node->right, queue_channel_info, format);
        }
    }
}


// print information about all channels
static ngx_int_t
ngx_http_push_stream_all_channels_info(ngx_http_request_t *r)
{
    ngx_buf_t              *b;
    ngx_uint_t              len = 0;
    ngx_str_t               content_type = NGX_HTTP_PUSH_STREAM_CONTENT_TYPE_JSON;
    const ngx_str_t        *format = &NGX_HTTP_PUSH_STREAM_CHANNEL_INFO_JSON;
    const ngx_str_t        *head = &NGX_HTTP_PUSH_STREAM_CHANNEL_INFO_GROUP_HEAD_JSON;
    const ngx_str_t        *tail = &NGX_HTTP_PUSH_STREAM_CHANNEL_INFO_GROUP_TAIL_JSON;
    ngx_chain_t            *chain;
    ngx_slab_pool_t        *shpool = (ngx_slab_pool_t *) ngx_http_push_stream_shm_zone->shm.addr;
    ngx_rbtree_t           *tree = &((ngx_http_push_stream_shm_data_t *) ngx_http_push_stream_shm_zone->data)->tree;
    ngx_queue_t             queue_channel_info;
    ngx_queue_t            *cur;
    ngx_tm_t                tm;
    u_char                  currenttime[20];
    u_char                  hostname[ngx_cycle->hostname.len + 1];


    r->headers_out.content_type.len = content_type.len;
    r->headers_out.content_type.data = content_type.data;

    ngx_queue_init(&queue_channel_info);

    ngx_shmtx_lock(&shpool->mutex);
    ngx_http_push_stream_rbtree_walker_channel_info_locked(tree, r->pool, tree->root, &queue_channel_info, format);
    ngx_shmtx_unlock(&shpool->mutex);

    if ((chain = ngx_pcalloc(r->pool, sizeof(*chain))) == NULL) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "push stream module: unable to allocate memory for response channels info");
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    ngx_gmtime(ngx_time(), &tm);
    ngx_sprintf(currenttime, (char *) NGX_PUSH_STREAM_DATE_FORMAT_ISO_8601.data, tm.ngx_tm_year, tm.ngx_tm_mon, tm.ngx_tm_mday, tm.ngx_tm_hour, tm.ngx_tm_min, tm.ngx_tm_sec);
    currenttime[19] = '\0';

    ngx_memcpy(hostname, (char *) ngx_cycle->hostname.data, ngx_cycle->hostname.len);
    hostname[ngx_cycle->hostname.len] = '\0';

    if ((b = ngx_create_temp_buf(r->pool, head->len + sizeof(hostname) + sizeof(currenttime))) == NULL) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "push stream module: unable to allocate memory for response channels info head/tail");
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    b->last = ngx_sprintf(b->last, (char *) head->data, hostname, currenttime);

    // calculates the size required to send the information from each channel
    cur = ngx_queue_head(&queue_channel_info);
    while (cur != &queue_channel_info) {
        ngx_http_push_stream_msg_t *msg = (ngx_http_push_stream_msg_t *) cur;
        len += ngx_buf_size(msg->buf) + NGX_HTTP_PUSH_STREAM_CHANNEL_INFO_ITEM_SEP_JSON.len;
        cur = ngx_queue_next(cur);
    }
    // sum the size of tail and formatted head messages
    len += tail->len + ngx_buf_size(b);

    // lastly, set the content-length, because if the status code isn't 200, nginx may not do so automatically
    r->headers_out.content_length_n = len;

    if (ngx_http_send_header(r) > NGX_HTTP_SPECIAL_RESPONSE) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    // send the head message
    b->last_buf = 1;
    b->memory = 1;
    chain->buf = b;
    ngx_http_output_filter(r, chain);

    cur = ngx_queue_head(&queue_channel_info);
    while (cur != &queue_channel_info) {
        ngx_http_push_stream_msg_t *msg = (ngx_http_push_stream_msg_t *) cur;
        chain->buf = msg->buf;
        ngx_http_output_filter(r, chain);
        cur = ngx_queue_next(cur);
        // sends the separate information
        chain->buf = b;
        if (cur != &queue_channel_info) {
            chain->buf->pos = NGX_HTTP_PUSH_STREAM_CHANNEL_INFO_ITEM_SEP_JSON.data;
            chain->buf->last = NGX_HTTP_PUSH_STREAM_CHANNEL_INFO_ITEM_SEP_JSON.data + NGX_HTTP_PUSH_STREAM_CHANNEL_INFO_ITEM_SEP_JSON.len;
        } else {
            chain->buf->pos = NGX_HTTP_PUSH_STREAM_CHANNEL_INFO_ITEM_SEP_LAST_ITEM_JSON.data;
            chain->buf->last = NGX_HTTP_PUSH_STREAM_CHANNEL_INFO_ITEM_SEP_LAST_ITEM_JSON.data + NGX_HTTP_PUSH_STREAM_CHANNEL_INFO_ITEM_SEP_LAST_ITEM_JSON.len;
        }
        chain->buf->start = chain->buf->pos;
        chain->buf->end = chain->buf->last;
        ngx_http_output_filter(r, chain);
    }

    // send the tail message
    chain->buf = b;
    chain->buf->pos = tail->data;
    chain->buf->last = tail->data + tail->len;
    chain->buf->start = chain->buf->pos;
    chain->buf->end = chain->buf->last;
    ngx_http_output_filter(r, chain);

    return NGX_HTTP_OK;
}


static void
ngx_http_push_stream_reserve_message_locked(ngx_http_push_stream_channel_t *channel, ngx_http_push_stream_msg_t *msg)
{
    if (!msg->persistent) {
        msg->refcount++;
    }
    // we need a refcount because channel messages MAY be dequed before they are used up. It thus falls on the IPC stuff to free it.
}


static void
ngx_http_push_stream_release_message_locked(ngx_http_push_stream_channel_t *channel, ngx_http_push_stream_msg_t *msg)
{
    if (!msg->persistent) {
        msg->refcount--;
        if (msg->queue.next == NULL && msg->refcount <= 0) {
            // message had been dequeued and nobody needs it anymore
            ngx_http_push_stream_free_message_locked(msg, ngx_http_push_stream_shpool);
        }
        if (ngx_http_push_stream_get_oldest_message_locked(channel) == msg) {
            ngx_http_push_stream_delete_message_locked(channel, msg, ngx_http_push_stream_shpool);
        }
    }
}


static ngx_int_t
ngx_http_push_stream_respond_status_only(ngx_http_request_t *r, ngx_int_t status_code, const ngx_str_t *statusline)
{
    r->headers_out.status = status_code;


    if (statusline != NULL) {
        r->headers_out.status_line.len = statusline->len;
        r->headers_out.status_line.data = statusline->data;
    }

    r->headers_out.content_length_n = 0;
    r->header_only = 1;

    return ngx_http_send_header(r);
}
