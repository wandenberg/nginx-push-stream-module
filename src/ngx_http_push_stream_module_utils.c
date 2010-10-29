#include <ngx_http_push_stream_module.h>


#define NGX_HTTP_PUSH_STREAM_BUF_ALLOC_SIZE(buf)                             \
    (sizeof(*buf) +                                                          \
    (((buf)->temporary || (buf)->memory) ? ngx_buf_size(buf) : 0) +          \
    (((buf)->file!=NULL) ? (sizeof(*(buf)->file) + (buf)->file->name.len + 1) : 0))

#define NGX_HTTP_PUSH_STREAM_PUBLISHER_CHECK(val, fail, r, errormessage)     \
    if (val == fail) {                                                       \
        ngx_log_error(NGX_LOG_ERR, (r)->connection->log, 0, errormessage);   \
        ngx_http_finalize_request(r, NGX_HTTP_INTERNAL_SERVER_ERROR);        \
        return;                                                              \
    }

#define NGX_HTTP_PUSH_STREAM_PUBLISHER_CHECK_LOCKED(val, fail, r, errormessage, shpool) \
    if (val == fail) {                                                       \
        ngx_shmtx_unlock(&(shpool)->mutex);                                  \
        ngx_log_error(NGX_LOG_ERR, (r)->connection->log, 0, errormessage);   \
        ngx_http_finalize_request(r, NGX_HTTP_INTERNAL_SERVER_ERROR);        \
        return;                                                              \
    }

#define NGX_HTTP_PUSH_STREAM_MAKE_IN_MEMORY_CHAIN(chain, pool, errormessage) \
    if (chain == NULL) {                                                     \
        ngx_buf_t       *buffer;                                             \
        chain = ngx_pcalloc(pool, sizeof(ngx_chain_t));                      \
        buffer = ngx_pcalloc(pool, sizeof(ngx_buf_t));                       \
        if ((chain == NULL) || (buffer == NULL)) {                           \
            ngx_log_error(NGX_LOG_ERR, pool->log, 0, errormessage);          \
            return NGX_ERROR;                                                \
        }                                                                    \
        buffer->pos = NULL;                                                  \
        buffer->temporary = 0;                                               \
        buffer->memory = 1;                                                  \
        buffer->last_buf = 0;                                                \
        chain->buf = buffer;                                                 \
        chain->next = NULL;                                                  \
    }


// buffer is _copied_
// if shpool is provided, it is assumed that shm it is locked
static ngx_chain_t *
ngx_http_push_stream_create_output_chain_general(ngx_buf_t *buf, ngx_pool_t *pool, ngx_log_t *log, ngx_slab_pool_t *shpool)
{
    ngx_chain_t     *out;
    ngx_file_t      *file;


    if ((out = ngx_pcalloc(pool, sizeof(*out))) == NULL) {
        return NULL;
    }
    ngx_buf_t       *buf_copy;

    if ((buf_copy = ngx_pcalloc(pool, NGX_HTTP_PUSH_STREAM_BUF_ALLOC_SIZE(buf))) == NULL) {
        return NULL;
    }
    ngx_http_push_stream_copy_preallocated_buffer(buf, buf_copy);

    if (buf->file != NULL) {
        file = buf_copy->file;
        file->log = log;
        if (file->fd == NGX_INVALID_FILE) {
            if (shpool) {
                ngx_shmtx_unlock(&shpool->mutex);
                file->fd = ngx_open_file(file->name.data, NGX_FILE_RDONLY, NGX_FILE_OPEN, NGX_FILE_OWNER_ACCESS);
                ngx_shmtx_lock(&shpool->mutex);
            } else {
                file->fd = ngx_open_file(file->name.data, NGX_FILE_RDONLY, NGX_FILE_OPEN, NGX_FILE_OWNER_ACCESS);
            }
        }
        if (file->fd == NGX_INVALID_FILE) {
            return NULL;
        }
    }
    buf_copy->last_buf = 1;
    out->buf = buf_copy;
    out->next = NULL;

    return out;
}


static void
ngx_http_push_stream_copy_preallocated_buffer(ngx_buf_t *buf, ngx_buf_t *cbuf)
{
    if (cbuf != NULL) {
        ngx_memcpy(cbuf, buf, sizeof(*buf)); // overkill?
        if (buf->temporary || buf->memory) { // we don't want to copy mmpapped memory, so no ngx_buf_in_momory(buf)
            cbuf->pos = (u_char *) (cbuf + 1);
            cbuf->last = cbuf->pos + ngx_buf_size(buf);
            cbuf->start = cbuf->pos;
            cbuf->end = cbuf->start + ngx_buf_size(buf);
            ngx_memcpy(cbuf->pos, buf->pos, ngx_buf_size(buf));
            cbuf->memory = ngx_buf_in_memory_only(buf) ? 1 : 0;
        }
        if (buf->file != NULL) {
            cbuf->file = (ngx_file_t *) (cbuf + 1) + ((buf->temporary || buf->memory) ? ngx_buf_size(buf) : 0);
            cbuf->file->fd = NGX_INVALID_FILE;
            cbuf->file->log = NULL;
            cbuf->file->offset = buf->file->offset;
            cbuf->file->sys_offset = buf->file->sys_offset;
            cbuf->file->name.len = buf->file->name.len;
            cbuf->file->name.data = (u_char *) (cbuf->file + 1);
            ngx_memcpy(cbuf->file->name.data, buf->file->name.data, buf->file->name.len);
        }
    }
}


// remove a message from queue and free all associated memory
// assumes shpool is already locked
static ngx_inline void
ngx_http_push_stream_general_delete_message_locked(ngx_http_push_stream_channel_t *channel, ngx_http_push_stream_msg_t *msg, ngx_int_t force, ngx_slab_pool_t *shpool)
{
    if (msg == NULL) {
        return;
    }
    if (!msg->persistent) {
        if (channel != NULL) {
            ngx_queue_remove(&msg->queue);
            channel->stored_messages--;
        }
        if (msg->refcount <= 0 || force) {
            // nobody needs this message, or we were forced at integer-point to delete
            ngx_http_push_stream_free_message_locked(msg, shpool);
        }
    }
}


// free memory for a message
static ngx_inline void
ngx_http_push_stream_free_message_locked(ngx_http_push_stream_msg_t *msg, ngx_slab_pool_t *shpool)
{
    if (msg->buf->file != NULL) {
        ngx_shmtx_unlock(&shpool->mutex);
        if (msg->buf->file->fd != NGX_INVALID_FILE) {
            ngx_close_file(msg->buf->file->fd);
        }
        ngx_delete_file(msg->buf->file->name.data); // should I care about deletion errors? doubt it.
        ngx_shmtx_lock(&shpool->mutex);
    }
    ngx_slab_free_locked(shpool, msg->buf); // separate block, remember?
    ngx_slab_free_locked(shpool, msg);
}


// garbage-collecting slab allocator
void *
ngx_http_push_stream_slab_alloc_locked(size_t size)
{
    void        *p;


    if ((p = ngx_slab_alloc_locked(ngx_http_push_stream_shpool, size)) == NULL) {
        ngx_http_push_stream_channel_queue_t       *ccur, *cnext;
        ngx_uint_t                                  collected = 0;
        // failed. emergency garbage sweep, then collect channels
        ngx_queue_init(&channel_gc_sentinel.queue);
        ngx_http_push_stream_walk_rbtree(ngx_http_push_stream_channel_collector);
        for(ccur=(ngx_http_push_stream_channel_queue_t *) ngx_queue_next(&channel_gc_sentinel.queue); ccur!=&channel_gc_sentinel; ccur=cnext) {
            cnext = (ngx_http_push_stream_channel_queue_t *) ngx_queue_next(&ccur->queue);
            ngx_http_push_stream_delete_channel_locked(ccur->channel);
            ngx_free(ccur);
            collected++;
        }

        // TODO: collect worker messages maybe
#if (NGX_DEBUG)
        // only enable this log in debug mode
        ngx_log_error(NGX_LOG_WARN, ngx_cycle->log, 0, "push module: out of shared memory. emergency garbage collection deleted %ui unused channels.", collected);
#endif
        return ngx_slab_alloc_locked(ngx_http_push_stream_shpool, size);
    }

    return p;
}


//shpool must be locked. No memory is freed. O(1)
static ngx_http_push_stream_msg_t *
ngx_http_push_stream_get_oldest_message_locked(ngx_http_push_stream_channel_t *channel)
{
    ngx_queue_t     *sentinel = &channel->message_queue->queue;


    if (ngx_queue_empty(sentinel)) {
        return NULL;
    }

    ngx_queue_t     *qmsg = ngx_queue_head(sentinel);

    return ngx_queue_data(qmsg, ngx_http_push_stream_msg_t, queue);
}


static ngx_int_t
ngx_http_push_stream_channel_collector(ngx_http_push_stream_channel_t *channel, ngx_slab_pool_t *shpool)
{
    if ((ngx_http_push_stream_clean_channel_locked(channel)) != NULL) { // we're up for deletion
        ngx_http_push_stream_channel_queue_t        *trashy;
        if ((trashy = ngx_alloc(sizeof(*trashy), ngx_cycle->log)) != NULL) {
            // yeah, i'm allocating memory during garbage collection. sue me.
            trashy->channel = channel;
            ngx_queue_insert_tail(&channel_gc_sentinel.queue, &trashy->queue);
            return NGX_OK;
        }
        return NGX_ERROR;
    }
    return NGX_OK;
}


static ngx_table_elt_t *
ngx_http_push_stream_add_response_header(ngx_http_request_t *r, const ngx_str_t *header_name, const ngx_str_t *header_value)
{
    ngx_table_elt_t     *h = ngx_list_push(&r->headers_out.headers);


    if (h == NULL) {
        return NULL;
    }
    h->hash = 1;
    h->key.len = header_name->len;
    h->key.data = header_name->data;
    h->value.len = header_value->len;
    h->value.data = header_value->data;

    return h;
}


static ngx_int_t
ngx_http_push_stream_send_body_header(ngx_http_request_t *r, ngx_http_push_stream_loc_conf_t *pslcf)
{
    ngx_int_t rc = NGX_OK;

    if (pslcf->header_template.len > 0) {
        ngx_http_push_stream_header_chain->buf->pos = pslcf->header_template.data;
        ngx_http_push_stream_header_chain->buf->last = pslcf->header_template.data + pslcf->header_template.len;
        ngx_http_push_stream_header_chain->buf->start = ngx_http_push_stream_header_chain->buf->pos;
        ngx_http_push_stream_header_chain->buf->end = ngx_http_push_stream_header_chain->buf->last;

        rc = ngx_http_output_filter(r, ngx_http_push_stream_header_chain);

        if (rc == NGX_OK) {
            ngx_http_push_stream_crlf_chain->buf->pos = NGX_HTTP_PUSH_STREAM_CRLF.data;
            ngx_http_push_stream_crlf_chain->buf->last = NGX_HTTP_PUSH_STREAM_CRLF.data + NGX_HTTP_PUSH_STREAM_CRLF.len;
            ngx_http_push_stream_crlf_chain->buf->start = ngx_http_push_stream_crlf_chain->buf->pos;
            ngx_http_push_stream_crlf_chain->buf->end = ngx_http_push_stream_crlf_chain->buf->last;

            rc = ngx_http_output_filter(r, ngx_http_push_stream_crlf_chain);

            if (rc == NGX_OK) {
                rc = ngx_http_send_special(r, NGX_HTTP_FLUSH);
            }
        }
    }

    return rc;
}


static ngx_int_t
ngx_http_push_stream_send_ping(ngx_log_t *log, ngx_http_push_stream_loc_conf_t *pslcf)
{
    if (pslcf->message_template.len > 0) {
        if (ngx_http_push_stream_ping_buf == NULL) {
            ngx_slab_pool_t     *shpool = (ngx_slab_pool_t *) ngx_http_push_stream_shm_zone->shm.addr;
            ngx_shmtx_lock(&shpool->mutex);
            if (ngx_http_push_stream_ping_buf == NULL) {
                ngx_buf_t       *buf = NULL;
                ngx_pool_t      *temp_pool = NULL;
                if ((temp_pool = ngx_create_pool(NGX_CYCLE_POOL_SIZE, log)) == NULL) {
                    ngx_shmtx_unlock(&shpool->mutex);
                    ngx_log_error(NGX_LOG_ERR, log, 0, "push stream module: unable to allocate memory for temporary pool");
                    return NGX_ERROR;
                }

                if ((buf = ngx_http_push_stream_get_formatted_message_locked(pslcf, NULL, NULL, temp_pool)) == NULL) {
                    ngx_shmtx_unlock(&shpool->mutex);
                    ngx_log_error(NGX_LOG_ERR, ngx_http_push_stream_pool->log, 0, "push stream module: unable to format ping message");
                    ngx_destroy_pool(temp_pool);
                    return NGX_ERROR;
                }
                if ((ngx_http_push_stream_ping_buf = ngx_http_push_stream_slab_alloc_locked(NGX_HTTP_PUSH_STREAM_BUF_ALLOC_SIZE(buf))) == NULL) {
                    ngx_shmtx_unlock(&shpool->mutex);
                    ngx_log_error(NGX_LOG_ERR, ngx_http_push_stream_pool->log, 0, "push stream module: unable to allocate memory for formatted ping message");
                    ngx_destroy_pool(temp_pool);
                    return NGX_ERROR;
                }
                ngx_http_push_stream_copy_preallocated_buffer(buf, ngx_http_push_stream_ping_buf);
                ngx_http_push_stream_ping_msg->buf = ngx_http_push_stream_ping_buf;

                ngx_destroy_pool(temp_pool);
            }
            ngx_shmtx_unlock(&shpool->mutex);
        }
        ngx_http_push_stream_alert_worker_send_ping(ngx_pid, ngx_process_slot, ngx_http_push_stream_pool->log);
    }

    return NGX_OK;
}


static void
ngx_http_push_stream_ping_timer_wake_handler(ngx_event_t *ev)
{
    ngx_http_push_stream_loc_conf_t     *pslcf = ev->data;


    ngx_http_push_stream_send_ping(ev->log, pslcf);
    ngx_http_push_stream_ping_timer_reset(pslcf);
}


static void
ngx_http_push_stream_ping_timer_set(ngx_http_push_stream_loc_conf_t *pslcf)
{
    if ((pslcf->message_template.len > 0) && (pslcf->ping_message_interval != NGX_CONF_UNSET_MSEC)) {
        ngx_slab_pool_t     *shpool = (ngx_slab_pool_t *) ngx_http_push_stream_shm_zone->shm.addr;

        if (ngx_http_push_stream_ping_event.handler == NULL) {
            ngx_shmtx_lock(&shpool->mutex);
            if (ngx_http_push_stream_ping_event.handler == NULL) {
                ngx_http_push_stream_ping_event.handler = ngx_http_push_stream_ping_timer_wake_handler;
                ngx_http_push_stream_ping_event.data = pslcf;
                ngx_http_push_stream_ping_event.log = ngx_http_push_stream_pool->log;
                ngx_http_push_stream_ping_timer_reset(pslcf);
            }
            ngx_shmtx_unlock(&shpool->mutex);
        }
    }
}


static void
ngx_http_push_stream_ping_timer_reset(ngx_http_push_stream_loc_conf_t *pslcf)
{
    if ((pslcf->message_template.len > 0) && (pslcf->ping_message_interval != NGX_CONF_UNSET_MSEC)) {
        if (ngx_http_push_stream_ping_event.timedout) {
            #if defined nginx_version && nginx_version >= 7066
                ngx_time_update();
            #else
                ngx_time_update(0, 0);
            #endif
        }
        ngx_add_timer(&ngx_http_push_stream_ping_event, pslcf->ping_message_interval);
    }
}


static void
ngx_http_push_stream_disconnect_timer_set(ngx_http_push_stream_loc_conf_t *pslcf)
{
    if (pslcf->subscriber_disconnect_interval != NGX_CONF_UNSET_MSEC) {
        ngx_slab_pool_t     *shpool = (ngx_slab_pool_t *) ngx_http_push_stream_shm_zone->shm.addr;

        if (ngx_http_push_stream_disconnect_event.handler == NULL) {
            ngx_shmtx_lock(&shpool->mutex);
            if (ngx_http_push_stream_disconnect_event.handler == NULL) {
                ngx_http_push_stream_disconnect_event.handler = ngx_http_push_stream_disconnect_timer_wake_handler;
                ngx_http_push_stream_disconnect_event.data = pslcf;
                ngx_http_push_stream_disconnect_event.log = ngx_http_push_stream_pool->log;
                ngx_http_push_stream_disconnect_timer_reset(pslcf);
            }
            ngx_shmtx_unlock(&shpool->mutex);
        }
    }
}


static void
ngx_http_push_stream_disconnect_timer_reset(ngx_http_push_stream_loc_conf_t *pslcf)
{
    if (pslcf->subscriber_disconnect_interval != NGX_CONF_UNSET_MSEC) {
        if (ngx_http_push_stream_disconnect_event.timedout) {
            #if defined nginx_version && nginx_version >= 7066
                ngx_time_update();
            #else
                ngx_time_update(0, 0);
            #endif
        }
        ngx_add_timer(&ngx_http_push_stream_disconnect_event, pslcf->subscriber_disconnect_interval);
    }
}


static void
ngx_http_push_stream_disconnect_timer_wake_handler(ngx_event_t *ev)
{
    ngx_http_push_stream_loc_conf_t     *pslcf = ev->data;


    ngx_http_push_stream_alert_worker_disconnect_subscribers(ngx_pid, ngx_process_slot, ngx_http_push_stream_pool->log);
    ngx_http_push_stream_disconnect_timer_reset(pslcf);
}


static u_char *
ngx_http_push_stream_str_replace_locked(u_char *org, u_char *find, u_char *replace, ngx_pool_t *pool)
{
    ngx_uint_t len_org = ngx_strlen(org);
    ngx_uint_t len_find = ngx_strlen(find);
    ngx_uint_t len_replace = ngx_strlen(replace);

    u_char      *result = org;

    if (len_find > 0) {
        u_char      *ret = (u_char *) ngx_strstr(org, find);
        if (ret != NULL) {
            u_char      *tmp = ngx_pcalloc(pool,len_org + len_replace + len_find);

            u_int len_found = ret-org;
            ngx_memcpy(tmp, org, len_found);
            ngx_memcpy(tmp + len_found, replace, len_replace);
            ngx_memcpy(tmp + len_found + len_replace, org + len_found + len_find, len_org - len_found - len_find);

            result = ngx_http_push_stream_str_replace_locked(tmp, find, replace, pool);
        }
    }

    return result;
}


static ngx_buf_t *
ngx_http_push_stream_get_formatted_message_locked(ngx_http_push_stream_loc_conf_t *pslcf, ngx_http_push_stream_channel_t *channel, ngx_buf_t *buf, ngx_pool_t *pool)
{
    if (buf != NULL) {
        // ensure the final string in a reusable buffer
        *buf->last = '\0';
        buf->temporary = 1;
        buf->memory = 1;
    }

    if (pslcf->message_template.len > 0) {
        u_char template_with_crlf[pslcf->message_template.len + NGX_HTTP_PUSH_STREAM_CRLF.len + 1];
        ngx_memcpy(template_with_crlf, pslcf->message_template.data, pslcf->message_template.len);
        ngx_memcpy(template_with_crlf + pslcf->message_template.len, NGX_HTTP_PUSH_STREAM_CRLF.data, NGX_HTTP_PUSH_STREAM_CRLF.len);
        template_with_crlf[pslcf->message_template.len + NGX_HTTP_PUSH_STREAM_CRLF.len] = '\0';

        u_char char_id[10];
        u_char *msg = NGX_PUSH_STREAM_PING_MESSAGE_TEXT.data, *channel_id = NGX_PUSH_STREAM_PING_CHANNEL_ID.data;

        if ((channel != NULL) && (buf != NULL)) {
            ngx_memzero(char_id, sizeof(char_id));
            ngx_sprintf(char_id, "%d", channel->last_message_id + 1);
            msg = buf->pos;
            channel_id = channel->id.data;
        } else {
            ngx_memcpy(char_id, NGX_PUSH_STREAM_PING_MESSAGE_ID.data, NGX_PUSH_STREAM_PING_MESSAGE_ID.len + 1);
        }

        u_char      *txt = ngx_http_push_stream_str_replace_locked(template_with_crlf, NGX_PUSH_STREAM_TOKEN_MESSAGE_ID.data, char_id, pool);
        txt = ngx_http_push_stream_str_replace_locked(txt, NGX_PUSH_STREAM_TOKEN_MESSAGE_CHANNEL.data, channel_id, pool);
        txt = ngx_http_push_stream_str_replace_locked(txt, NGX_PUSH_STREAM_TOKEN_MESSAGE_TEXT.data, msg, pool);

        ngx_buf_t       *buf_msg = ngx_calloc_buf(pool);
        buf_msg->pos = txt;
        buf_msg->last = buf_msg->pos + ngx_strlen(txt) + 1;
        buf_msg->start = buf_msg->pos;
        buf_msg->end = buf_msg->last;
        buf_msg->temporary = 1;
        buf_msg->memory = 1;
        return buf_msg;
    } else if (buf != NULL) {
        ngx_uint_t len_org = ngx_buf_size(buf);
        ngx_uint_t len = len_org + NGX_HTTP_PUSH_STREAM_CRLF.len + 1;
        u_char *txt_with_crlf = ngx_pcalloc(pool, len);
        ngx_memcpy(txt_with_crlf, buf->pos, len_org);
        ngx_memcpy(txt_with_crlf + len_org, NGX_HTTP_PUSH_STREAM_CRLF.data, NGX_HTTP_PUSH_STREAM_CRLF.len);

        buf->pos = txt_with_crlf;
        buf->last = buf->pos + len;
        buf->start = buf->pos;
        buf->end = buf->last;
        *buf->last = '\0';
    }

    return buf;
}


static void
ngx_http_push_stream_worker_subscriber_cleanup_locked(ngx_http_push_stream_worker_subscriber_t *worker_subscriber)
{
    ngx_http_push_stream_subscription_t     *cur, *next, *sentinel;


    sentinel = worker_subscriber->subscriptions_sentinel;
    cur = (ngx_http_push_stream_subscription_t *) ngx_queue_head(&sentinel->queue);

    while (cur != sentinel) {
        next = (ngx_http_push_stream_subscription_t *) ngx_queue_next(&cur->queue);
        cur->channel->subscribers--;
        ngx_queue_remove(&cur->subscriber->queue);
        ngx_queue_remove(&cur->queue);
        cur = next;
    }
    ngx_queue_init(&sentinel->queue);
    ngx_queue_remove(&worker_subscriber->queue);
    ngx_queue_init(&worker_subscriber->queue);
    worker_subscriber->clndata->worker_subscriber = NULL;
}
