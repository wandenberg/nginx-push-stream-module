/*
 * Copyright (C) 2010-2022 Wandenberg Peixoto <wandenberg@gmail.com>, Rogério Carvalho Schneider <stockrt@gmail.com>
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
ngx_flag_t ngx_http_push_stream_enabled = 0;

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

    /* Main directives*/
    { ngx_string("push_stream_shared_memory_size"),
        NGX_HTTP_MAIN_CONF|NGX_CONF_TAKE12,
        ngx_http_push_stream_set_shm_size_slot,
        NGX_HTTP_MAIN_CONF_OFFSET,
        0,
        NULL },
    { ngx_string("push_stream_channel_deleted_message_text"),
        NGX_HTTP_MAIN_CONF|NGX_CONF_TAKE1,
        ngx_conf_set_str_slot,
        NGX_HTTP_MAIN_CONF_OFFSET,
        offsetof(ngx_http_push_stream_main_conf_t, channel_deleted_message_text),
        NULL },
    { ngx_string("push_stream_channel_inactivity_time"),
        NGX_HTTP_MAIN_CONF|NGX_CONF_TAKE1,
        ngx_conf_set_sec_slot,
        NGX_HTTP_MAIN_CONF_OFFSET,
        offsetof(ngx_http_push_stream_main_conf_t, channel_inactivity_time),
        NULL },
    { ngx_string("push_stream_ping_message_text"),
        NGX_HTTP_MAIN_CONF|NGX_CONF_TAKE1,
        ngx_conf_set_str_slot,
        NGX_HTTP_MAIN_CONF_OFFSET,
        offsetof(ngx_http_push_stream_main_conf_t, ping_message_text),
        NULL },
    { ngx_string("push_stream_timeout_with_body"),
        NGX_HTTP_MAIN_CONF|NGX_CONF_TAKE1,
        ngx_conf_set_flag_slot,
        NGX_HTTP_MAIN_CONF_OFFSET,
        offsetof(ngx_http_push_stream_main_conf_t, timeout_with_body),
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
    { ngx_string("push_stream_max_number_of_wildcard_channels"),
        NGX_HTTP_MAIN_CONF|NGX_CONF_TAKE1,
        ngx_conf_set_num_slot,
        NGX_HTTP_MAIN_CONF_OFFSET,
        offsetof(ngx_http_push_stream_main_conf_t, max_number_of_wildcard_channels),
        NULL },
    { ngx_string("push_stream_wildcard_channel_prefix"),
        NGX_HTTP_MAIN_CONF|NGX_CONF_TAKE1,
        ngx_conf_set_str_slot,
        NGX_HTTP_MAIN_CONF_OFFSET,
        offsetof(ngx_http_push_stream_main_conf_t, wildcard_channel_prefix),
        NULL },
    { ngx_string("push_stream_events_channel_id"),
        NGX_HTTP_MAIN_CONF|NGX_CONF_TAKE1,
        ngx_conf_set_str_slot,
        NGX_HTTP_MAIN_CONF_OFFSET,
        offsetof(ngx_http_push_stream_main_conf_t, events_channel_id),
        NULL },

    /* Location directives */
    { ngx_string("push_stream_channels_path"),
        NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_HTTP_LIF_CONF|NGX_CONF_TAKE1,
        ngx_http_set_complex_value_slot,
        NGX_HTTP_LOC_CONF_OFFSET,
        offsetof(ngx_http_push_stream_loc_conf_t, channels_path),
        NULL },
    { ngx_string("push_stream_store_messages"),
        NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_HTTP_LIF_CONF|NGX_CONF_TAKE1,
        ngx_conf_set_flag_slot,
        NGX_HTTP_LOC_CONF_OFFSET,
        offsetof(ngx_http_push_stream_loc_conf_t, store_messages),
        NULL },
    { ngx_string("push_stream_channel_info_on_publish"),
        NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
        ngx_conf_set_flag_slot,
        NGX_HTTP_LOC_CONF_OFFSET,
        offsetof(ngx_http_push_stream_loc_conf_t, channel_info_on_publish),
        NULL },
    { ngx_string("push_stream_authorized_channels_only"),
        NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
        ngx_conf_set_flag_slot,
        NGX_HTTP_LOC_CONF_OFFSET,
        offsetof(ngx_http_push_stream_loc_conf_t, authorized_channels_only),
        NULL },
    { ngx_string("push_stream_header_template_file"),
        NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
        ngx_http_push_stream_set_header_template_from_file,
        NGX_HTTP_LOC_CONF_OFFSET,
        offsetof(ngx_http_push_stream_loc_conf_t, header_template),
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
    { ngx_string("push_stream_wildcard_channel_max_qtd"),
        NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
        ngx_conf_set_num_slot,
        NGX_HTTP_LOC_CONF_OFFSET,
        offsetof(ngx_http_push_stream_loc_conf_t, wildcard_channel_max_qtd),
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
        NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_HTTP_LIF_CONF|NGX_CONF_TAKE1,
        ngx_http_set_complex_value_slot,
        NGX_HTTP_LOC_CONF_OFFSET,
        offsetof(ngx_http_push_stream_loc_conf_t, last_received_message_time),
        NULL },
    { ngx_string("push_stream_last_received_message_tag"),
        NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_HTTP_LIF_CONF|NGX_CONF_TAKE1,
        ngx_http_set_complex_value_slot,
        NGX_HTTP_LOC_CONF_OFFSET,
        offsetof(ngx_http_push_stream_loc_conf_t, last_received_message_tag),
        NULL },
    { ngx_string("push_stream_last_event_id"),
        NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_HTTP_LIF_CONF|NGX_CONF_TAKE1,
        ngx_http_set_complex_value_slot,
        NGX_HTTP_LOC_CONF_OFFSET,
        offsetof(ngx_http_push_stream_loc_conf_t, last_event_id),
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
        ngx_http_set_complex_value_slot,
        NGX_HTTP_LOC_CONF_OFFSET,
        offsetof(ngx_http_push_stream_loc_conf_t, allowed_origins),
        NULL },
    { ngx_string("push_stream_allow_connections_to_events_channel"),
        NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_HTTP_LIF_CONF|NGX_CONF_TAKE1,
        ngx_conf_set_flag_slot,
        NGX_HTTP_LOC_CONF_OFFSET,
        offsetof(ngx_http_push_stream_loc_conf_t, allow_connections_to_events_channel),
        NULL },

    ngx_null_command
};


static ngx_http_module_t    ngx_http_push_stream_module_ctx = {
    ngx_http_push_stream_preconfig,             /* preconfiguration */
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

    if (!ngx_http_push_stream_enabled) {
        ngx_log_error(NGX_LOG_NOTICE, cycle->log, 0, "ngx_http_push_stream_module will not be used with this configuration.");
        return NGX_OK;
    }

    // initialize our little IPC
    ngx_int_t rc;
    if ((rc = ngx_http_push_stream_init_ipc(cycle, ccf->worker_processes)) == NGX_OK) {
        ngx_http_push_stream_alert_shutting_down_workers();
    }
    return rc;
}


static ngx_int_t
ngx_http_push_stream_init_worker(ngx_cycle_t *cycle)
{
    if (!ngx_http_push_stream_enabled) {
        return NGX_OK;
    }

    if ((ngx_process != NGX_PROCESS_SINGLE) && (ngx_process != NGX_PROCESS_WORKER)) {
        return NGX_OK;
    }

    if ((ngx_http_push_stream_ipc_init_worker()) != NGX_OK) {
        return NGX_ERROR;
    }

    // turn on timer to cleanup memory of old messages and channels
    ngx_http_push_stream_memory_cleanup_timer_set();

    return ngx_http_push_stream_register_worker_message_handler(cycle);
}


static void
ngx_http_push_stream_exit_master(ngx_cycle_t *cycle)
{
    if (!ngx_http_push_stream_enabled) {
        return;
    }

    // destroy channel tree in shared memory
    ngx_http_push_stream_collect_expired_messages_and_empty_channels(1);
    ngx_http_push_stream_free_memory_of_expired_messages_and_channels(1);
}


static void
ngx_http_push_stream_exit_worker(ngx_cycle_t *cycle)
{
    if (!ngx_http_push_stream_enabled) {
        return;
    }

    if ((ngx_process != NGX_PROCESS_SINGLE) && (ngx_process != NGX_PROCESS_WORKER)) {
        return;
    }

    ngx_http_push_stream_cleanup_shutting_down_worker();

    ngx_http_push_stream_ipc_exit_worker(cycle);
}


static ngx_int_t
ngx_http_push_stream_preconfig(ngx_conf_t *cf)
{
    size_t              size = ngx_align(2 * ngx_max(sizeof(ngx_http_push_stream_global_shm_data_t), ngx_pagesize), ngx_pagesize);
    ngx_shm_zone_t     *shm_zone = ngx_shared_memory_add(cf, &ngx_http_push_stream_global_shm_name, size, &ngx_http_push_stream_module);

    if (shm_zone == NULL) {
        return NGX_ERROR;
    }

    shm_zone->init = ngx_http_push_stream_init_global_shm_zone;
    shm_zone->data = (void *) 1;

    return NGX_OK;
}


static ngx_int_t
ngx_http_push_stream_postconfig(ngx_conf_t *cf)
{
    if ((ngx_http_push_stream_padding_max_len > 0) && (ngx_http_push_stream_module_paddings_chunks == NULL)) {
        ngx_uint_t steps = ngx_http_push_stream_padding_max_len / 100;
        if ((ngx_http_push_stream_module_paddings_chunks = ngx_pcalloc(cf->pool, sizeof(ngx_str_t) * (steps + 1))) == NULL) {
            ngx_conf_log_error(NGX_LOG_ERR, cf, 0, "push stream module: unable to allocate memory to create padding messages");
            return NGX_ERROR;
        }

        u_int padding_max_len = ngx_http_push_stream_padding_max_len + ((ngx_http_push_stream_padding_max_len % 2) ? 1 : 0);
        ngx_str_t *aux = ngx_http_push_stream_create_str(cf->pool, padding_max_len);
        if (aux == NULL) {
            ngx_conf_log_error(NGX_LOG_ERR, cf, 0, "push stream module: unable to allocate memory to create padding messages value");
            return NGX_ERROR;
        }

        while (padding_max_len > 0) {
            padding_max_len -= 2;
            ngx_memcpy(aux->data + padding_max_len, CRLF, 2);
        }

        ngx_int_t i, len = ngx_http_push_stream_padding_max_len;
        for (i = steps; i >= 0; i--) {
            ngx_str_t *padding = ngx_pcalloc(cf->pool, sizeof(ngx_str_t));
            if ((*(ngx_http_push_stream_module_paddings_chunks + i) = padding) == NULL) {
                ngx_conf_log_error(NGX_LOG_ERR, cf, 0, "push stream module: unable to allocate memory to create padding messages");
                return NGX_ERROR;
            }
            padding->data = &aux->data[aux->len - len];
            padding->len = len;
            len = i * 100;
        }
    }

    if ((ngx_http_push_stream_padding_max_len > 0) && (ngx_http_push_stream_module_paddings_chunks_for_eventsource == NULL)) {
        ngx_uint_t steps = ngx_http_push_stream_padding_max_len / 100;
        if ((ngx_http_push_stream_module_paddings_chunks_for_eventsource = ngx_pcalloc(cf->pool, sizeof(ngx_str_t) * (steps + 1))) == NULL) {
            ngx_conf_log_error(NGX_LOG_ERR, cf, 0, "push stream module: unable to allocate memory to create padding messages for eventsource");
            return NGX_ERROR;
        }

        u_int padding_max_len = ngx_http_push_stream_padding_max_len + ((ngx_http_push_stream_padding_max_len % 2) ? 1 : 0);
        ngx_str_t *aux = ngx_http_push_stream_create_str(cf->pool, padding_max_len);
        if (aux == NULL) {
            ngx_conf_log_error(NGX_LOG_ERR, cf, 0, "push stream module: unable to allocate memory to create padding messages value");
            return NGX_ERROR;
        }

        ngx_memset(aux->data, ':', padding_max_len);
        padding_max_len -= 1;
        ngx_memcpy(aux->data + padding_max_len, "\n", 1);

        ngx_int_t i, len = ngx_http_push_stream_padding_max_len;
        for (i = steps; i >= 0; i--) {
            ngx_str_t *padding = ngx_pcalloc(cf->pool, sizeof(ngx_str_t));
            if ((*(ngx_http_push_stream_module_paddings_chunks_for_eventsource + i) = padding) == NULL) {
                ngx_conf_log_error(NGX_LOG_ERR, cf, 0, "push stream module: unable to allocate memory to create padding messages");
                return NGX_ERROR;
            }
            padding->data = &aux->data[aux->len - len];
            padding->len = len;
            len = i * 100;
        }
    }

    return NGX_OK;
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
    ngx_str_null(&mcf->channel_deleted_message_text);
    mcf->channel_inactivity_time = NGX_CONF_UNSET;
    ngx_str_null(&mcf->ping_message_text);
    ngx_str_null(&mcf->wildcard_channel_prefix);
    mcf->max_number_of_channels = NGX_CONF_UNSET_UINT;
    mcf->max_number_of_wildcard_channels = NGX_CONF_UNSET_UINT;
    mcf->message_ttl = NGX_CONF_UNSET;
    mcf->max_channel_id_length = NGX_CONF_UNSET_UINT;
    mcf->max_subscribers_per_channel = NGX_CONF_UNSET;
    mcf->max_messages_stored_per_channel = NGX_CONF_UNSET_UINT;
    mcf->qtd_templates = 0;
    mcf->timeout_with_body = NGX_CONF_UNSET;
    ngx_str_null(&mcf->events_channel_id);
    mcf->ping_msg = NULL;
    mcf->longpooling_timeout_msg = NULL;
    ngx_queue_init(&mcf->msg_templates);

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
    ngx_conf_init_value(conf->channel_inactivity_time, NGX_HTTP_PUSH_STREAM_DEFAULT_CHANNEL_INACTIVITY_TIME);
    ngx_conf_merge_str_value(conf->channel_deleted_message_text, conf->channel_deleted_message_text, NGX_HTTP_PUSH_STREAM_CHANNEL_DELETED_MESSAGE_TEXT);
    ngx_conf_merge_str_value(conf->ping_message_text, conf->ping_message_text, NGX_HTTP_PUSH_STREAM_PING_MESSAGE_TEXT);
    ngx_conf_merge_str_value(conf->wildcard_channel_prefix, conf->wildcard_channel_prefix, NGX_HTTP_PUSH_STREAM_DEFAULT_WILDCARD_CHANNEL_PREFIX);
    ngx_conf_merge_str_value(conf->events_channel_id, conf->events_channel_id, NGX_HTTP_PUSH_STREAM_DEFAULT_EVENTS_CHANNEL_ID);
    ngx_conf_init_value(conf->timeout_with_body, 0);

    // sanity checks
    // shm size should be set
    if (conf->shm_zone == NULL) {
        ngx_conf_log_error(NGX_LOG_ERR, cf, 0, "push stream module: push_stream_shared_memory_size must be set.");
        return NGX_CONF_ERROR;
    }

    // max number of channels cannot be zero
    if ((conf->max_number_of_channels != NGX_CONF_UNSET_UINT) && (conf->max_number_of_channels == 0)) {
        ngx_conf_log_error(NGX_LOG_ERR, cf, 0, "push stream module: push_stream_max_number_of_channels cannot be zero.");
        return NGX_CONF_ERROR;
    }

    // max number of wildcard channels cannot be zero
    if ((conf->max_number_of_wildcard_channels != NGX_CONF_UNSET_UINT) && (conf->max_number_of_wildcard_channels == 0)) {
        ngx_conf_log_error(NGX_LOG_ERR, cf, 0, "push stream module: push_stream_max_number_of_wildcard_channels cannot be zero.");
        return NGX_CONF_ERROR;
    }

    // message ttl cannot be zero
    if ((conf->message_ttl != NGX_CONF_UNSET) && (conf->message_ttl == 0)) {
        ngx_conf_log_error(NGX_LOG_ERR, cf, 0, "push stream module: push_stream_message_ttl cannot be zero.");
        return NGX_CONF_ERROR;
    }

    // max subscriber per channel cannot be zero
    if ((conf->max_subscribers_per_channel != NGX_CONF_UNSET_UINT) && (conf->max_subscribers_per_channel == 0)) {
        ngx_conf_log_error(NGX_LOG_ERR, cf, 0, "push stream module: push_stream_max_subscribers_per_channel cannot be zero.");
        return NGX_CONF_ERROR;
    }

    // max messages stored per channel cannot be zero
    if ((conf->max_messages_stored_per_channel != NGX_CONF_UNSET_UINT) && (conf->max_messages_stored_per_channel == 0)) {
        ngx_conf_log_error(NGX_LOG_ERR, cf, 0, "push stream module: push_stream_max_messages_stored_per_channel cannot be zero.");
        return NGX_CONF_ERROR;
    }

    // max channel id length cannot be zero
    if ((conf->max_channel_id_length != NGX_CONF_UNSET_UINT) && (conf->max_channel_id_length == 0)) {
        ngx_conf_log_error(NGX_LOG_ERR, cf, 0, "push stream module: push_stream_max_channel_id_length cannot be zero.");
        return NGX_CONF_ERROR;
    }

    ngx_regex_compile_t *backtrack_parser = NULL;
    u_char               errstr[NGX_MAX_CONF_ERRSTR];

    if ((backtrack_parser = ngx_pcalloc(cf->pool, sizeof(ngx_regex_compile_t))) == NULL) {
        ngx_conf_log_error(NGX_LOG_ERR, cf, 0, "push stream module: push stream module: unable to allocate memory to compile backtrack parser");
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

    lcf->channels_path = NULL;
    lcf->authorized_channels_only = NGX_CONF_UNSET_UINT;
    lcf->store_messages = NGX_CONF_UNSET_UINT;
    lcf->message_template_index = -1;
    ngx_str_null(&lcf->message_template);
    ngx_str_null(&lcf->header_template);
    ngx_str_null(&lcf->footer_template);
    lcf->wildcard_channel_max_qtd = NGX_CONF_UNSET_UINT;
    lcf->location_type = NGX_CONF_UNSET_UINT;
    lcf->ping_message_interval = NGX_CONF_UNSET_MSEC;
    lcf->subscriber_connection_ttl = NGX_CONF_UNSET_MSEC;
    lcf->longpolling_connection_ttl = NGX_CONF_UNSET_MSEC;
    lcf->websocket_allow_publish = NGX_CONF_UNSET_UINT;
    lcf->channel_info_on_publish = NGX_CONF_UNSET_UINT;
    lcf->allow_connections_to_events_channel = NGX_CONF_UNSET_UINT;
    lcf->last_received_message_time = NULL;
    lcf->last_received_message_tag = NULL;
    lcf->last_event_id = NULL;
    lcf->user_agent = NULL;
    ngx_str_null(&lcf->padding_by_user_agent);
    lcf->paddings = NULL;
    lcf->allowed_origins = NULL;

    return lcf;
}


static char *
ngx_http_push_stream_merge_loc_conf(ngx_conf_t *cf, void *parent, void *child)
{
    ngx_http_push_stream_main_conf_t    *mcf = ngx_http_conf_get_module_main_conf(cf, ngx_http_push_stream_module);
    ngx_http_push_stream_loc_conf_t     *prev = parent, *conf = child;

    ngx_conf_merge_uint_value(conf->authorized_channels_only, prev->authorized_channels_only, 0);
    ngx_conf_merge_value(conf->store_messages, prev->store_messages, 0);
    ngx_conf_merge_str_value(conf->header_template, prev->header_template, NGX_HTTP_PUSH_STREAM_DEFAULT_HEADER_TEMPLATE);
    ngx_conf_merge_str_value(conf->message_template, prev->message_template, NGX_HTTP_PUSH_STREAM_DEFAULT_MESSAGE_TEMPLATE);
    ngx_conf_merge_str_value(conf->footer_template, prev->footer_template, NGX_HTTP_PUSH_STREAM_DEFAULT_FOOTER_TEMPLATE);
    ngx_conf_merge_uint_value(conf->wildcard_channel_max_qtd, prev->wildcard_channel_max_qtd, mcf->max_number_of_wildcard_channels);
    ngx_conf_merge_msec_value(conf->ping_message_interval, prev->ping_message_interval, NGX_CONF_UNSET_MSEC);
    ngx_conf_merge_msec_value(conf->subscriber_connection_ttl, prev->subscriber_connection_ttl, NGX_CONF_UNSET_MSEC);
    ngx_conf_merge_msec_value(conf->longpolling_connection_ttl, prev->longpolling_connection_ttl, conf->subscriber_connection_ttl);
    ngx_conf_merge_value(conf->websocket_allow_publish, prev->websocket_allow_publish, 0);
    ngx_conf_merge_value(conf->channel_info_on_publish, prev->channel_info_on_publish, 1);
    ngx_conf_merge_value(conf->allow_connections_to_events_channel, prev->allow_connections_to_events_channel, 0);
    ngx_conf_merge_str_value(conf->padding_by_user_agent, prev->padding_by_user_agent, NGX_HTTP_PUSH_STREAM_DEFAULT_PADDING_BY_USER_AGENT);
    ngx_conf_merge_uint_value(conf->location_type, prev->location_type, NGX_CONF_UNSET_UINT);

    if (conf->channels_path == NULL) {
        conf->channels_path = prev->channels_path;
    }

    if (conf->last_received_message_time == NULL) {
        conf->last_received_message_time = prev->last_received_message_time;
    }

    if (conf->last_received_message_tag == NULL) {
        conf->last_received_message_tag = prev->last_received_message_tag;
    }

    if (conf->last_event_id == NULL) {
        conf->last_event_id = prev->last_event_id;
    }

    if (conf->user_agent == NULL) {
        conf->user_agent = prev->user_agent;
    }

    if (conf->allowed_origins == NULL) {
        conf->allowed_origins = prev->allowed_origins ;
    }

    if (conf->location_type == NGX_CONF_UNSET_UINT) {
        return NGX_CONF_OK;
    }

    if (conf->channels_path == NULL) {
        ngx_conf_log_error(NGX_LOG_ERR, cf, 0, "push stream module: push_stream_channels_path must be set.");
        return NGX_CONF_ERROR;
    }

    // changing properties for event source support
    if (conf->location_type == NGX_HTTP_PUSH_STREAM_SUBSCRIBER_MODE_EVENTSOURCE) {
        // formatting header template
        if (ngx_strncmp(conf->header_template.data, NGX_HTTP_PUSH_STREAM_EVENTSOURCE_COMMENT_PREFIX.data, NGX_HTTP_PUSH_STREAM_EVENTSOURCE_COMMENT_PREFIX.len) != 0) {
            if (conf->header_template.len > 0) {
                ngx_str_t *aux = ngx_http_push_stream_apply_template_to_each_line(&conf->header_template, &NGX_HTTP_PUSH_STREAM_EVENTSOURCE_COMMENT_TEMPLATE, cf->pool);
                if (aux == NULL) {
                    ngx_conf_log_error(NGX_LOG_ERR, cf, 0, "push stream module: push_stream_message_module failed to apply template to header message.");
                    return NGX_CONF_ERROR;
                }
                conf->header_template.data = aux->data;
                conf->header_template.len = aux->len;
            } else {
                conf->header_template.data = NGX_HTTP_PUSH_STREAM_EVENTSOURCE_DEFAULT_HEADER_TEMPLATE.data;
                conf->header_template.len = NGX_HTTP_PUSH_STREAM_EVENTSOURCE_DEFAULT_HEADER_TEMPLATE.len;
            }
        }

        // formatting message template
        if (ngx_strncmp(conf->message_template.data, NGX_HTTP_PUSH_STREAM_EVENTSOURCE_MESSAGE_PREFIX.data, NGX_HTTP_PUSH_STREAM_EVENTSOURCE_MESSAGE_PREFIX.len) != 0) {
            ngx_str_t *aux = (conf->message_template.len > 0) ? &conf->message_template : (ngx_str_t *) &NGX_HTTP_PUSH_STREAM_TOKEN_MESSAGE_TEXT;
            ngx_str_t *template = ngx_http_push_stream_create_str(cf->pool, NGX_HTTP_PUSH_STREAM_EVENTSOURCE_MESSAGE_PREFIX.len + aux->len + 1);
            if (template == NULL) {
                ngx_conf_log_error(NGX_LOG_ERR, cf, 0, "push stream module: unable to allocate memory to append message prefix to message template");
                return NGX_CONF_ERROR;
            }
            u_char *last = ngx_copy(template->data, NGX_HTTP_PUSH_STREAM_EVENTSOURCE_MESSAGE_PREFIX.data, NGX_HTTP_PUSH_STREAM_EVENTSOURCE_MESSAGE_PREFIX.len);
            last = ngx_copy(last, aux->data, aux->len);
            ngx_memcpy(last, "\n", 1);

            conf->message_template.data = template->data;
            conf->message_template.len = template->len;
        }

        // formatting footer template
        if (ngx_strncmp(conf->footer_template.data, NGX_HTTP_PUSH_STREAM_EVENTSOURCE_COMMENT_PREFIX.data, NGX_HTTP_PUSH_STREAM_EVENTSOURCE_COMMENT_PREFIX.len) != 0) {
            if (conf->footer_template.len > 0) {
                ngx_str_t *aux = ngx_http_push_stream_apply_template_to_each_line(&conf->footer_template, &NGX_HTTP_PUSH_STREAM_EVENTSOURCE_COMMENT_TEMPLATE, cf->pool);
                if (aux == NULL) {
                    ngx_conf_log_error(NGX_LOG_ERR, cf, 0, "push stream module: push_stream_message_module failed to apply template to footer message.");
                    return NGX_CONF_ERROR;
                }

                conf->footer_template.data = aux->data;
                conf->footer_template.len = aux->len;
            }
        }
    } else if (conf->location_type == NGX_HTTP_PUSH_STREAM_SUBSCRIBER_MODE_WEBSOCKET) {
        // formatting header and footer template for chunk transfer
        if (conf->header_template.len > 0) {
            ngx_str_t *aux = ngx_http_push_stream_get_formatted_websocket_frame(&NGX_HTTP_PUSH_STREAM_WEBSOCKET_TEXT_LAST_FRAME_BYTE, sizeof(NGX_HTTP_PUSH_STREAM_WEBSOCKET_TEXT_LAST_FRAME_BYTE), conf->header_template.data, conf->header_template.len, cf->pool);
            if (aux == NULL) {
                ngx_conf_log_error(NGX_LOG_ERR, cf, 0, "push stream module: unable to allocate memory to format header template");
                return NGX_CONF_ERROR;
            }
            conf->header_template.data = aux->data;
            conf->header_template.len = aux->len;
        }

        if (conf->footer_template.len > 0) {
            ngx_str_t *aux = ngx_http_push_stream_get_formatted_websocket_frame(&NGX_HTTP_PUSH_STREAM_WEBSOCKET_TEXT_LAST_FRAME_BYTE, sizeof(NGX_HTTP_PUSH_STREAM_WEBSOCKET_TEXT_LAST_FRAME_BYTE), conf->footer_template.data, conf->footer_template.len, cf->pool);
            if (aux == NULL) {
                ngx_conf_log_error(NGX_LOG_ERR, cf, 0, "push stream module: unable to allocate memory to format footer template");
                return NGX_CONF_ERROR;
            }
            conf->footer_template.data = aux->data;
            conf->footer_template.len = aux->len;
        }
    }

    // sanity checks
    // ping message interval cannot be zero
    if ((conf->ping_message_interval != NGX_CONF_UNSET_MSEC) && (conf->ping_message_interval == 0)) {
        ngx_conf_log_error(NGX_LOG_ERR, cf, 0, "push stream module: push_stream_ping_message_interval cannot be zero.");
        return NGX_CONF_ERROR;
    }

    // subscriber connection ttl cannot be zero
    if ((conf->subscriber_connection_ttl != NGX_CONF_UNSET_MSEC) && (conf->subscriber_connection_ttl == 0)) {
        ngx_conf_log_error(NGX_LOG_ERR, cf, 0, "push stream module: push_stream_subscriber_connection_ttl cannot be zero.");
        return NGX_CONF_ERROR;
    }

    // long polling connection ttl cannot be zero
    if ((conf->longpolling_connection_ttl != NGX_CONF_UNSET_MSEC) && (conf->longpolling_connection_ttl == 0)) {
        ngx_conf_log_error(NGX_LOG_ERR, cf, 0, "push stream module: push_stream_longpolling_connection_ttl cannot be zero.");
        return NGX_CONF_ERROR;
    }

    // message template cannot be blank
    if (conf->message_template.len == 0) {
        ngx_conf_log_error(NGX_LOG_ERR, cf, 0, "push stream module: push_stream_message_template cannot be blank.");
        return NGX_CONF_ERROR;
    }

    // wildcard channel max qtd cannot be zero
    if ((conf->wildcard_channel_max_qtd != NGX_CONF_UNSET_UINT) && (conf->wildcard_channel_max_qtd == 0)) {
        ngx_conf_log_error(NGX_LOG_ERR, cf, 0, "push stream module: push_stream_wildcard_channel_max_qtd cannot be zero.");
        return NGX_CONF_ERROR;
    }

    // wildcard channel max qtd cannot be set without a channel prefix
    if ((conf->wildcard_channel_max_qtd != NGX_CONF_UNSET_UINT) && (conf->wildcard_channel_max_qtd > 0) && (mcf->wildcard_channel_prefix.len == 0)) {
        ngx_conf_log_error(NGX_LOG_ERR, cf, 0, "push stream module: cannot set wildcard channel max qtd if push_stream_wildcard_channel_prefix is not set or blank.");
        return NGX_CONF_ERROR;
    }

    // max number of wildcard channels cannot be smaller than value in wildcard channel max qtd
    if ((mcf->max_number_of_wildcard_channels != NGX_CONF_UNSET_UINT) && (conf->wildcard_channel_max_qtd != NGX_CONF_UNSET_UINT) &&  (mcf->max_number_of_wildcard_channels < conf->wildcard_channel_max_qtd)) {
        ngx_conf_log_error(NGX_LOG_ERR, cf, 0, "push stream module: max number of wildcard channels cannot be smaller than value in push_stream_wildcard_channel_max_qtd.");
        return NGX_CONF_ERROR;
    }

    if ((conf->location_type == NGX_HTTP_PUSH_STREAM_SUBSCRIBER_MODE_LONGPOLLING) ||
        (conf->location_type == NGX_HTTP_PUSH_STREAM_SUBSCRIBER_MODE_POLLING) ||
        (conf->location_type == NGX_HTTP_PUSH_STREAM_SUBSCRIBER_MODE_STREAMING) ||
        (conf->location_type == NGX_HTTP_PUSH_STREAM_SUBSCRIBER_MODE_EVENTSOURCE) ||
        (conf->location_type == NGX_HTTP_PUSH_STREAM_SUBSCRIBER_MODE_WEBSOCKET)) {
        if ((conf->message_template_index = ngx_http_push_stream_find_or_add_template(cf, conf->message_template, (conf->location_type == NGX_HTTP_PUSH_STREAM_SUBSCRIBER_MODE_EVENTSOURCE), (conf->location_type == NGX_HTTP_PUSH_STREAM_SUBSCRIBER_MODE_WEBSOCKET))) < 0) {
            ngx_conf_log_error(NGX_LOG_ERR, cf, 0, "push stream module: push stream module: unable to parse message template: %V", &conf->message_template);
            return NGX_CONF_ERROR;
        }


        if (conf->padding_by_user_agent.len > 0) {
            if ((conf->paddings = ngx_http_push_stream_parse_paddings(cf, &conf->padding_by_user_agent)) == NULL) {
                ngx_conf_log_error(NGX_LOG_ERR, cf, 0, "push stream module: push stream module: unable to parse paddings by user agent");
                return NGX_CONF_ERROR;
            }

            ngx_queue_t *q;
            for (q = ngx_queue_head(conf->paddings); q != ngx_queue_sentinel(conf->paddings); q = ngx_queue_next(q)) {
                ngx_http_push_stream_padding_t *padding = ngx_queue_data(q, ngx_http_push_stream_padding_t, queue);
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
    ngx_http_push_stream_main_conf_t    *mcf = ngx_http_conf_get_module_main_conf(cf, ngx_http_push_stream_module);

    ngx_http_push_stream_enabled = 1;
    mcf->enabled = 1;
    clcf->handler = handler;
    clcf->if_modified_since = NGX_HTTP_IMS_OFF;

    return NGX_CONF_OK;
}


static char *
ngx_http_push_stream_channels_statistics(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    char *rc = ngx_http_push_stream_setup_handler(cf, conf, &ngx_http_push_stream_channels_statistics_handler);

    if (rc == NGX_CONF_OK) {
        ngx_http_push_stream_loc_conf_t     *pslcf = conf;
        pslcf->location_type = NGX_HTTP_PUSH_STREAM_STATISTICS_MODE;
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
            ngx_conf_log_error(NGX_LOG_ERR, cf, 0, "push stream module: invalid push_stream_publisher mode value: %V, accepted values (%s, %s)", &value, NGX_HTTP_PUSH_STREAM_MODE_NORMAL.data, NGX_HTTP_PUSH_STREAM_MODE_ADMIN.data);
            return NGX_CONF_ERROR;
        }
    }

    return ngx_http_push_stream_setup_handler(cf, conf, &ngx_http_push_stream_publisher_handler);
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
        } else if ((value.len == NGX_HTTP_PUSH_STREAM_MODE_EVENTSOURCE.len) && (ngx_strncasecmp(value.data, NGX_HTTP_PUSH_STREAM_MODE_EVENTSOURCE.data, NGX_HTTP_PUSH_STREAM_MODE_EVENTSOURCE.len) == 0)) {
            *field = NGX_HTTP_PUSH_STREAM_SUBSCRIBER_MODE_EVENTSOURCE;
        } else if ((value.len == NGX_HTTP_PUSH_STREAM_MODE_WEBSOCKET.len) && (ngx_strncasecmp(value.data, NGX_HTTP_PUSH_STREAM_MODE_WEBSOCKET.data, NGX_HTTP_PUSH_STREAM_MODE_WEBSOCKET.len) == 0)) {
            *field = NGX_HTTP_PUSH_STREAM_SUBSCRIBER_MODE_WEBSOCKET;
        } else {
            ngx_conf_log_error(NGX_LOG_ERR, cf, 0, "push stream module: invalid push_stream_subscriber mode value: %V, accepted values (%V, %V, %V, %V, %V)", &value, &NGX_HTTP_PUSH_STREAM_MODE_STREAMING, &NGX_HTTP_PUSH_STREAM_MODE_POLLING, &NGX_HTTP_PUSH_STREAM_MODE_LONGPOLLING, &NGX_HTTP_PUSH_STREAM_MODE_EVENTSOURCE, &NGX_HTTP_PUSH_STREAM_MODE_WEBSOCKET);
            return NGX_CONF_ERROR;
        }
    }

    if (*field == NGX_HTTP_PUSH_STREAM_SUBSCRIBER_MODE_WEBSOCKET) {
        char *rc = ngx_http_push_stream_setup_handler(cf, conf, &ngx_http_push_stream_websocket_handler);
#if (NGX_HAVE_SHA1)
        if (rc == NGX_CONF_OK) {
            ngx_http_push_stream_loc_conf_t     *pslcf = conf;
            pslcf->location_type = NGX_HTTP_PUSH_STREAM_SUBSCRIBER_MODE_WEBSOCKET;
        }
#else
        rc = NGX_CONF_ERROR;
        ngx_conf_log_error(NGX_LOG_ERR, cf, 0, "push stream module: push stream module: sha1 support is needed to use WebSocket");
#endif
        return rc;
    }
    return ngx_http_push_stream_setup_handler(cf, conf, &ngx_http_push_stream_subscriber_handler);
}


// shared memory
char *
ngx_http_push_stream_set_shm_size_slot(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_push_stream_main_conf_t    *mcf = ngx_http_conf_get_module_main_conf(cf, ngx_http_push_stream_module);
    size_t                               shm_size;
    size_t                               shm_size_limit = 32 * ngx_pagesize;
    ngx_str_t                           *value;
    ngx_str_t                           *name;

    value = cf->args->elts;

    shm_size = ngx_align(ngx_parse_size(&value[1]), ngx_pagesize);
    if (shm_size < shm_size_limit) {
        ngx_conf_log_error(NGX_LOG_WARN, cf, 0, "The push_stream_shared_memory_size value must be at least %ulKiB", shm_size_limit >> 10);
        return NGX_CONF_ERROR;
    }

    name = (cf->args->nelts > 2) ? &value[2] : &ngx_http_push_stream_shm_name;
    if ((ngx_http_push_stream_global_shm_zone != NULL) && (ngx_http_push_stream_global_shm_zone->data != NULL)) {
        ngx_http_push_stream_global_shm_data_t *global_data = (ngx_http_push_stream_global_shm_data_t *) ngx_http_push_stream_global_shm_zone->data;
        ngx_queue_t                            *q;

        for (q = ngx_queue_head(&global_data->shm_datas_queue); q != ngx_queue_sentinel(&global_data->shm_datas_queue); q = ngx_queue_next(q)) {
            ngx_http_push_stream_shm_data_t *data = ngx_queue_data(q, ngx_http_push_stream_shm_data_t, shm_data_queue);
            if ((name->len == data->shm_zone->shm.name.len) &&
                (ngx_strncmp(name->data, data->shm_zone->shm.name.data, name->len) == 0) &&
                (data->shm_zone->shm.size != shm_size)) {
                shm_size = data->shm_zone->shm.size;
                ngx_conf_log_error(NGX_LOG_WARN, cf, 0, "Cannot change memory area size without restart, ignoring change on zone: %V", name);
            }
        }
    }
    ngx_conf_log_error(NGX_LOG_INFO, cf, 0, "Using %udKiB of shared memory for push stream module on zone: %V", shm_size >> 10, name);

    mcf->shm_zone = ngx_shared_memory_add(cf, name, shm_size, &ngx_http_push_stream_module);

    if (mcf->shm_zone == NULL) {
        return NGX_CONF_ERROR;
    }

    if (mcf->shm_zone->data) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0, "duplicate zone \"%V\"", name);
        return NGX_CONF_ERROR;
    }

    mcf->shm_zone->init = ngx_http_push_stream_init_shm_zone;
    mcf->shm_zone->data = mcf;

    return NGX_CONF_OK;
}


char *
ngx_http_push_stream_set_header_template_from_file(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_str_t                      *field = (ngx_str_t *) ((char *) conf + cmd->offset);

    if (field->data != NULL) {
        return "is duplicate or template set by 'push_stream_header_template'";
    }

    ngx_str_t                      *value = &(((ngx_str_t *) cf->args->elts)[1]);
    ngx_file_t                      file;
    ngx_file_info_t                 fi;
    ssize_t                         n;

    ngx_memzero(&file, sizeof(ngx_file_t));
    file.name = *value;
    file.log = cf->log;

    file.fd = ngx_open_file(value->data, NGX_FILE_RDONLY, NGX_FILE_OPEN, 0);
    if (file.fd == NGX_INVALID_FILE) {
        ngx_conf_log_error(NGX_LOG_ERR, cf, 0, "push stream module: unable to open file \"%V\" for header template", value);
        return NGX_CONF_ERROR;
    }

    if (ngx_fd_info(file.fd, &fi) == NGX_FILE_ERROR) {
        ngx_conf_log_error(NGX_LOG_ERR, cf, 0, "push stream module: unable to stat file \"%V\" for header template", value);
        ngx_close_file(file.fd);
        return NGX_CONF_ERROR;
    }

    field->len = (size_t) ngx_file_size(&fi);

    field->data = ngx_pcalloc(cf->pool, field->len);
    if (field->data == NULL) {
        ngx_conf_log_error(NGX_LOG_ERR, cf, 0, "push stream module: unable to allocate memory to read header template file", value);
        ngx_close_file(file.fd);
        return NGX_CONF_ERROR;
    }

    n = ngx_read_file(&file, field->data, field->len, 0);
    if (n == NGX_ERROR) {
        ngx_conf_log_error(NGX_LOG_ERR, cf, 0, "push stream module: unable to read data from file \"%V\" for header template", value);
        ngx_close_file(file.fd);
        return NGX_CONF_ERROR;
    }

    if ((size_t) n != field->len) {
        ngx_conf_log_error(NGX_LOG_ERR, cf, 0,  "push stream module: returned only %z bytes instead of %z from file \"%V\"", n, field->len, value);
        ngx_close_file(file.fd);
        return NGX_CONF_ERROR;
    }

    if (ngx_close_file(file.fd) == NGX_FILE_ERROR) {
        ngx_conf_log_error(NGX_LOG_ERR, cf, 0, "push stream module: unable to close file \"%V\" for header template", value);
        return NGX_CONF_ERROR;
    }

    return NGX_CONF_OK;
}


// shared memory zone initializer
ngx_int_t
ngx_http_push_stream_init_global_shm_zone(ngx_shm_zone_t *shm_zone, void *data)
{
    ngx_slab_pool_t                            *shpool = (ngx_slab_pool_t *) shm_zone->shm.addr;
    ngx_http_push_stream_global_shm_data_t     *d;
    int i;

    if (data) { /* zone already initialized */
        shm_zone->data = data;
        ngx_queue_init(&((ngx_http_push_stream_global_shm_data_t *) data)->shm_datas_queue);
        ngx_http_push_stream_global_shm_zone = shm_zone;
        return NGX_OK;
    }

    if ((d = (ngx_http_push_stream_global_shm_data_t *) ngx_slab_alloc(shpool, sizeof(*d))) == NULL) { //shm_data plus an array.
        return NGX_ERROR;
    }
    shm_zone->data = d;
    for (i = 0; i < NGX_MAX_PROCESSES; i++) {
        d->pid[i] = -1;
    }

    ngx_queue_init(&d->shm_datas_queue);

    ngx_http_push_stream_global_shm_zone = shm_zone;

    return NGX_OK;
}


ngx_int_t
ngx_http_push_stream_init_shm_zone(ngx_shm_zone_t *shm_zone, void *data)
{
    ngx_http_push_stream_global_shm_data_t *global_shm_data = (ngx_http_push_stream_global_shm_data_t *) ngx_http_push_stream_global_shm_zone->data;
    ngx_http_push_stream_main_conf_t       *mcf = shm_zone->data;
    ngx_http_push_stream_shm_data_t        *d;
    int i;

    mcf->shm_zone = shm_zone;
    mcf->shpool = (ngx_slab_pool_t *) shm_zone->shm.addr;

    if (data) { /* zone already initialized */
        shm_zone->data = data;
        d = (ngx_http_push_stream_shm_data_t *) data;
        d->mcf = mcf;
        d->shm_zone = shm_zone;
        d->shpool = mcf->shpool;
        mcf->shm_data = data;
        ngx_queue_insert_tail(&global_shm_data->shm_datas_queue, &d->shm_data_queue);
        return NGX_OK;
    }

    ngx_rbtree_node_t                   *sentinel;

    if ((d = (ngx_http_push_stream_shm_data_t *) ngx_slab_alloc(mcf->shpool, sizeof(*d))) == NULL) { //shm_data plus an array.
        return NGX_ERROR;
    }
    d->mcf = mcf;
    mcf->shm_data = d;
    shm_zone->data = d;
    for (i = 0; i < NGX_MAX_PROCESSES; i++) {
        d->ipc[i].pid = -1;
        d->ipc[i].startup = 0;
        d->ipc[i].subscribers = 0;
        ngx_queue_init(&d->ipc[i].messages_queue);
        ngx_queue_init(&d->ipc[i].subscribers_queue);
    }

    d->channels = 0;
    d->wildcard_channels = 0;
    d->published_messages = 0;
    d->stored_messages = 0;
    d->subscribers = 0;
    d->channels_in_delete = 0;
    d->channels_in_trash = 0;
    d->messages_in_trash = 0;
    d->startup = ngx_time();
    d->last_message_time = 0;
    d->last_message_tag = 0;
    d->shm_zone = shm_zone;
    d->shpool = mcf->shpool;
    d->slots_for_census = 0;
    d->events_channel = NULL;

    // initialize rbtree
    if ((sentinel = ngx_slab_alloc(mcf->shpool, sizeof(*sentinel))) == NULL) {
        return NGX_ERROR;
    }
    ngx_rbtree_init(&d->tree, sentinel, ngx_http_push_stream_rbtree_insert);

    ngx_queue_init(&d->messages_trash);
    ngx_queue_init(&d->channels_queue);
    ngx_queue_init(&d->channels_to_delete);
    ngx_queue_init(&d->channels_trash);

    ngx_queue_insert_tail(&global_shm_data->shm_datas_queue, &d->shm_data_queue);

    if (ngx_http_push_stream_create_shmtx(&d->messages_trash_mutex, &d->messages_trash_lock, (u_char *) "push_stream_messages_trash") != NGX_OK) {
        return NGX_ERROR;
    }

    if (ngx_http_push_stream_create_shmtx(&d->channels_queue_mutex, &d->channels_queue_lock, (u_char *) "push_stream_channels_queue") != NGX_OK) {
        return NGX_ERROR;
    }

    if (ngx_http_push_stream_create_shmtx(&d->channels_to_delete_mutex, &d->channels_to_delete_lock, (u_char *) "push_stream_channels_to_delete") != NGX_OK) {
        return NGX_ERROR;
    }

    if (ngx_http_push_stream_create_shmtx(&d->channels_trash_mutex, &d->channels_trash_lock, (u_char *) "push_stream_channels_trash") != NGX_OK) {
        return NGX_ERROR;
    }

    if (ngx_http_push_stream_create_shmtx(&d->cleanup_mutex, &d->cleanup_lock, (u_char *) "push_stream_cleanup") != NGX_OK) {
        return NGX_ERROR;
    }

    u_char lock_name[25];
    for (i = 0; i < 10; i++) {
        ngx_sprintf(lock_name, "push_stream_channels_%d%Z", i);
        if (ngx_http_push_stream_create_shmtx(&d->channels_mutex[i], &d->channels_lock[i], lock_name) != NGX_OK) {
            return NGX_ERROR;
        }
    }

    d->mutex_round_robin = 0;

    if (mcf->events_channel_id.len > 0) {
        if ((d->events_channel = ngx_http_push_stream_get_channel(&mcf->events_channel_id, ngx_cycle->log, mcf)) == NULL) {
            ngx_log_error(NGX_LOG_ERR, ngx_cycle->log, 0, "push stream module: unable to create events channel");
            return NGX_ERROR;
        }

        if (ngx_http_push_stream_create_shmtx(&d->events_channel_mutex, &d->events_channel_lock, (u_char *) "push_stream_events_channel") != NGX_OK) {
            return NGX_ERROR;
        }

        d->events_channel->mutex = &d->events_channel_mutex;
    }

    return NGX_OK;
}
