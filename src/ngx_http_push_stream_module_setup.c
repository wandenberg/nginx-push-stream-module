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

ngx_uint_t ngx_http_push_stream_padding_max_len = 0;

static ngx_command_t    ngx_http_push_stream_commands[] = {
    { ngx_string("push_stream_channels_statistics"),
        NGX_HTTP_LOC_CONF|NGX_CONF_NOARGS,
        ngx_http_push_stream_channels_statistics,
        NGX_HTTP_LOC_CONF_OFFSET,
        0,
        NULL },
    { ngx_string("push_stream_publisher"),
        NGX_HTTP_LOC_CONF|NGX_CONF_NOARGS|NGX_CONF_TAKE1,
        ngx_http_push_stream_publisher,
        NGX_HTTP_LOC_CONF_OFFSET,
        offsetof(ngx_http_push_stream_loc_conf_t, location_type),
        NULL },
    { ngx_string("push_stream_subscriber"),
        NGX_HTTP_LOC_CONF|NGX_CONF_NOARGS|NGX_CONF_TAKE1,
        ngx_http_push_stream_subscriber,
        NGX_HTTP_LOC_CONF_OFFSET,
        offsetof(ngx_http_push_stream_loc_conf_t, location_type),
        NULL },
    { ngx_string("push_stream_websocket"),
        NGX_HTTP_LOC_CONF|NGX_CONF_NOARGS,
        ngx_http_push_stream_websocket,
        NGX_HTTP_LOC_CONF_OFFSET,
        0,
        NULL },

    /* Main directives*/
    { ngx_string("push_stream_shared_memory_size"),
        NGX_HTTP_MAIN_CONF|NGX_CONF_TAKE1,
        ngx_conf_set_size_slot,
        NGX_HTTP_MAIN_CONF_OFFSET,
        offsetof(ngx_http_push_stream_main_conf_t, shm_size),
        NULL },
    { ngx_string("push_stream_shared_memory_cleanup_objects_ttl"),
        NGX_HTTP_MAIN_CONF|NGX_CONF_TAKE1,
        ngx_conf_set_sec_slot,
        NGX_HTTP_MAIN_CONF_OFFSET,
        offsetof(ngx_http_push_stream_main_conf_t, shm_cleanup_objects_ttl),
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
    { ngx_string("push_stream_message_ttl"),
        NGX_HTTP_MAIN_CONF|NGX_CONF_TAKE1,
        ngx_conf_set_sec_slot,
        NGX_HTTP_MAIN_CONF_OFFSET,
        offsetof(ngx_http_push_stream_main_conf_t, message_ttl),
        NULL },
    { ngx_string("push_stream_max_subscribers_per_channel"),
        NGX_HTTP_MAIN_CONF|NGX_CONF_TAKE1,
        ngx_conf_set_num_slot,
        NGX_HTTP_MAIN_CONF_OFFSET,
        offsetof(ngx_http_push_stream_main_conf_t, max_subscribers_per_channel),
        NULL },
    { ngx_string("push_stream_max_messages_stored_per_channel"),
        NGX_HTTP_MAIN_CONF|NGX_CONF_TAKE1,
        ngx_conf_set_num_slot,
        NGX_HTTP_MAIN_CONF_OFFSET,
        offsetof(ngx_http_push_stream_main_conf_t, max_messages_stored_per_channel),
        NULL },
    { ngx_string("push_stream_max_channel_id_length"),
        NGX_HTTP_MAIN_CONF|NGX_CONF_TAKE1,
        ngx_conf_set_num_slot,
        NGX_HTTP_MAIN_CONF_OFFSET,
        offsetof(ngx_http_push_stream_main_conf_t, max_channel_id_length),
        NULL },
    { ngx_string("push_stream_max_number_of_channels"),
        NGX_HTTP_MAIN_CONF|NGX_CONF_TAKE1,
        ngx_conf_set_num_slot,
        NGX_HTTP_MAIN_CONF_OFFSET,
        offsetof(ngx_http_push_stream_main_conf_t, max_number_of_channels),
        NULL },
    { ngx_string("push_stream_max_number_of_broadcast_channels"),
        NGX_HTTP_MAIN_CONF|NGX_CONF_TAKE1,
        ngx_conf_set_num_slot,
        NGX_HTTP_MAIN_CONF_OFFSET,
        offsetof(ngx_http_push_stream_main_conf_t, max_number_of_broadcast_channels),
        NULL },
    { ngx_string("push_stream_broadcast_channel_prefix"),
        NGX_HTTP_MAIN_CONF|NGX_CONF_TAKE1,
        ngx_conf_set_str_slot,
        NGX_HTTP_MAIN_CONF_OFFSET,
        offsetof(ngx_http_push_stream_main_conf_t, broadcast_channel_prefix),
        NULL },

    /* Location directives */
    { ngx_string("push_stream_store_messages"),
        NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
        ngx_conf_set_flag_slot,
        NGX_HTTP_LOC_CONF_OFFSET,
        offsetof(ngx_http_push_stream_loc_conf_t, store_messages),
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
    { ngx_string("push_stream_broadcast_channel_max_qtd"),
        NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
        ngx_conf_set_num_slot,
        NGX_HTTP_LOC_CONF_OFFSET,
        offsetof(ngx_http_push_stream_loc_conf_t, broadcast_channel_max_qtd),
        NULL },
    { ngx_string("push_stream_keepalive"),
        NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
        ngx_conf_set_flag_slot,
        NGX_HTTP_LOC_CONF_OFFSET,
        offsetof(ngx_http_push_stream_loc_conf_t, keepalive),
        NULL },
    { ngx_string("push_stream_eventsource_support"),
        NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
        ngx_conf_set_flag_slot,
        NGX_HTTP_LOC_CONF_OFFSET,
        offsetof(ngx_http_push_stream_loc_conf_t, eventsource_support),
        NULL },
    { ngx_string("push_stream_ping_message_interval"),
        NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
        ngx_conf_set_msec_slot,
        NGX_HTTP_LOC_CONF_OFFSET,
        offsetof(ngx_http_push_stream_loc_conf_t, ping_message_interval),
        NULL },
    { ngx_string("push_stream_subscriber_connection_ttl"),
        NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
        ngx_conf_set_msec_slot,
        NGX_HTTP_LOC_CONF_OFFSET,
        offsetof(ngx_http_push_stream_loc_conf_t, subscriber_connection_ttl),
        NULL },
    { ngx_string("push_stream_longpolling_connection_ttl"),
        NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
        ngx_conf_set_msec_slot,
        NGX_HTTP_LOC_CONF_OFFSET,
        offsetof(ngx_http_push_stream_loc_conf_t, longpolling_connection_ttl),
        NULL },
    { ngx_string("push_stream_websocket_allow_publish"),
        NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
        ngx_conf_set_flag_slot,
        NGX_HTTP_LOC_CONF_OFFSET,
        offsetof(ngx_http_push_stream_loc_conf_t, websocket_allow_publish),
        NULL },
    { ngx_string("push_stream_last_received_message_time"),
        NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
        ngx_http_set_complex_value_slot,
        NGX_HTTP_LOC_CONF_OFFSET,
        offsetof(ngx_http_push_stream_loc_conf_t, last_received_message_time),
        NULL },
    { ngx_string("push_stream_last_received_message_tag"),
        NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
        ngx_http_set_complex_value_slot,
        NGX_HTTP_LOC_CONF_OFFSET,
        offsetof(ngx_http_push_stream_loc_conf_t, last_received_message_tag),
        NULL },
    { ngx_string("push_stream_user_agent"),
        NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
        ngx_http_set_complex_value_slot,
        NGX_HTTP_LOC_CONF_OFFSET,
        offsetof(ngx_http_push_stream_loc_conf_t, user_agent),
        NULL },
    { ngx_string("push_stream_padding_by_user_agent"),
        NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
        ngx_conf_set_str_slot,
        NGX_HTTP_LOC_CONF_OFFSET,
        offsetof(ngx_http_push_stream_loc_conf_t, padding_by_user_agent),
        NULL },
    { ngx_string("push_stream_allowed_origins"),
        NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
        ngx_conf_set_str_slot,
        NGX_HTTP_LOC_CONF_OFFSET,
        offsetof(ngx_http_push_stream_loc_conf_t, allowed_origins),
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

    if ((ngx_http_push_stream_module_main_conf == NULL) || !ngx_http_push_stream_module_main_conf->enabled) {
        ngx_log_error(NGX_LOG_NOTICE, cycle->log, 0, "ngx_http_push_stream_module will not be used with this configuration.");
        return NGX_OK;
    }

    // initialize our little IPC
    return ngx_http_push_stream_init_ipc(cycle, ccf->worker_processes);
}


static ngx_int_t
ngx_http_push_stream_init_worker(ngx_cycle_t *cycle)
{
    if ((ngx_http_push_stream_module_main_conf == NULL) || !ngx_http_push_stream_module_main_conf->enabled) {
        return NGX_OK;
    }

    if ((ngx_process != NGX_PROCESS_SINGLE) && (ngx_process != NGX_PROCESS_WORKER)) {
        return NGX_OK;
    }

    if ((ngx_http_push_stream_ipc_init_worker()) != NGX_OK) {
        return NGX_ERROR;
    }

    ngx_http_push_stream_shm_data_t        *data = (ngx_http_push_stream_shm_data_t *) ngx_http_push_stream_shm_zone->data;
    ngx_http_push_stream_worker_data_t     *thisworker_data = data->ipc + ngx_process_slot;
    thisworker_data->pid = ngx_pid;

    // turn on timer to cleanup memory of old messages and channels
    ngx_http_push_stream_memory_cleanup_timer_set();

    return ngx_http_push_stream_register_worker_message_handler(cycle);
}


static void
ngx_http_push_stream_exit_master(ngx_cycle_t *cycle)
{
    if ((ngx_http_push_stream_module_main_conf == NULL) || !ngx_http_push_stream_module_main_conf->enabled) {
        return;
    }

    ngx_http_push_stream_shm_data_t        *data = (ngx_http_push_stream_shm_data_t *) ngx_http_push_stream_shm_zone->data;
    ngx_slab_pool_t                        *shpool = (ngx_slab_pool_t *) ngx_http_push_stream_shm_zone->shm.addr;

    // destroy channel tree in shared memory
    ngx_http_push_stream_collect_expired_messages_and_empty_channels(data, shpool, 1);
    ngx_http_push_stream_free_memory_of_expired_messages_and_channels(1);
}


static void
ngx_http_push_stream_exit_worker(ngx_cycle_t *cycle)
{
    if ((ngx_http_push_stream_module_main_conf == NULL) || !ngx_http_push_stream_module_main_conf->enabled) {
        return;
    }

    if ((ngx_process != NGX_PROCESS_SINGLE) && (ngx_process != NGX_PROCESS_WORKER)) {
        return;
    }

    ngx_http_push_stream_clean_worker_data();

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
    size_t                              shm_size_limit = 32 * ngx_pagesize;

    if (!conf->enabled) {
        return NGX_OK;
    }

    // initialize shared memory
    shm_size = ngx_align(conf->shm_size, ngx_pagesize);
    if (shm_size < shm_size_limit) {
        ngx_conf_log_error(NGX_LOG_WARN, cf, 0, "The push_stream_shared_memory_size value must be at least %udKiB", shm_size_limit >> 10);
        shm_size = shm_size_limit;
    }

    if (ngx_http_push_stream_shm_size && ngx_http_push_stream_shm_size != shm_size) {
        ngx_conf_log_error(NGX_LOG_WARN, cf, 0, "Cannot change memory area size without restart, ignoring change");
    } else {
        ngx_http_push_stream_shm_size = shm_size;
    }
    ngx_conf_log_error(NGX_LOG_INFO, cf, 0, "Using %udKiB of shared memory for push stream module", shm_size >> 10);

    ngx_uint_t steps = ngx_http_push_stream_padding_max_len / 100;
    if ((ngx_http_push_stream_module_paddings_chunks = ngx_palloc(cf->pool, sizeof(ngx_str_t) * (steps + 1))) == NULL) {
        ngx_conf_log_error(NGX_LOG_ERR, cf, 0, "push stream module: unable to allocate memory to create padding messages");
        return NGX_ERROR;
    }

    u_char aux[ngx_http_push_stream_padding_max_len + 1];
    ngx_memset(aux, ' ', ngx_http_push_stream_padding_max_len);
    aux[ngx_http_push_stream_padding_max_len] = '\0';

    ngx_int_t i, len = ngx_http_push_stream_padding_max_len;
    for (i = steps; i >= 0; i--) {
        if ((*(ngx_http_push_stream_module_paddings_chunks + i) = ngx_http_push_stream_get_formatted_chunk(aux, len, cf->pool)) == NULL) {
            ngx_conf_log_error(NGX_LOG_ERR, cf, 0, "push stream module: unable to allocate memory to create padding messages");
            return NGX_ERROR;
        }
        len = i * 100;
        *(aux + len) = '\0';
    }

    return ngx_http_push_stream_set_up_shm(cf, ngx_http_push_stream_shm_size);
}


// main config
static void *
ngx_http_push_stream_create_main_conf(ngx_conf_t *cf)
{
    ngx_http_push_stream_main_conf_t    *mcf = ngx_pcalloc(cf->pool, sizeof(ngx_http_push_stream_main_conf_t));

    if (mcf == NULL) {
        return NGX_CONF_ERROR;
    }

    mcf->enabled = 0;
    mcf->shm_size = NGX_CONF_UNSET_SIZE;
    mcf->memory_cleanup_interval = NGX_CONF_UNSET_MSEC;
    mcf->shm_cleanup_objects_ttl = NGX_CONF_UNSET;
    mcf->channel_deleted_message_text.data = NULL;
    mcf->ping_message_text.data = NULL;
    mcf->broadcast_channel_prefix.data = NULL;
    mcf->max_number_of_channels = NGX_CONF_UNSET_UINT;
    mcf->max_number_of_broadcast_channels = NGX_CONF_UNSET_UINT;
    mcf->message_ttl = NGX_CONF_UNSET;
    mcf->max_channel_id_length = NGX_CONF_UNSET_UINT;
    mcf->max_subscribers_per_channel = NGX_CONF_UNSET;
    mcf->max_messages_stored_per_channel = NGX_CONF_UNSET_UINT;
    mcf->qtd_templates = 0;
    ngx_queue_init(&mcf->msg_templates.queue);

    ngx_http_push_stream_module_main_conf = mcf;

    return mcf;
}


static char *
ngx_http_push_stream_init_main_conf(ngx_conf_t *cf, void *parent)
{
    ngx_http_push_stream_main_conf_t     *conf = parent;

    if (!conf->enabled) {
        return NGX_CONF_OK;
    }

    ngx_conf_init_value(conf->message_ttl, NGX_HTTP_PUSH_STREAM_DEFAULT_MESSAGE_TTL);
    ngx_conf_init_value(conf->shm_cleanup_objects_ttl, NGX_HTTP_PUSH_STREAM_DEFAULT_SHM_MEMORY_CLEANUP_OBJECTS_TTL);
    ngx_conf_init_size_value(conf->shm_size, NGX_HTTP_PUSH_STREAM_DEFAULT_SHM_SIZE);
    ngx_conf_merge_str_value(conf->channel_deleted_message_text, conf->channel_deleted_message_text, NGX_HTTP_PUSH_STREAM_CHANNEL_DELETED_MESSAGE_TEXT);
    ngx_conf_merge_str_value(conf->ping_message_text, conf->ping_message_text, NGX_HTTP_PUSH_STREAM_PING_MESSAGE_TEXT);
    ngx_conf_merge_str_value(conf->broadcast_channel_prefix, conf->broadcast_channel_prefix, NGX_HTTP_PUSH_STREAM_DEFAULT_BROADCAST_CHANNEL_PREFIX);

    // sanity checks
    // memory cleanup objects ttl cannot't be small
    if (conf->shm_cleanup_objects_ttl < NGX_HTTP_PUSH_STREAM_DEFAULT_SHM_MEMORY_CLEANUP_OBJECTS_TTL) {
        ngx_conf_log_error(NGX_LOG_ERR, cf, 0, "memory cleanup objects ttl cannot't be less than %d.", NGX_HTTP_PUSH_STREAM_DEFAULT_SHM_MEMORY_CLEANUP_OBJECTS_TTL);
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

    // message ttl cannot be zero
    if ((conf->message_ttl != NGX_CONF_UNSET) && (conf->message_ttl == 0)) {
        ngx_conf_log_error(NGX_LOG_ERR, cf, 0, "push_stream_message_ttl cannot be zero.");
        return NGX_CONF_ERROR;
    }

    // max subscriber per channel cannot be zero
    if ((conf->max_subscribers_per_channel != NGX_CONF_UNSET_UINT) && (conf->max_subscribers_per_channel == 0)) {
        ngx_conf_log_error(NGX_LOG_ERR, cf, 0, "push_stream_max_subscribers_per_channel cannot be zero.");
        return NGX_CONF_ERROR;
    }

    // max messages stored per channel cannot be zero
    if ((conf->max_messages_stored_per_channel != NGX_CONF_UNSET_UINT) && (conf->max_messages_stored_per_channel == 0)) {
        ngx_conf_log_error(NGX_LOG_ERR, cf, 0, "push_stream_max_messages_stored_per_channel cannot be zero.");
        return NGX_CONF_ERROR;
    }

    // max channel id length cannot be zero
    if ((conf->max_channel_id_length != NGX_CONF_UNSET_UINT) && (conf->max_channel_id_length == 0)) {
        ngx_conf_log_error(NGX_LOG_ERR, cf, 0, "push_stream_max_channel_id_length cannot be zero.");
        return NGX_CONF_ERROR;
    }

    // calc memory cleanup interval
    ngx_uint_t interval = conf->shm_cleanup_objects_ttl / 10;
    conf->memory_cleanup_interval = (interval * 1000) + 1000; // min 4 seconds (((30 / 10) * 1000) + 1000)

    ngx_regex_compile_t *backtrack_parser = NULL;
    u_char               errstr[NGX_MAX_CONF_ERRSTR];

    if ((backtrack_parser = ngx_pcalloc(cf->pool, sizeof(ngx_regex_compile_t))) == NULL) {
        ngx_conf_log_error(NGX_LOG_ERR, cf, 0, "push stream module: unable to allocate memory to compile backtrack parser");
        return NGX_CONF_ERROR;
    }

    backtrack_parser->pattern = NGX_HTTP_PUSH_STREAM_BACKTRACK_PATTERN;
    backtrack_parser->pool = cf->pool;
    backtrack_parser->err.len = NGX_MAX_CONF_ERRSTR;
    backtrack_parser->err.data = errstr;

    if (ngx_regex_compile(backtrack_parser) != NGX_OK) {
        ngx_conf_log_error(NGX_LOG_ERR, cf, 0, "push stream module: unable to compile backtrack parser pattern %V", &NGX_HTTP_PUSH_STREAM_BACKTRACK_PATTERN);
        return NGX_CONF_ERROR;
    }

    conf->backtrack_parser_regex = backtrack_parser->regex;

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

    lcf->authorized_channels_only = NGX_CONF_UNSET_UINT;
    lcf->store_messages = NGX_CONF_UNSET_UINT;
    lcf->message_template_index = -1;
    lcf->message_template.data = NULL;
    lcf->header_template.data = NULL;
    lcf->footer_template.data = NULL;
    lcf->content_type.data = NULL;
    lcf->broadcast_channel_max_qtd = NGX_CONF_UNSET_UINT;
    lcf->keepalive = NGX_CONF_UNSET_UINT;
    lcf->location_type = NGX_CONF_UNSET_UINT;
    lcf->eventsource_support = NGX_CONF_UNSET_UINT;
    lcf->ping_message_interval = NGX_CONF_UNSET_MSEC;
    lcf->subscriber_connection_ttl = NGX_CONF_UNSET_MSEC;
    lcf->longpolling_connection_ttl = NGX_CONF_UNSET_MSEC;
    lcf->websocket_allow_publish = NGX_CONF_UNSET_UINT;
    lcf->last_received_message_time = NULL;
    lcf->last_received_message_tag = NULL;
    lcf->user_agent = NULL;
    lcf->padding_by_user_agent.data = NULL;
    lcf->paddings = NULL;
    lcf->allowed_origins.data = NULL;

    return lcf;
}


static char *
ngx_http_push_stream_merge_loc_conf(ngx_conf_t *cf, void *parent, void *child)
{
    ngx_http_push_stream_loc_conf_t     *prev = parent, *conf = child;

    ngx_conf_merge_uint_value(conf->authorized_channels_only, prev->authorized_channels_only, 0);
    ngx_conf_merge_value(conf->store_messages, prev->store_messages, 0);
    ngx_conf_merge_str_value(conf->header_template, prev->header_template, NGX_HTTP_PUSH_STREAM_DEFAULT_HEADER_TEMPLATE);
    ngx_conf_merge_str_value(conf->message_template, prev->message_template, NGX_HTTP_PUSH_STREAM_DEFAULT_MESSAGE_TEMPLATE);
    ngx_conf_merge_str_value(conf->footer_template, prev->footer_template, NGX_HTTP_PUSH_STREAM_DEFAULT_FOOTER_TEMPLATE);
    ngx_conf_merge_str_value(conf->content_type, prev->content_type, NGX_HTTP_PUSH_STREAM_DEFAULT_CONTENT_TYPE);
    ngx_conf_merge_uint_value(conf->broadcast_channel_max_qtd, prev->broadcast_channel_max_qtd, ngx_http_push_stream_module_main_conf->max_number_of_broadcast_channels);
    ngx_conf_merge_uint_value(conf->keepalive, prev->keepalive, 0);
    ngx_conf_merge_value(conf->eventsource_support, prev->eventsource_support, 0);
    ngx_conf_merge_msec_value(conf->ping_message_interval, prev->ping_message_interval, NGX_CONF_UNSET_MSEC);
    ngx_conf_merge_msec_value(conf->subscriber_connection_ttl, prev->subscriber_connection_ttl, NGX_CONF_UNSET_MSEC);
    ngx_conf_merge_msec_value(conf->longpolling_connection_ttl, prev->longpolling_connection_ttl, conf->subscriber_connection_ttl);
    ngx_conf_merge_value(conf->websocket_allow_publish, prev->websocket_allow_publish, 0);
    ngx_conf_merge_str_value(conf->padding_by_user_agent, prev->padding_by_user_agent, NGX_HTTP_PUSH_STREAM_DEFAULT_PADDING_BY_USER_AGENT);
    ngx_conf_merge_str_value(conf->allowed_origins, prev->allowed_origins, NGX_HTTP_PUSH_STREAM_DEFAULT_ALLOWED_ORIGINS);

    if (conf->last_received_message_time == NULL) {
        conf->last_received_message_time = prev->last_received_message_time;
    }

    if (conf->last_received_message_tag == NULL) {
        conf->last_received_message_tag = prev->last_received_message_tag;
    }

    if (conf->user_agent == NULL) {
        conf->user_agent = prev->user_agent;
    }

    if (conf->location_type == NGX_CONF_UNSET_UINT) {
        return NGX_CONF_OK;
    }

    // changing properties for event source support
    if (conf->eventsource_support) {
        if ((conf->location_type != NGX_HTTP_PUSH_STREAM_SUBSCRIBER_MODE_LONGPOLLING) &&
            (conf->location_type != NGX_HTTP_PUSH_STREAM_SUBSCRIBER_MODE_POLLING) &&
            (conf->location_type != NGX_HTTP_PUSH_STREAM_SUBSCRIBER_MODE_STREAMING)) {
            ngx_conf_log_error(NGX_LOG_ERR, cf, 0, "push stream module: event source support is only available on subscriber location");
            return NGX_CONF_ERROR;
        }

        conf->content_type.data = NGX_HTTP_PUSH_STREAM_EVENTSOURCE_CONTENT_TYPE.data;
        conf->content_type.len = NGX_HTTP_PUSH_STREAM_EVENTSOURCE_CONTENT_TYPE.len;

        // formatting header template
        if (conf->header_template.len > 0) {
            ngx_str_t *aux = ngx_http_push_stream_apply_template_to_each_line(&conf->header_template, &NGX_HTTP_PUSH_STREAM_EVENTSOURCE_COMMENT_TEMPLATE, cf->pool);
            if (aux == NULL) {
                ngx_conf_log_error(NGX_LOG_ERR, cf, 0, "push_stream_message_module failed to apply template to header message.");
                return NGX_CONF_ERROR;
            }
            conf->header_template.data = aux->data;
            conf->header_template.len = aux->len;
        } else {
            conf->header_template.data = NGX_HTTP_PUSH_STREAM_EVENTSOURCE_DEFAULT_HEADER_TEMPLATE.data;
            conf->header_template.len = NGX_HTTP_PUSH_STREAM_EVENTSOURCE_DEFAULT_HEADER_TEMPLATE.len;
        }

        // formatting message template
        ngx_str_t *aux = (conf->message_template.len > 0) ? &conf->message_template : (ngx_str_t *) &NGX_HTTP_PUSH_STREAM_TOKEN_MESSAGE_TEXT;
        ngx_str_t *template = ngx_http_push_stream_create_str(cf->pool, NGX_HTTP_PUSH_STREAM_EVENTSOURCE_MESSAGE_PREFIX.len + aux->len + sizeof(CRLF) -1);
        if (template == NULL) {
            ngx_conf_log_error(NGX_LOG_ERR, cf, 0, "push stream module: unable to allocate memory to append message prefix to message template");
            return NGX_CONF_ERROR;
        }
        u_char *last = ngx_copy(template->data, NGX_HTTP_PUSH_STREAM_EVENTSOURCE_MESSAGE_PREFIX.data, NGX_HTTP_PUSH_STREAM_EVENTSOURCE_MESSAGE_PREFIX.len);
        last = ngx_copy(last, aux->data, aux->len);
        ngx_memcpy(last, CRLF, 2);

        conf->message_template.data = template->data;
        conf->message_template.len = template->len;

        // formatting footer template
        if (conf->footer_template.len > 0) {
            ngx_str_t *aux = ngx_http_push_stream_apply_template_to_each_line(&conf->footer_template, &NGX_HTTP_PUSH_STREAM_EVENTSOURCE_COMMENT_TEMPLATE, cf->pool);
            if (aux == NULL) {
                ngx_conf_log_error(NGX_LOG_ERR, cf, 0, "push_stream_message_module failed to apply template to footer message.");
                return NGX_CONF_ERROR;
            }

            conf->footer_template.data = aux->data;
            conf->footer_template.len = aux->len;
        }
    }


    // sanity checks
    // ping message interval cannot be zero
    if ((conf->ping_message_interval != NGX_CONF_UNSET_MSEC) && (conf->ping_message_interval == 0)) {
        ngx_conf_log_error(NGX_LOG_ERR, cf, 0, "push_stream_ping_message_interval cannot be zero.");
        return NGX_CONF_ERROR;
    }

    // subscriber connection ttl cannot be zero
    if ((conf->subscriber_connection_ttl != NGX_CONF_UNSET_MSEC) && (conf->subscriber_connection_ttl == 0)) {
        ngx_conf_log_error(NGX_LOG_ERR, cf, 0, "push_stream_subscriber_connection_ttl cannot be zero.");
        return NGX_CONF_ERROR;
    }

    // long polling connection ttl cannot be zero
    if ((conf->longpolling_connection_ttl != NGX_CONF_UNSET_MSEC) && (conf->longpolling_connection_ttl == 0)) {
        ngx_conf_log_error(NGX_LOG_ERR, cf, 0, "push_stream_longpolling_connection_ttl cannot be zero.");
        return NGX_CONF_ERROR;
    }

    // message template cannot be blank
    if (conf->message_template.len == 0) {
        ngx_conf_log_error(NGX_LOG_ERR, cf, 0, "push_stream_message_template cannot be blank.");
        return NGX_CONF_ERROR;
    }

    // broadcast channel max qtd cannot be zero
    if ((conf->broadcast_channel_max_qtd != NGX_CONF_UNSET_UINT) && (conf->broadcast_channel_max_qtd == 0)) {
        ngx_conf_log_error(NGX_LOG_ERR, cf, 0, "push_stream_broadcast_channel_max_qtd cannot be zero.");
        return NGX_CONF_ERROR;
    }

    // broadcast channel max qtd cannot be set without a channel prefix
    if ((conf->broadcast_channel_max_qtd != NGX_CONF_UNSET_UINT) && (conf->broadcast_channel_max_qtd > 0) && (ngx_http_push_stream_module_main_conf->broadcast_channel_prefix.len == 0)) {
        ngx_conf_log_error(NGX_LOG_ERR, cf, 0, "cannot set broadcast channel max qtd if push_stream_broadcast_channel_prefix is not set or blank.");
        return NGX_CONF_ERROR;
    }

    // max number of broadcast channels cannot be smaller than value in broadcast channel max qtd
    if ((ngx_http_push_stream_module_main_conf->max_number_of_broadcast_channels != NGX_CONF_UNSET_UINT) && (conf->broadcast_channel_max_qtd != NGX_CONF_UNSET_UINT) &&  (ngx_http_push_stream_module_main_conf->max_number_of_broadcast_channels < conf->broadcast_channel_max_qtd)) {
        ngx_conf_log_error(NGX_LOG_ERR, cf, 0, "max number of broadcast channels cannot be smaller than value in push_stream_broadcast_channel_max_qtd.");
        return NGX_CONF_ERROR;
    }

    // formatting header and footer template for chunk transfer
    if (conf->header_template.len > 0) {
        ngx_str_t *aux = NULL;
        if (conf->location_type == NGX_HTTP_PUSH_STREAM_WEBSOCKET_MODE) {
            aux = ngx_http_push_stream_get_formatted_websocket_frame(conf->header_template.data, conf->header_template.len, cf->pool);
        } else {
            aux = ngx_http_push_stream_get_formatted_chunk(conf->header_template.data, conf->header_template.len, cf->pool);
        }

        if (aux == NULL) {
            ngx_conf_log_error(NGX_LOG_ERR, cf, 0, "push stream module: unable to allocate memory to format header template");
            return NGX_CONF_ERROR;
        }
        conf->header_template.data = aux->data;
        conf->header_template.len = aux->len;
    }

    if (conf->footer_template.len > 0) {
        ngx_str_t *aux = NULL;
        if (conf->location_type == NGX_HTTP_PUSH_STREAM_WEBSOCKET_MODE) {
            aux = ngx_http_push_stream_get_formatted_websocket_frame(conf->footer_template.data, conf->footer_template.len, cf->pool);
        } else {
            aux = ngx_http_push_stream_get_formatted_chunk(conf->footer_template.data, conf->footer_template.len, cf->pool);
        }

        if (aux == NULL) {
            ngx_conf_log_error(NGX_LOG_ERR, cf, 0, "push stream module: unable to allocate memory to format footer template");
            return NGX_CONF_ERROR;
        }
        conf->footer_template.data = aux->data;
        conf->footer_template.len = aux->len;
    }

    if ((conf->location_type == NGX_HTTP_PUSH_STREAM_SUBSCRIBER_MODE_LONGPOLLING) ||
        (conf->location_type == NGX_HTTP_PUSH_STREAM_SUBSCRIBER_MODE_POLLING) ||
        (conf->location_type == NGX_HTTP_PUSH_STREAM_SUBSCRIBER_MODE_STREAMING) ||
        (conf->location_type == NGX_HTTP_PUSH_STREAM_WEBSOCKET_MODE)) {
        conf->message_template_index = ngx_http_push_stream_find_or_add_template(cf, conf->message_template, conf->eventsource_support, (conf->location_type == NGX_HTTP_PUSH_STREAM_WEBSOCKET_MODE));

        if (conf->padding_by_user_agent.len > 0) {
            if ((conf->paddings = ngx_http_push_stream_parse_paddings(cf, &conf->padding_by_user_agent)) == NULL) {
                ngx_conf_log_error(NGX_LOG_ERR, cf, 0, "push stream module: unable to parse paddings by user agent");
                return NGX_CONF_ERROR;
            }

            ngx_http_push_stream_padding_t *padding = conf->paddings;
            while ((padding = (ngx_http_push_stream_padding_t *) ngx_queue_next(&padding->queue)) != conf->paddings) {
                ngx_http_push_stream_padding_max_len = ngx_max(ngx_http_push_stream_padding_max_len, padding->header_min_len);
                ngx_http_push_stream_padding_max_len = ngx_max(ngx_http_push_stream_padding_max_len, padding->message_min_len);
            }
        }
    }

    return NGX_CONF_OK;
}


static char *
ngx_http_push_stream_setup_handler(ngx_conf_t *cf, void *conf, ngx_int_t (*handler) (ngx_http_request_t *))
{
    ngx_http_core_loc_conf_t            *clcf = ngx_http_conf_get_module_loc_conf(cf, ngx_http_core_module);
    ngx_http_push_stream_main_conf_t    *psmcf = ngx_http_conf_get_module_main_conf(cf, ngx_http_push_stream_module);

    psmcf->enabled = 1;
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
        pslcf->location_type = NGX_HTTP_PUSH_STREAM_STATISTICS_MODE;
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
    ngx_int_t                      *field = (ngx_int_t *) ((char *) conf + cmd->offset);
    if (*field != NGX_CONF_UNSET) {
        return "is duplicate";
    }

    *field = NGX_HTTP_PUSH_STREAM_PUBLISHER_MODE_NORMAL; //default
    if(cf->args->nelts > 1) {
        ngx_str_t                   value = (((ngx_str_t *) cf->args->elts)[1]);
        if ((value.len == NGX_HTTP_PUSH_STREAM_MODE_NORMAL.len) && (ngx_strncasecmp(value.data, NGX_HTTP_PUSH_STREAM_MODE_NORMAL.data, NGX_HTTP_PUSH_STREAM_MODE_NORMAL.len) == 0)) {
            *field = NGX_HTTP_PUSH_STREAM_PUBLISHER_MODE_NORMAL;
        } else if ((value.len == NGX_HTTP_PUSH_STREAM_MODE_ADMIN.len) && (ngx_strncasecmp(value.data, NGX_HTTP_PUSH_STREAM_MODE_ADMIN.data, NGX_HTTP_PUSH_STREAM_MODE_ADMIN.len) == 0)) {
            *field = NGX_HTTP_PUSH_STREAM_PUBLISHER_MODE_ADMIN;
        } else {
            ngx_conf_log_error(NGX_LOG_ERR, cf, 0, "invalid push_stream_publisher mode value: %V, accepted values (%s, %s)", &value, NGX_HTTP_PUSH_STREAM_MODE_NORMAL.data, NGX_HTTP_PUSH_STREAM_MODE_ADMIN.data);
            return NGX_CONF_ERROR;
        }
    }

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
    ngx_int_t                      *field = (ngx_int_t *) ((char *) conf + cmd->offset);
    if (*field != NGX_CONF_UNSET) {
        return "is duplicate";
    }

    *field = NGX_HTTP_PUSH_STREAM_SUBSCRIBER_MODE_STREAMING; //default
    if(cf->args->nelts > 1) {
        ngx_str_t                   value = (((ngx_str_t *) cf->args->elts)[1]);
        if ((value.len == NGX_HTTP_PUSH_STREAM_MODE_STREAMING.len) && (ngx_strncasecmp(value.data, NGX_HTTP_PUSH_STREAM_MODE_STREAMING.data, NGX_HTTP_PUSH_STREAM_MODE_STREAMING.len) == 0)) {
            *field = NGX_HTTP_PUSH_STREAM_SUBSCRIBER_MODE_STREAMING;
        } else if ((value.len == NGX_HTTP_PUSH_STREAM_MODE_POLLING.len) && (ngx_strncasecmp(value.data, NGX_HTTP_PUSH_STREAM_MODE_POLLING.data, NGX_HTTP_PUSH_STREAM_MODE_POLLING.len) == 0)) {
            *field = NGX_HTTP_PUSH_STREAM_SUBSCRIBER_MODE_POLLING;
        } else if ((value.len == NGX_HTTP_PUSH_STREAM_MODE_LONGPOLLING.len) && (ngx_strncasecmp(value.data, NGX_HTTP_PUSH_STREAM_MODE_LONGPOLLING.data, NGX_HTTP_PUSH_STREAM_MODE_LONGPOLLING.len) == 0)) {
            *field = NGX_HTTP_PUSH_STREAM_SUBSCRIBER_MODE_LONGPOLLING;
        } else {
            ngx_conf_log_error(NGX_LOG_ERR, cf, 0, "invalid push_stream_subscriber mode value: %V, accepted values (%s, %s, %s)", &value, NGX_HTTP_PUSH_STREAM_MODE_STREAMING.data, NGX_HTTP_PUSH_STREAM_MODE_POLLING.data, NGX_HTTP_PUSH_STREAM_MODE_LONGPOLLING.data);
            return NGX_CONF_ERROR;
        }
    }

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


static char *
ngx_http_push_stream_websocket(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    char *rc = ngx_http_push_stream_setup_handler(cf, conf, &ngx_http_push_stream_websocket_handler);
#if (NGX_HAVE_SHA1)
    if (rc == NGX_CONF_OK) {
        ngx_http_push_stream_loc_conf_t     *pslcf = conf;
        pslcf->location_type = NGX_HTTP_PUSH_STREAM_WEBSOCKET_MODE;
        pslcf->index_channels_path = ngx_http_get_variable_index(cf, &ngx_http_push_stream_channels_path);
        if (pslcf->index_channels_path == NGX_ERROR) {
            rc = NGX_CONF_ERROR;
        }
    }
#else
    rc = NGX_CONF_ERROR;
    ngx_conf_log_error(NGX_LOG_ERR, cf, 0, "push stream module: sha1 support is needed to use WebSocket");
#endif

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
    ngx_rbtree_node_t                   *sentinel;
    ngx_http_push_stream_shm_data_t     *d;

    if ((d = (ngx_http_push_stream_shm_data_t *) ngx_slab_alloc(shpool, sizeof(*d))) == NULL) { //shm_data plus an array.
        return NGX_ERROR;
    }
    shm_zone->data = d;
    ngx_queue_init(&d->messages_to_delete.queue);
    for (i = 0; i < NGX_MAX_PROCESSES; i++) {
        d->ipc[i].pid = -1;
        d->ipc[i].startup = 0;
        d->ipc[i].subscribers = 0;
        d->ipc[i].messages_queue = NULL;
        d->ipc[i].subscribers_sentinel = NULL;
    }

    d->startup = ngx_time();
    d->last_message_time = 0;
    d->last_message_tag = 0;

    // initialize rbtree
    if ((sentinel = ngx_slab_alloc(shpool, sizeof(*sentinel))) == NULL) {
        return NGX_ERROR;
    }
    ngx_rbtree_init(&d->tree, sentinel, ngx_http_push_stream_rbtree_insert);

    ngx_queue_init(&d->channels_queue);
    ngx_queue_init(&d->channels_to_delete);
    ngx_queue_init(&d->channels_trash);

    // create ping message
    if ((ngx_http_push_stream_ping_msg = ngx_http_push_stream_convert_char_to_msg_on_shared_locked(ngx_http_push_stream_module_main_conf->ping_message_text.data, ngx_http_push_stream_module_main_conf->ping_message_text.len, NULL, NGX_HTTP_PUSH_STREAM_PING_MESSAGE_ID, NULL, NULL, ngx_cycle->pool)) == NULL) {
        return NGX_ERROR;
    }

    return NGX_OK;
}
