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
 * ngx_http_push_stream_module_setup.c
 *
 * Created: Oct 26, 2010
 * Authors: Wandenberg Peixoto <wandenberg@gmail.com>, Rogério Carvalho Schneider <stockrt@gmail.com>
 */

#include <ngx_http_push_stream_module_setup.h>

static ngx_command_t    ngx_http_push_stream_commands[] = {
    { ngx_string("push_stream_channels_statistics"),
        NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_NOARGS,
        ngx_http_push_stream_channels_statistics,
        NGX_HTTP_LOC_CONF_OFFSET,
        0,
        NULL },
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
    { ngx_string("push_stream_memory_cleanup_timeout"),
        NGX_HTTP_MAIN_CONF|NGX_CONF_TAKE1,
        ngx_conf_set_sec_slot,
        NGX_HTTP_MAIN_CONF_OFFSET,
        offsetof(ngx_http_push_stream_main_conf_t, memory_cleanup_timeout),
        NULL },
    { ngx_string("push_stream_channel_deleted_message_text"),
        NGX_HTTP_MAIN_CONF|NGX_CONF_TAKE1,
        ngx_conf_set_str_slot,
        NGX_HTTP_MAIN_CONF_OFFSET,
        offsetof(ngx_http_push_stream_main_conf_t, channel_deleted_message_text),
        NULL },
    { ngx_string("push_stream_ping_message_text"),
        NGX_HTTP_MAIN_CONF|NGX_CONF_TAKE1,
        ngx_conf_set_str_slot,
        NGX_HTTP_MAIN_CONF_OFFSET,
        offsetof(ngx_http_push_stream_main_conf_t, ping_message_text),
        NULL },
    { ngx_string("push_stream_store_messages"),
        NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
        ngx_conf_set_flag_slot,
        NGX_HTTP_LOC_CONF_OFFSET,
        offsetof(ngx_http_push_stream_loc_conf_t, store_messages),
        NULL },
    { ngx_string("push_stream_min_message_buffer_timeout"),
        NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
        ngx_conf_set_sec_slot,
        NGX_HTTP_LOC_CONF_OFFSET,
        offsetof(ngx_http_push_stream_loc_conf_t, buffer_timeout),
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
        offsetof(ngx_http_push_stream_loc_conf_t, authorized_channels_only),
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
    { ngx_string("push_stream_footer_template"),
        NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
        ngx_conf_set_str_slot,
        NGX_HTTP_LOC_CONF_OFFSET,
        offsetof(ngx_http_push_stream_loc_conf_t, footer_template),
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
    { ngx_string("push_stream_max_number_of_channels"),
        NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
        ngx_conf_set_num_slot,
        NGX_HTTP_LOC_CONF_OFFSET,
        offsetof(ngx_http_push_stream_loc_conf_t, max_number_of_channels),
        NULL },
    { ngx_string("push_stream_max_number_of_broadcast_channels"),
        NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
        ngx_conf_set_num_slot,
        NGX_HTTP_LOC_CONF_OFFSET,
        offsetof(ngx_http_push_stream_loc_conf_t, max_number_of_broadcast_channels),
        NULL },
    { ngx_string("push_stream_keepalive"),
        NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
        ngx_conf_set_flag_slot,
        NGX_HTTP_LOC_CONF_OFFSET,
        offsetof(ngx_http_push_stream_loc_conf_t, keepalive),
        NULL },
    { ngx_string("push_stream_publisher_admin"),
        NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
        ngx_conf_set_flag_slot,
        NGX_HTTP_LOC_CONF_OFFSET,
        offsetof(ngx_http_push_stream_loc_conf_t, publisher_admin),
        NULL },
    ngx_null_command
};


static ngx_http_module_t    ngx_http_push_stream_module_ctx = {
    NULL,                                       /* preconfiguration */
    ngx_http_push_stream_postconfig,            /* postconfiguration */
    ngx_http_push_stream_create_main_conf,      /* create main configuration */
    ngx_http_push_stream_init_main_conf,        /* init main configuration */
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

    if (ngx_http_push_stream_shm_zone == NULL) {
        ngx_log_error(NGX_LOG_NOTICE, cycle->log, 0, "ngx_http_push_stream_module will not be used with this configuration.");
        return NGX_OK;
    }

    // initialize our little IPC
    return ngx_http_push_stream_init_ipc(cycle, ccf->worker_processes);
}


static ngx_int_t
ngx_http_push_stream_init_worker(ngx_cycle_t *cycle)
{
    if (ngx_http_push_stream_shm_zone == NULL) {
        return NGX_OK;
    }

    if ((ngx_http_push_stream_ipc_init_worker()) != NGX_OK) {
        return NGX_ERROR;
    }

    ngx_http_push_stream_shm_data_t        *data = (ngx_http_push_stream_shm_data_t *) ngx_http_push_stream_shm_zone->data;
    ngx_http_push_stream_worker_data_t     *thisworker_data = data->ipc + ngx_process_slot;
    thisworker_data->pid = ngx_pid;

    // turn on timer to cleanup memory of old messages and channels
    ngx_http_push_stream_memory_cleanup_timer_set(ngx_http_push_stream_module_main_conf);

    return ngx_http_push_stream_register_worker_message_handler(cycle);
}


static void
ngx_http_push_stream_exit_master(ngx_cycle_t *cycle)
{
    if (ngx_http_push_stream_shm_zone == NULL) {
        return;
    }

    ngx_http_push_stream_shm_data_t        *data = (ngx_http_push_stream_shm_data_t *) ngx_http_push_stream_shm_zone->data;
    ngx_slab_pool_t                        *shpool = (ngx_slab_pool_t *) ngx_http_push_stream_shm_zone->shm.addr;

    // destroy channel tree in shared memory
    ngx_http_push_stream_collect_expired_messages_and_empty_channels(data, shpool, data->tree.root, 1);
    ngx_http_push_stream_free_memory_of_expired_messages_and_channels(1);
}


static void
ngx_http_push_stream_exit_worker(ngx_cycle_t *cycle)
{
    if (ngx_http_push_stream_shm_zone == NULL) {
        return;
    }

    // disconnect all subscribers (force_disconnect = 1)
    ngx_http_push_stream_disconnect_worker_subscribers(1);
    ngx_http_push_stream_clean_worker_data();

    if (ngx_http_push_stream_ping_event.timer_set) {
        ngx_del_timer(&ngx_http_push_stream_ping_event);
    }

    if (ngx_http_push_stream_disconnect_event.timer_set) {
        ngx_del_timer(&ngx_http_push_stream_disconnect_event);
    }

    if (ngx_http_push_stream_memory_cleanup_event.timer_set) {
        ngx_del_timer(&ngx_http_push_stream_memory_cleanup_event);
    }

    if (ngx_http_push_stream_buffer_cleanup_event.timer_set) {
        ngx_del_timer(&ngx_http_push_stream_buffer_cleanup_event);
    }

    ngx_http_push_stream_ipc_exit_worker(cycle);
}


static ngx_int_t
ngx_http_push_stream_postconfig(ngx_conf_t *cf)
{
    ngx_http_push_stream_main_conf_t   *conf = ngx_http_conf_get_module_main_conf(cf, ngx_http_push_stream_module);
    size_t                              shm_size;

    // initialize shared memory
    shm_size = ngx_align(conf->shm_size, ngx_pagesize);
    if (shm_size < 16 * ngx_pagesize) {
        ngx_conf_log_error(NGX_LOG_WARN, cf, 0, "The push_stream_max_reserved_memory value must be at least %udKiB", (16 * ngx_pagesize) >> 10);
        shm_size = 16 * ngx_pagesize;
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
    ngx_http_push_stream_main_conf_t    *mcf = ngx_pcalloc(cf->pool, sizeof(ngx_http_push_stream_main_conf_t));

    if (mcf == NULL) {
        return NGX_CONF_ERROR;
    }

    mcf->shm_size = NGX_CONF_UNSET_SIZE;
    mcf->memory_cleanup_timeout = NGX_CONF_UNSET;
    mcf->channel_deleted_message_text.data = NULL;
    mcf->ping_message_text.data = NULL;
    mcf->qtd_templates = 0;
    ngx_queue_init(&mcf->msg_templates.queue);

    ngx_http_push_stream_module_main_conf = mcf;

    return mcf;
}


static char *
ngx_http_push_stream_init_main_conf(ngx_conf_t *cf, void *parent)
{
    ngx_http_push_stream_main_conf_t     *conf = parent;

    if (conf->memory_cleanup_timeout == NGX_CONF_UNSET) {
        conf->memory_cleanup_timeout = NGX_HTTP_PUSH_STREAM_DEFAULT_MEMORY_CLEANUP_TIMEOUT;
    }

    if (conf->shm_size == NGX_CONF_UNSET_SIZE) {
        conf->shm_size = NGX_HTTP_PUSH_STREAM_DEFAULT_SHM_SIZE;
    }

    if (conf->channel_deleted_message_text.data == NULL) {
        conf->channel_deleted_message_text.data = NGX_HTTP_PUSH_STREAM_CHANNEL_DELETED_MESSAGE_TEXT.data;
        conf->channel_deleted_message_text.len = NGX_HTTP_PUSH_STREAM_CHANNEL_DELETED_MESSAGE_TEXT.len;
    }

    if (conf->ping_message_text.data == NULL) {
        conf->ping_message_text.data = NGX_HTTP_PUSH_STREAM_PING_MESSAGE_TEXT.data;
        conf->ping_message_text.len = NGX_HTTP_PUSH_STREAM_PING_MESSAGE_TEXT.len;
    }

    // memory cleanup timeout cannot't be small
    if (conf->memory_cleanup_timeout < NGX_HTTP_PUSH_STREAM_DEFAULT_MEMORY_CLEANUP_TIMEOUT) {
        ngx_conf_log_error(NGX_LOG_ERR, cf, 0, "memory cleanup timeout cannot't be less than %d.", NGX_HTTP_PUSH_STREAM_DEFAULT_MEMORY_CLEANUP_TIMEOUT);
        return NGX_CONF_ERROR;
    }

    // calc memory cleanup interval
    ngx_uint_t interval = conf->memory_cleanup_timeout / 3;
    conf->memory_cleanup_interval = (interval * 1000) + 1000; // min 11 seconds (((30 / 3) * 1000) + 1000)

    return NGX_CONF_OK;
}


// location config stuff
static void *
ngx_http_push_stream_create_loc_conf(ngx_conf_t *cf)
{
    ngx_http_push_stream_loc_conf_t     *lcf = ngx_pcalloc(cf->pool, sizeof(ngx_http_push_stream_loc_conf_t));

    if (lcf == NULL) {
        return NGX_CONF_ERROR;
    }

    lcf->buffer_timeout = NGX_CONF_UNSET;
    lcf->max_messages = NGX_CONF_UNSET_UINT;
    lcf->authorized_channels_only = NGX_CONF_UNSET_UINT;
    lcf->store_messages = NGX_CONF_UNSET_UINT;
    lcf->max_channel_id_length = NGX_CONF_UNSET_UINT;
    lcf->message_template_index = -1;
    lcf->message_template.data = NULL;
    lcf->header_template.data = NULL;
    lcf->footer_template.data = NULL;
    lcf->ping_message_interval = NGX_CONF_UNSET_MSEC;
    lcf->content_type.data = NULL;
    lcf->subscriber_disconnect_interval = NGX_CONF_UNSET_MSEC;
    lcf->subscriber_connection_timeout = NGX_CONF_UNSET;
    lcf->broadcast_channel_prefix.data = NULL;
    lcf->broadcast_channel_max_qtd = NGX_CONF_UNSET_UINT;
    lcf->max_number_of_channels = NGX_CONF_UNSET_UINT;
    lcf->max_number_of_broadcast_channels = NGX_CONF_UNSET_UINT;
    lcf->buffer_cleanup_interval = NGX_CONF_UNSET_MSEC;
    lcf->keepalive = NGX_CONF_UNSET_UINT;
    lcf->publisher_admin = NGX_CONF_UNSET_UINT;

    return lcf;
}


static char *
ngx_http_push_stream_merge_loc_conf(ngx_conf_t *cf, void *parent, void *child)
{
    ngx_http_push_stream_loc_conf_t     *prev = parent, *conf = child;

    ngx_conf_merge_sec_value(conf->buffer_timeout, prev->buffer_timeout, NGX_CONF_UNSET);
    ngx_conf_merge_uint_value(conf->max_messages, prev->max_messages, NGX_CONF_UNSET_UINT);
    ngx_conf_merge_uint_value(conf->authorized_channels_only, prev->authorized_channels_only, 0);
    ngx_conf_merge_uint_value(conf->store_messages, prev->store_messages, 0);
    ngx_conf_merge_uint_value(conf->max_channel_id_length, prev->max_channel_id_length, NGX_CONF_UNSET_UINT);
    ngx_conf_merge_str_value(conf->header_template, prev->header_template, NGX_HTTP_PUSH_STREAM_DEFAULT_HEADER_TEMPLATE);
    ngx_conf_merge_str_value(conf->message_template, prev->message_template, NGX_HTTP_PUSH_STREAM_DEFAULT_MESSAGE_TEMPLATE);
    ngx_conf_merge_str_value(conf->footer_template, prev->footer_template, NGX_HTTP_PUSH_STREAM_DEFAULT_FOOTER_TEMPLATE);
    ngx_conf_merge_msec_value(conf->ping_message_interval, prev->ping_message_interval, NGX_CONF_UNSET_MSEC);
    ngx_conf_merge_str_value(conf->content_type, prev->content_type, NGX_HTTP_PUSH_STREAM_DEFAULT_CONTENT_TYPE);
    ngx_conf_merge_msec_value(conf->subscriber_disconnect_interval, prev->subscriber_disconnect_interval, NGX_CONF_UNSET_MSEC);
    ngx_conf_merge_sec_value(conf->subscriber_connection_timeout, prev->subscriber_connection_timeout, NGX_CONF_UNSET);
    ngx_conf_merge_str_value(conf->broadcast_channel_prefix, prev->broadcast_channel_prefix, NGX_HTTP_PUSH_STREAM_DEFAULT_BROADCAST_CHANNEL_PREFIX);
    ngx_conf_merge_uint_value(conf->broadcast_channel_max_qtd, prev->broadcast_channel_max_qtd, NGX_CONF_UNSET_UINT);
    ngx_conf_merge_uint_value(conf->max_number_of_channels, prev->max_number_of_channels, NGX_CONF_UNSET_UINT);
    ngx_conf_merge_uint_value(conf->max_number_of_broadcast_channels, prev->max_number_of_broadcast_channels, NGX_CONF_UNSET_UINT);
    ngx_conf_merge_uint_value(conf->buffer_cleanup_interval, prev->buffer_cleanup_interval, NGX_CONF_UNSET_MSEC);
    ngx_conf_merge_uint_value(conf->keepalive, prev->keepalive, 0);
    ngx_conf_merge_uint_value(conf->publisher_admin, prev->publisher_admin, 0);


    // sanity checks
    // ping message interval cannot be zero
    if ((conf->ping_message_interval != NGX_CONF_UNSET_MSEC) && (conf->ping_message_interval == 0)) {
        ngx_conf_log_error(NGX_LOG_ERR, cf, 0, "push_stream_ping_message_interval cannot be zero.");
        return NGX_CONF_ERROR;
    }

    // message template cannot be blank
    if (conf->message_template.len == 0) {
        ngx_conf_log_error(NGX_LOG_ERR, cf, 0, "push_stream_message_template cannot be blank.");
        return NGX_CONF_ERROR;
    }

    // subscriber connection timeout cannot be zero
    if ((conf->subscriber_connection_timeout != NGX_CONF_UNSET) && (conf->subscriber_connection_timeout == 0)) {
        ngx_conf_log_error(NGX_LOG_ERR, cf, 0, "push_stream_subscriber_connection_timeout cannot be zero.");
        return NGX_CONF_ERROR;
    }

    // buffer timeout cannot be zero
    if ((conf->buffer_timeout != NGX_CONF_UNSET) && (conf->buffer_timeout == 0)) {
        ngx_conf_log_error(NGX_LOG_ERR, cf, 0, "push_stream_min_message_buffer_timeout cannot be zero.");
        return NGX_CONF_ERROR;
    }

    // max buffer message cannot be zero
    if ((conf->max_messages != NGX_CONF_UNSET_UINT) && (conf->max_messages == 0)) {
        ngx_conf_log_error(NGX_LOG_ERR, cf, 0, "push_stream_max_message_buffer_length cannot be zero.");
        return NGX_CONF_ERROR;
    }

    // store messages cannot be set without buffer timeout or max messages
    if ((conf->store_messages != NGX_CONF_UNSET_UINT) && (conf->store_messages) && (conf->buffer_timeout == NGX_CONF_UNSET) && (conf->max_messages == NGX_CONF_UNSET_UINT)) {
        ngx_conf_log_error(NGX_LOG_ERR, cf, 0, "push_stream_store_messages cannot be set without set max message buffer length or min message buffer timeout.");
        return NGX_CONF_ERROR;
    }

    // max channel id length cannot be zero
    if ((conf->max_channel_id_length != NGX_CONF_UNSET_UINT) && (conf->max_channel_id_length == 0)) {
        ngx_conf_log_error(NGX_LOG_ERR, cf, 0, "push_stream_max_channel_id_length cannot be zero.");
        return NGX_CONF_ERROR;
    }

    // broadcast channel max qtd cannot be zero
    if ((conf->broadcast_channel_max_qtd != NGX_CONF_UNSET_UINT) && (conf->broadcast_channel_max_qtd == 0)) {
        ngx_conf_log_error(NGX_LOG_ERR, cf, 0, "push_stream_broadcast_channel_max_qtd cannot be zero.");
        return NGX_CONF_ERROR;
    }

    // broadcast channel max qtd cannot be set without a channel prefix
    if ((conf->broadcast_channel_max_qtd != NGX_CONF_UNSET_UINT) && (conf->broadcast_channel_max_qtd > 0) && (conf->broadcast_channel_prefix.len == 0)) {
        ngx_conf_log_error(NGX_LOG_ERR, cf, 0, "cannot set broadcast channel max qtd if push_stream_broadcast_channel_prefix is not set or blank.");
        return NGX_CONF_ERROR;
    }

    // broadcast channel prefix cannot be set without a channel max qtd
    if ((conf->broadcast_channel_prefix.len > 0) && (conf->broadcast_channel_max_qtd == NGX_CONF_UNSET_UINT)) {
        ngx_conf_log_error(NGX_LOG_ERR, cf, 0, "cannot set broadcast channel prefix if push_stream_broadcast_channel_max_qtd is not set.");
        return NGX_CONF_ERROR;
    }

    // max number of channels cannot be zero
    if ((conf->max_number_of_channels != NGX_CONF_UNSET_UINT) && (conf->max_number_of_channels == 0)) {
        ngx_conf_log_error(NGX_LOG_ERR, cf, 0, "push_stream_max_number_of_channels cannot be zero.");
        return NGX_CONF_ERROR;
    }

    // max number of broadcast channels cannot be zero
    if ((conf->max_number_of_broadcast_channels != NGX_CONF_UNSET_UINT) && (conf->max_number_of_broadcast_channels == 0)) {
        ngx_conf_log_error(NGX_LOG_ERR, cf, 0, "push_stream_max_number_of_broadcast_channels cannot be zero.");
        return NGX_CONF_ERROR;
    }

    // max number of broadcast channels cannot be smaller than value in broadcast channel max qtd
    if ((conf->max_number_of_broadcast_channels != NGX_CONF_UNSET_UINT) && (conf->broadcast_channel_max_qtd != NGX_CONF_UNSET_UINT) &&  (conf->max_number_of_broadcast_channels < conf->broadcast_channel_max_qtd)) {
        ngx_conf_log_error(NGX_LOG_ERR, cf, 0, "max number of broadcast channels cannot be smaller than value in push_stream_broadcast_channel_max_qtd.");
        return NGX_CONF_ERROR;
    }

    // formatting header and footer template for chunk transfer
    if (conf->header_template.len > 0) {
        ngx_str_t *aux = ngx_http_push_stream_get_formatted_chunk(conf->header_template.data, conf->header_template.len, cf->pool);
        if (aux == NULL) {
            ngx_conf_log_error(NGX_LOG_ERR, cf, 0, "push stream module: unable to allocate memory to format header template");
            return NGX_CONF_ERROR;
        }
        conf->header_template.data = aux->data;
        conf->header_template.len = aux->len;
    }

    if (conf->footer_template.len > 0) {
        ngx_str_t *aux = ngx_http_push_stream_get_formatted_chunk(conf->footer_template.data, conf->footer_template.len, cf->pool);
        if (aux == NULL) {
            ngx_conf_log_error(NGX_LOG_ERR, cf, 0, "push stream module: unable to allocate memory to format footer template");
            return NGX_CONF_ERROR;
        }
        conf->footer_template.data = aux->data;
        conf->footer_template.len = aux->len;
    }

    conf->message_template_index = ngx_http_push_stream_find_or_add_template(cf, conf->message_template);

    // calc buffer cleanup interval
    if (conf->buffer_timeout != NGX_CONF_UNSET) {
        ngx_uint_t interval = conf->buffer_timeout / 3;
        conf->buffer_cleanup_interval = (interval > 1) ? (interval * 1000) + 1000 : 1000; // min 1 second
    } else if (conf->buffer_cleanup_interval == NGX_CONF_UNSET_MSEC) {
        conf->buffer_cleanup_interval = 1000; // 1 second
    }

    // calc subscriber disconnect interval
    if (conf->subscriber_connection_timeout != NGX_CONF_UNSET) {
        ngx_uint_t interval = conf->subscriber_connection_timeout / 3;
        conf->subscriber_disconnect_interval = (interval > 1) ? (interval * 1000) + 1000 : 1000; // min 1 second
    }

    return NGX_CONF_OK;
}


static char *
ngx_http_push_stream_setup_handler(ngx_conf_t *cf, void *conf, ngx_int_t (*handler) (ngx_http_request_t *))
{
    ngx_http_core_loc_conf_t            *clcf = ngx_http_conf_get_module_loc_conf(cf, ngx_http_core_module);

    clcf->handler = handler;
    clcf->if_modified_since = NGX_HTTP_IMS_OFF;
    // disable chunked_filter_module for streaming connections
    clcf->chunked_transfer_encoding = 0;

    return NGX_CONF_OK;
}


static char *
ngx_http_push_stream_channels_statistics(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    char *rc = ngx_http_push_stream_setup_handler(cf, conf, &ngx_http_push_stream_channels_statistics_handler);

    if (rc == NGX_CONF_OK) {
        ngx_http_push_stream_loc_conf_t     *pslcf = conf;
        pslcf->index_channel_id = ngx_http_get_variable_index(cf, &ngx_http_push_stream_channel_id);
        if (pslcf->index_channel_id == NGX_ERROR) {
            rc = NGX_CONF_ERROR;
        }
    }

    return rc;
}


static char *
ngx_http_push_stream_publisher(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    char *rc = ngx_http_push_stream_setup_handler(cf, conf, &ngx_http_push_stream_publisher_handler);

    if (rc == NGX_CONF_OK) {
        ngx_http_push_stream_loc_conf_t     *pslcf = conf;
        pslcf->index_channel_id = ngx_http_get_variable_index(cf, &ngx_http_push_stream_channel_id);
        if (pslcf->index_channel_id == NGX_ERROR) {
            rc = NGX_CONF_ERROR;
        }
    }

    return rc;
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
    ngx_http_push_stream_shm_zone = ngx_shared_memory_add(cf, &ngx_http_push_stream_shm_name, shm_size, &ngx_http_push_stream_module);

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
    int i;

    if (data) { /* zone already initialized */
        shm_zone->data = data;
        return NGX_OK;
    }

    ngx_slab_pool_t                     *shpool = (ngx_slab_pool_t *) shm_zone->shm.addr;
    ngx_rbtree_node_t                   *sentinel, *remove_sentinel, *unrecoverable_sentinel;
    ngx_http_push_stream_shm_data_t     *d;

    if ((d = (ngx_http_push_stream_shm_data_t *) ngx_slab_alloc(shpool, sizeof(*d))) == NULL) { //shm_data plus an array.
        return NGX_ERROR;
    }
    shm_zone->data = d;
    ngx_queue_init(&d->messages_to_delete.queue);
    for (i = 0; i < NGX_MAX_PROCESSES; i++) {
        d->ipc[i].pid = -1;
        d->ipc[i].subscribers = 0;
        d->ipc[i].messages_queue = NULL;
        d->ipc[i].worker_subscribers_sentinel = NULL;
    }

    // initialize rbtree
    if ((sentinel = ngx_slab_alloc(shpool, sizeof(*sentinel))) == NULL) {
        return NGX_ERROR;
    }
    ngx_rbtree_init(&d->tree, sentinel, ngx_http_push_stream_rbtree_insert);

    if ((remove_sentinel = ngx_slab_alloc(shpool, sizeof(*remove_sentinel))) == NULL) {
        return NGX_ERROR;
    }
    ngx_rbtree_init(&d->channels_to_delete, remove_sentinel, ngx_http_push_stream_rbtree_insert);

    if ((unrecoverable_sentinel = ngx_slab_alloc(shpool, sizeof(*unrecoverable_sentinel))) == NULL) {
        return NGX_ERROR;
    }
    ngx_rbtree_init(&d->unrecoverable_channels, unrecoverable_sentinel, ngx_http_push_stream_rbtree_insert);

    // create ping message
    ngx_http_push_stream_ping_msg = ngx_http_push_stream_convert_char_to_msg_on_shared_locked(ngx_http_push_stream_module_main_conf->ping_message_text.data, ngx_http_push_stream_module_main_conf->ping_message_text.len, NULL, NGX_HTTP_PUSH_STREAM_PING_MESSAGE_ID, ngx_cycle->pool);
    if (ngx_http_push_stream_ping_msg == NULL) {
        return NGX_ERROR;
    }

    return NGX_OK;
}
