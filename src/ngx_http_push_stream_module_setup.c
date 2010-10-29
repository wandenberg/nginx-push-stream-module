#include <ngx_http_push_stream_module.h>


static ngx_command_t    ngx_http_push_stream_commands[] = {
    { ngx_string("push_stream_publisher"),
        NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_NOARGS,
        ngx_http_push_stream_publisher,
        NGX_HTTP_LOC_CONF_OFFSET,
        0,
        NULL },
    { ngx_string("push_stream_subscriber"),
        NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_NOARGS,
        ngx_http_push_stream_subscriber,
        NGX_HTTP_LOC_CONF_OFFSET,
        0,
        NULL },
    { ngx_string("push_stream_max_reserved_memory"),
        NGX_HTTP_MAIN_CONF|NGX_CONF_TAKE1,
        ngx_conf_set_size_slot,
        NGX_HTTP_MAIN_CONF_OFFSET,
        offsetof(ngx_http_push_stream_main_conf_t, shm_size),
        NULL },
    { ngx_string("push_stream_store_messages"),
        NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
        ngx_conf_set_flag_slot,
        NGX_HTTP_LOC_CONF_OFFSET,
        offsetof(ngx_http_push_stream_loc_conf_t, store_messages),
        NULL },
    { ngx_string("push_stream_delete_oldest_received_message"),
        NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
        ngx_conf_set_flag_slot,
        NGX_HTTP_LOC_CONF_OFFSET,
        offsetof(ngx_http_push_stream_loc_conf_t, delete_oldest_received_message),
        NULL },
    { ngx_string("push_stream_min_message_buffer_timeout"),
        NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
        ngx_conf_set_sec_slot,
        NGX_HTTP_LOC_CONF_OFFSET,
        offsetof(ngx_http_push_stream_loc_conf_t, buffer_timeout),
        NULL },
    { ngx_string("push_stream_min_message_buffer_length"),
        NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
        ngx_conf_set_num_slot,
        NGX_HTTP_LOC_CONF_OFFSET,
        offsetof(ngx_http_push_stream_loc_conf_t, min_messages),
        NULL },
    { ngx_string("push_stream_max_message_buffer_length"),
        NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
        ngx_conf_set_num_slot,
        NGX_HTTP_LOC_CONF_OFFSET,
        offsetof(ngx_http_push_stream_loc_conf_t, max_messages),
        NULL },
    { ngx_string("push_stream_max_channel_id_length"),
        NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
        ngx_conf_set_num_slot,
        NGX_HTTP_LOC_CONF_OFFSET,
        offsetof(ngx_http_push_stream_loc_conf_t, max_channel_id_length),
        NULL },
    { ngx_string("push_stream_authorized_channels_only"),
        NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
        ngx_conf_set_flag_slot,
        NGX_HTTP_LOC_CONF_OFFSET,
        offsetof(ngx_http_push_stream_loc_conf_t, authorize_channel),
        NULL },
    { ngx_string("push_stream_header_template"),
        NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
        ngx_conf_set_str_slot,
        NGX_HTTP_LOC_CONF_OFFSET,
        offsetof(ngx_http_push_stream_loc_conf_t, header_template),
        NULL },
    { ngx_string("push_stream_message_template"),
        NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
        ngx_conf_set_str_slot,
        NGX_HTTP_LOC_CONF_OFFSET,
        offsetof(ngx_http_push_stream_loc_conf_t, message_template),
        NULL },
    { ngx_string("push_stream_content_type"),
        NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
        ngx_conf_set_str_slot,
        NGX_HTTP_LOC_CONF_OFFSET,
        offsetof(ngx_http_push_stream_loc_conf_t, content_type),
        NULL },
    { ngx_string("push_stream_ping_message_interval"),
        NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
        ngx_conf_set_msec_slot,
        NGX_HTTP_LOC_CONF_OFFSET,
        offsetof(ngx_http_push_stream_loc_conf_t, ping_message_interval),
        NULL },
    { ngx_string("push_stream_subscriber_disconnect_interval"),
        NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
        ngx_conf_set_msec_slot,
        NGX_HTTP_LOC_CONF_OFFSET,
        offsetof(ngx_http_push_stream_loc_conf_t, subscriber_disconnect_interval),
        NULL },
    { ngx_string("push_stream_subscriber_connection_timeout"),
        NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
        ngx_conf_set_sec_slot,
        NGX_HTTP_LOC_CONF_OFFSET,
        offsetof(ngx_http_push_stream_loc_conf_t, subscriber_connection_timeout),
        NULL },
    { ngx_string("push_stream_broadcast_channel_prefix"),
        NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
        ngx_conf_set_str_slot,
        NGX_HTTP_LOC_CONF_OFFSET,
        offsetof(ngx_http_push_stream_loc_conf_t, broadcast_channel_prefix),
        NULL },
    { ngx_string("push_stream_broadcast_channel_max_qtd"),
        NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
        ngx_conf_set_num_slot,
        NGX_HTTP_LOC_CONF_OFFSET,
        offsetof(ngx_http_push_stream_loc_conf_t, broadcast_channel_max_qtd),
        NULL },
    ngx_null_command
};


static ngx_http_module_t    ngx_http_push_stream_module_ctx = {
    NULL,                                       /* preconfiguration */
    ngx_http_push_stream_postconfig,            /* postconfiguration */
    ngx_http_push_stream_create_main_conf,      /* create main configuration */
    NULL,                                       /* init main configuration */
    NULL,                                       /* create server configuration */
    NULL,                                       /* merge server configuration */
    ngx_http_push_stream_create_loc_conf,       /* create location configuration */
    ngx_http_push_stream_merge_loc_conf,        /* merge location configuration */
};


ngx_module_t    ngx_http_push_stream_module = {
    NGX_MODULE_V1,
    &ngx_http_push_stream_module_ctx,           /* module context */
    ngx_http_push_stream_commands,              /* module directives */
    NGX_HTTP_MODULE,                            /* module type */
    NULL,                                       /* init master */
    ngx_http_push_stream_init_module,           /* init module */
    ngx_http_push_stream_init_worker,           /* init process */
    NULL,                                       /* init thread */
    NULL,                                       /* exit thread */
    ngx_http_push_stream_exit_worker,           /* exit process */
    ngx_http_push_stream_exit_master,           /* exit master */
    NGX_MODULE_V1_PADDING
};


static ngx_int_t
ngx_http_push_stream_init_module(ngx_cycle_t *cycle)
{
    ngx_core_conf_t                         *ccf = (ngx_core_conf_t *) ngx_get_conf(cycle->conf_ctx, ngx_core_module);


    ngx_http_push_stream_worker_processes = ccf->worker_processes;
    // initialize subscriber queues
    // pool, please
    if ((ngx_http_push_stream_pool = ngx_create_pool(NGX_CYCLE_POOL_SIZE, cycle->log)) == NULL) { // I trust the cycle pool size to be a well-tuned one.
        return NGX_ERROR;
    }

    if (ngx_http_push_stream_ping_msg == NULL) {
        ngx_slab_pool_t                     *shpool = (ngx_slab_pool_t *) ngx_http_push_stream_shm_zone->shm.addr;
        ngx_shmtx_lock(&shpool->mutex);
        if (ngx_http_push_stream_ping_msg == NULL) {
            if ((ngx_http_push_stream_ping_msg = ngx_http_push_stream_slab_alloc_locked(sizeof(ngx_http_push_stream_msg_t))) == NULL) {
                ngx_shmtx_unlock(&shpool->mutex);
                ngx_log_error(NGX_LOG_ERR, cycle->log, 0, "push stream module: unable to allocate memory for ngx_http_push_stream_ping_msg");
                return NGX_ERROR;
            }
            ngx_http_push_stream_ping_msg->expires = 0;
            ngx_http_push_stream_ping_msg->delete_oldest_received_min_messages = NGX_MAX_UINT32_VALUE;
            ngx_http_push_stream_ping_msg->persistent = 1;
        }
        ngx_shmtx_unlock(&shpool->mutex);
    }

    NGX_HTTP_PUSH_STREAM_MAKE_IN_MEMORY_CHAIN(ngx_http_push_stream_header_chain, ngx_http_push_stream_pool, "push stream module: unable to allocate chain to send header to new subscribers");
    NGX_HTTP_PUSH_STREAM_MAKE_IN_MEMORY_CHAIN(ngx_http_push_stream_crlf_chain, ngx_http_push_stream_pool, "push stream module: unable to allocate chain to send crlf to subscribers, on flush");

    // initialize our little IPC
    return ngx_http_push_stream_init_ipc(cycle, ngx_http_push_stream_worker_processes);
}


static ngx_int_t
ngx_http_push_stream_init_worker(ngx_cycle_t *cycle)
{
    if ((ngx_http_push_stream_init_ipc_shm(ngx_http_push_stream_worker_processes)) != NGX_OK) {
        return NGX_ERROR;
    }

    return ngx_http_push_stream_register_worker_message_handler(cycle);
}


static void
ngx_http_push_stream_exit_master(ngx_cycle_t *cycle)
{
    ngx_pfree(ngx_http_push_stream_pool, ngx_http_push_stream_ping_buf);
    ngx_pfree(ngx_http_push_stream_pool, ngx_http_push_stream_ping_msg);
    ngx_free_chain(ngx_http_push_stream_pool, ngx_http_push_stream_header_chain);
    ngx_free_chain(ngx_http_push_stream_pool, ngx_http_push_stream_crlf_chain);
    // destroy channel tree in shared memory
    ngx_http_push_stream_walk_rbtree(ngx_http_push_stream_movezig_channel_locked);
}


static void
ngx_http_push_stream_exit_worker(ngx_cycle_t *cycle)
{
    // disconnect all subscribers (force_disconnect = 1)
    ngx_http_push_stream_disconnect_worker_subscribers(1);

    if (ngx_http_push_stream_ping_event.timer_set) {
        ngx_del_timer(&ngx_http_push_stream_ping_event);
    }

    ngx_http_push_stream_ipc_exit_worker(cycle);
}


static ngx_int_t
ngx_http_push_stream_postconfig(ngx_conf_t *cf)
{
    ngx_http_push_stream_main_conf_t   *conf = ngx_http_conf_get_module_main_conf(cf, ngx_http_push_stream_module);
    size_t                              shm_size;


    // initialize shared memory
    if (conf->shm_size == NGX_CONF_UNSET_SIZE) {
        conf->shm_size = NGX_HTTP_PUSH_STREAM_DEFAULT_SHM_SIZE;
    }
    shm_size = ngx_align(conf->shm_size, ngx_pagesize);
    if (shm_size < 8 * ngx_pagesize) {
        ngx_conf_log_error(NGX_LOG_WARN, cf, 0, "The push_stream_max_reserved_memory value must be at least %udKiB", (8 * ngx_pagesize) >> 10);
        shm_size = 8 * ngx_pagesize;
    }
    if (ngx_http_push_stream_shm_zone && ngx_http_push_stream_shm_zone->shm.size != shm_size) {
        ngx_conf_log_error(NGX_LOG_WARN, cf, 0, "Cannot change memory area size without restart, ignoring change");
    }
    ngx_conf_log_error(NGX_LOG_INFO, cf, 0, "Using %udKiB of shared memory for push stream module", shm_size >> 10);

    return ngx_http_push_stream_set_up_shm(cf, shm_size);
}


// main config
static void *
ngx_http_push_stream_create_main_conf(ngx_conf_t *cf)
{
    ngx_http_push_stream_main_conf_t    *mcf = ngx_pcalloc(cf->pool, sizeof(*mcf));


    if (mcf == NULL) {
        return NGX_CONF_ERROR;
    }

    mcf->shm_size = NGX_CONF_UNSET_SIZE;

    return mcf;
}


// location config stuff
static void *
ngx_http_push_stream_create_loc_conf(ngx_conf_t *cf)
{
    ngx_http_push_stream_loc_conf_t     *lcf = ngx_pcalloc(cf->pool, sizeof(*lcf));


    if (lcf == NULL) {
        return NGX_CONF_ERROR;
    }

    lcf->buffer_timeout = NGX_CONF_UNSET;
    lcf->max_messages = NGX_CONF_UNSET;
    lcf->min_messages = NGX_CONF_UNSET;
    lcf->authorize_channel = NGX_CONF_UNSET;
    lcf->store_messages = NGX_CONF_UNSET;
    lcf->delete_oldest_received_message = NGX_CONF_UNSET;
    lcf->max_channel_id_length = NGX_CONF_UNSET;
    lcf->message_template.data = NULL;
    lcf->header_template.data = NULL;
    lcf->ping_message_interval = NGX_CONF_UNSET_MSEC;
    lcf->content_type.data = NULL;
    lcf->subscriber_disconnect_interval = NGX_CONF_UNSET_MSEC;
    lcf->subscriber_connection_timeout = NGX_CONF_UNSET;
    lcf->broadcast_channel_prefix.data = NULL;
    lcf->broadcast_channel_max_qtd = NGX_CONF_UNSET;

    return lcf;
}


static char *
ngx_http_push_stream_merge_loc_conf(ngx_conf_t *cf, void *parent, void *child)
{
    ngx_http_push_stream_loc_conf_t     *prev = parent, *conf = child;


    ngx_conf_merge_sec_value(conf->buffer_timeout, prev->buffer_timeout, NGX_HTTP_PUSH_STREAM_DEFAULT_BUFFER_TIMEOUT);
    ngx_conf_merge_value(conf->max_messages, prev->max_messages, NGX_HTTP_PUSH_STREAM_DEFAULT_MAX_MESSAGES);
    ngx_conf_merge_value(conf->min_messages, prev->min_messages, NGX_HTTP_PUSH_STREAM_DEFAULT_MIN_MESSAGES);
    ngx_conf_merge_value(conf->authorize_channel, prev->authorize_channel, 1);
    ngx_conf_merge_value(conf->store_messages, prev->store_messages, 1);
    ngx_conf_merge_value(conf->delete_oldest_received_message, prev->delete_oldest_received_message, 0);
    ngx_conf_merge_value(conf->max_channel_id_length, prev->max_channel_id_length, NGX_HTTP_PUSH_STREAM_MAX_CHANNEL_ID_LENGTH);
    ngx_conf_merge_str_value(conf->header_template, prev->header_template, NGX_HTTP_PUSH_STREAM_DEFAULT_HEADER_TEMPLATE);
    ngx_conf_merge_str_value(conf->message_template, prev->message_template, NGX_HTTP_PUSH_STREAM_DEFAULT_MESSAGE_TEMPLATE);
    ngx_conf_merge_msec_value(conf->ping_message_interval, prev->ping_message_interval, NGX_CONF_UNSET_MSEC);
    ngx_conf_merge_str_value(conf->content_type, prev->content_type, NGX_HTTP_PUSH_STREAM_DEFAULT_CONTENT_TYPE);
    ngx_conf_merge_msec_value(conf->subscriber_disconnect_interval, prev->subscriber_disconnect_interval, NGX_CONF_UNSET_MSEC);
    ngx_conf_merge_sec_value(conf->subscriber_connection_timeout, prev->subscriber_connection_timeout, NGX_CONF_UNSET);
    ngx_conf_merge_str_value(conf->broadcast_channel_prefix, prev->broadcast_channel_prefix, NGX_HTTP_PUSH_STREAM_DEFAULT_BROADCAST_CHANNEL_PREFIX);
    ngx_conf_merge_value(conf->broadcast_channel_max_qtd, prev->broadcast_channel_max_qtd, 1);

    // sanity checks
    if (conf->max_messages < conf->min_messages) {
        // min/max buffer size makes sense?
        ngx_conf_log_error(NGX_LOG_ERR, cf, 0, "push_stream_max_message_buffer_length cannot be smaller than push_stream_min_message_buffer_length.");
        return NGX_CONF_ERROR;
    }

    return NGX_CONF_OK;
}


static char *
ngx_http_push_stream_setup_handler(ngx_conf_t *cf, void *conf, ngx_int_t (*handler) (ngx_http_request_t *))
{
    ngx_http_core_loc_conf_t            *clcf = ngx_http_conf_get_module_loc_conf(cf, ngx_http_core_module);
    ngx_http_push_stream_loc_conf_t     *pslcf = conf;


    clcf->handler = handler;
    clcf->if_modified_since = NGX_HTTP_IMS_OFF;
    pslcf->index_channel_id = ngx_http_get_variable_index(cf, &ngx_http_push_stream_channel_id);

    if (pslcf->index_channel_id == NGX_ERROR) {
        return NGX_CONF_ERROR;
    }

    return NGX_CONF_OK;
}


static char *
ngx_http_push_stream_publisher(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    return ngx_http_push_stream_setup_handler(cf, conf, &ngx_http_push_stream_publisher_handler);
}


static char *
ngx_http_push_stream_subscriber(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    char *rc = ngx_http_push_stream_setup_handler(cf, conf, &ngx_http_push_stream_subscriber_handler);


    if (rc == NGX_CONF_OK) {
        ngx_http_push_stream_loc_conf_t     *pslcf = conf;
        pslcf->index_channels_path = ngx_http_get_variable_index(cf, &ngx_http_push_stream_channels_path);
        if (pslcf->index_channels_path == NGX_ERROR) {
            rc = NGX_CONF_ERROR;
        }
    }

    return rc;
}


// shared memory
static ngx_int_t
ngx_http_push_stream_set_up_shm(ngx_conf_t *cf, size_t shm_size)
{
    ngx_http_push_stream_shm_zone = ngx_shared_memory_add(cf, &ngx_push_stream_shm_name, shm_size, &ngx_http_push_stream_module);

    if (ngx_http_push_stream_shm_zone == NULL) {
        return NGX_ERROR;
    }

    ngx_http_push_stream_shm_zone->init = ngx_http_push_stream_init_shm_zone;
    ngx_http_push_stream_shm_zone->data = (void *) 1;

    return NGX_OK;
}


// shared memory zone initializer
static ngx_int_t
ngx_http_push_stream_init_shm_zone(ngx_shm_zone_t *shm_zone, void *data)
{
    if (data) { /* zone already initialized */
        shm_zone->data = data;
        return NGX_OK;
    }

    ngx_slab_pool_t                     *shpool = (ngx_slab_pool_t *) shm_zone->shm.addr;
    ngx_rbtree_node_t                   *sentinel;
    ngx_http_push_stream_shm_data_t     *d;

    ngx_http_push_stream_shpool = shpool; // we'll be using this a bit.

    if ((d = (ngx_http_push_stream_shm_data_t *) ngx_slab_alloc(shpool, sizeof(*d))) == NULL) { //shm_data plus an array.
        return NGX_ERROR;
    }
    shm_zone->data = d;
    d->ipc = NULL;
    // initialize rbtree
    if ((sentinel = ngx_slab_alloc(shpool, sizeof(*sentinel))) == NULL) {
        return NGX_ERROR;
    }
    ngx_rbtree_init(&d->tree, sentinel, ngx_http_push_stream_rbtree_insert);

    return NGX_OK;
}


// great justice appears to be at hand
static ngx_int_t
ngx_http_push_stream_movezig_channel_locked(ngx_http_push_stream_channel_t *channel, ngx_slab_pool_t *shpool)
{
    ngx_queue_t                     *sentinel = &channel->message_queue->queue;
    ngx_http_push_stream_msg_t      *msg = NULL;


    while (!ngx_queue_empty(sentinel)) {
        msg = ngx_queue_data(ngx_queue_head(sentinel), ngx_http_push_stream_msg_t, queue);
        ngx_http_push_stream_force_delete_message_locked(channel, msg, shpool);
    }

    return NGX_OK;
}
