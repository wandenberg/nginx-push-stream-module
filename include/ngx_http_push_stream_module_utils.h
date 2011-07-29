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
 * ngx_http_push_stream_module_utils.h
 *
 * Created: Oct 26, 2010
 * Authors: Wandenberg Peixoto <wandenberg@gmail.com>, Rogério Carvalho Schneider <stockrt@gmail.com>
 */

#ifndef NGX_HTTP_PUSH_STREAM_MODULE_UTILS_H_
#define NGX_HTTP_PUSH_STREAM_MODULE_UTILS_H_

#include <ngx_http_push_stream_module.h>
#include <ngx_http_push_stream_module_ipc.h>

typedef struct {
    char                 *subtype;
    size_t                len;
    ngx_str_t            *content_type;
    ngx_str_t            *format_item;
    ngx_str_t            *format_group_head;
    ngx_str_t            *format_group_item;
    ngx_str_t            *format_group_last_item;
    ngx_str_t            *format_group_tail;
    ngx_str_t            *format_summarized;
    ngx_str_t            *format_summarized_worker_item;
    ngx_str_t            *format_summarized_worker_last_item;
} ngx_http_push_stream_content_subtype_t;


#define  NGX_HTTP_PUSH_STREAM_CHANNEL_INFO_PLAIN_PATTERN "channel: %s" CRLF"published_messages: %ui" CRLF"stored_messages: %ui" CRLF"active_subscribers: %ui"
#define  NGX_HTTP_PUSH_STREAM_WORKER_INFO_PLAIN_PATTERN "  pid: %d" CRLF"  subscribers: %ui"
static ngx_str_t  NGX_HTTP_PUSH_STREAM_CHANNEL_INFO_PLAIN = ngx_string(NGX_HTTP_PUSH_STREAM_CHANNEL_INFO_PLAIN_PATTERN CRLF);
static ngx_str_t  NGX_HTTP_PUSH_STREAM_CHANNEL_INFO_GROUP_HEAD_PLAIN = ngx_string("hostname: %s, time: %s, channels: %ui, broadcast_channels: %ui, infos: " CRLF);
static ngx_str_t  NGX_HTTP_PUSH_STREAM_CHANNEL_INFO_GROUP_TAIL_PLAIN = ngx_string(CRLF);
static ngx_str_t  NGX_HTTP_PUSH_STREAM_CHANNEL_INFO_GROUP_ITEM_PLAIN = ngx_string(NGX_HTTP_PUSH_STREAM_CHANNEL_INFO_PLAIN_PATTERN "," CRLF);
static ngx_str_t  NGX_HTTP_PUSH_STREAM_CHANNEL_INFO_GROUP_LAST_ITEM_PLAIN = ngx_string(NGX_HTTP_PUSH_STREAM_CHANNEL_INFO_PLAIN_PATTERN);
static ngx_str_t  NGX_HTTP_PUSH_STREAM_CHANNELS_INFO_SUMMARIZED_PLAIN = ngx_string("hostname: %s" CRLF "time: %s" CRLF "channels: %ui" CRLF "broadcast_channels: %ui" CRLF "published_messages: %ui" CRLF "subscribers: %ui" CRLF "by_worker:"CRLF"%s" CRLF);
static ngx_str_t  NGX_HTTP_PUSH_STREAM_CHANNELS_INFO_SUMMARIZED_WORKER_ITEM_PLAIN = ngx_string(NGX_HTTP_PUSH_STREAM_WORKER_INFO_PLAIN_PATTERN "," CRLF);
static ngx_str_t  NGX_HTTP_PUSH_STREAM_CHANNELS_INFO_SUMMARIZED_WORKER_LAST_ITEM_PLAIN = ngx_string(NGX_HTTP_PUSH_STREAM_WORKER_INFO_PLAIN_PATTERN);
static ngx_str_t  NGX_HTTP_PUSH_STREAM_CONTENT_TYPE_PLAIN = ngx_string("text/plain");


#define  NGX_HTTP_PUSH_STREAM_CHANNEL_INFO_JSON_PATTERN "{\"channel\": \"%s\", \"published_messages\": \"%ui\", \"stored_messages\": \"%ui\", \"subscribers\": \"%ui\"}"
#define  NGX_HTTP_PUSH_STREAM_WORKER_INFO_JSON_PATTERN "{\"pid\": \"%d\", \"subscribers\": \"%ui\"}"
static ngx_str_t  NGX_HTTP_PUSH_STREAM_CHANNEL_INFO_JSON = ngx_string(NGX_HTTP_PUSH_STREAM_CHANNEL_INFO_JSON_PATTERN CRLF);
static ngx_str_t  NGX_HTTP_PUSH_STREAM_CHANNEL_INFO_GROUP_HEAD_JSON = ngx_string("{\"hostname\": \"%s\", \"time\": \"%s\", \"channels\": \"%ui\", \"broadcast_channels\": \"%ui\", \"infos\": [" CRLF);
static ngx_str_t  NGX_HTTP_PUSH_STREAM_CHANNEL_INFO_GROUP_TAIL_JSON = ngx_string("]}" CRLF);
static ngx_str_t  NGX_HTTP_PUSH_STREAM_CHANNEL_INFO_GROUP_ITEM_JSON = ngx_string(NGX_HTTP_PUSH_STREAM_CHANNEL_INFO_JSON_PATTERN "," CRLF);
static ngx_str_t  NGX_HTTP_PUSH_STREAM_CHANNEL_INFO_GROUP_LAST_ITEM_JSON = ngx_string(NGX_HTTP_PUSH_STREAM_CHANNEL_INFO_JSON_PATTERN CRLF);
static ngx_str_t  NGX_HTTP_PUSH_STREAM_CHANNELS_INFO_SUMMARIZED_JSON = ngx_string("{\"hostname\": \"%s\", \"time\": \"%s\", \"channels\": \"%ui\", \"broadcast_channels\": \"%ui\", \"published_messages\": \"%ui\", \"subscribers\": \"%ui\", \"by_worker\": [" CRLF "%s" CRLF"]}" CRLF);
static ngx_str_t  NGX_HTTP_PUSH_STREAM_CHANNELS_INFO_SUMMARIZED_WORKER_ITEM_JSON = ngx_string(NGX_HTTP_PUSH_STREAM_WORKER_INFO_JSON_PATTERN "," CRLF);
static ngx_str_t  NGX_HTTP_PUSH_STREAM_CHANNELS_INFO_SUMMARIZED_WORKER_LAST_ITEM_JSON = ngx_string(NGX_HTTP_PUSH_STREAM_WORKER_INFO_JSON_PATTERN);
static ngx_str_t  NGX_HTTP_PUSH_STREAM_CONTENT_TYPE_JSON = ngx_string("application/json");
static ngx_str_t  NGX_HTTP_PUSH_STREAM_CONTENT_TYPE_X_JSON = ngx_string("text/x-json");

#define  NGX_HTTP_PUSH_STREAM_CHANNEL_INFO_YAML_PATTERN "  channel: %s" CRLF"  published_messages: %ui" CRLF"  stored_messages: %ui" CRLF"  subscribers: %ui"
#define  NGX_HTTP_PUSH_STREAM_WORKER_INFO_YAML_PATTERN "    pid: %d" CRLF"    subscribers: %ui"
static ngx_str_t  NGX_HTTP_PUSH_STREAM_CHANNEL_INFO_YAML = ngx_string(NGX_HTTP_PUSH_STREAM_CHANNEL_INFO_YAML_PATTERN CRLF);
static ngx_str_t  NGX_HTTP_PUSH_STREAM_CHANNEL_INFO_GROUP_HEAD_YAML = ngx_string("hostname: %s" CRLF"time: %s" CRLF"channels: %ui" CRLF"broadcast_channels: %ui" CRLF"infos: "CRLF);
static ngx_str_t  NGX_HTTP_PUSH_STREAM_CHANNEL_INFO_GROUP_TAIL_YAML = ngx_string(CRLF);
static ngx_str_t  NGX_HTTP_PUSH_STREAM_CHANNEL_INFO_GROUP_ITEM_YAML = ngx_string(" -" CRLF NGX_HTTP_PUSH_STREAM_CHANNEL_INFO_YAML_PATTERN CRLF);
static ngx_str_t  NGX_HTTP_PUSH_STREAM_CHANNEL_INFO_GROUP_LAST_ITEM_YAML = ngx_string(" -" CRLF NGX_HTTP_PUSH_STREAM_CHANNEL_INFO_YAML_PATTERN);
static ngx_str_t  NGX_HTTP_PUSH_STREAM_CHANNELS_INFO_SUMMARIZED_YAML = ngx_string("  hostname: %s" CRLF"  time: %s" CRLF"  channels: %ui" CRLF"  broadcast_channels: %ui" CRLF"  published_messages: %ui" CRLF"  subscribers: %ui" CRLF "  by_worker:"CRLF"%s" CRLF);
static ngx_str_t  NGX_HTTP_PUSH_STREAM_CHANNELS_INFO_SUMMARIZED_WORKER_ITEM_YAML = ngx_string("   -" CRLF NGX_HTTP_PUSH_STREAM_WORKER_INFO_YAML_PATTERN CRLF);
static ngx_str_t  NGX_HTTP_PUSH_STREAM_CHANNELS_INFO_SUMMARIZED_WORKER_LAST_ITEM_YAML = ngx_string("   -" CRLF NGX_HTTP_PUSH_STREAM_WORKER_INFO_YAML_PATTERN);
static ngx_str_t  NGX_HTTP_PUSH_STREAM_CONTENT_TYPE_YAML = ngx_string("application/yaml");
static ngx_str_t  NGX_HTTP_PUSH_STREAM_CONTENT_TYPE_X_YAML = ngx_string("text/x-yaml");


#define  NGX_HTTP_PUSH_STREAM_CHANNEL_INFO_XML_PATTERN \
    "<channel>" CRLF \
    "  <name>%s</name>" CRLF \
    "  <published_messages>%ui</published_messages>" CRLF \
    "  <stored_messages>%ui</stored_messages>" CRLF \
    "  <subscribers>%ui</subscribers>" CRLF \
    "</channel>" CRLF
#define  NGX_HTTP_PUSH_STREAM_WORKER_INFO_XML_PATTERN \
    "<worker>" CRLF \
    "  <pid>%d</pid>" CRLF \
    "  <subscribers>%ui</subscribers>" CRLF \
    "</worker>" CRLF
static ngx_str_t  NGX_HTTP_PUSH_STREAM_CHANNEL_INFO_XML = ngx_string("<?xml version=\"1.0\" encoding=\"UTF-8\" ?>" CRLF NGX_HTTP_PUSH_STREAM_CHANNEL_INFO_XML_PATTERN CRLF);
static ngx_str_t  NGX_HTTP_PUSH_STREAM_CHANNEL_INFO_GROUP_HEAD_XML = ngx_string("<?xml version=\"1.0\" encoding=\"UTF-8\" ?>" CRLF "<root>" CRLF"  <hostname>%s</hostname>" CRLF"  <time>%s</time>" CRLF"  <channels>%ui</channels>" CRLF"  <broadcast_channels>%ui</broadcast_channels>" CRLF"  <infos>" CRLF);
static ngx_str_t  NGX_HTTP_PUSH_STREAM_CHANNEL_INFO_GROUP_TAIL_XML = ngx_string("  </infos>" CRLF"</root>" CRLF);
static ngx_str_t  NGX_HTTP_PUSH_STREAM_CHANNEL_INFO_GROUP_ITEM_XML = ngx_string(NGX_HTTP_PUSH_STREAM_CHANNEL_INFO_XML_PATTERN);
static ngx_str_t  NGX_HTTP_PUSH_STREAM_CHANNEL_INFO_GROUP_LAST_ITEM_XML = ngx_string(NGX_HTTP_PUSH_STREAM_CHANNEL_INFO_XML_PATTERN);
static ngx_str_t  NGX_HTTP_PUSH_STREAM_CHANNELS_INFO_SUMMARIZED_XML = ngx_string(
        "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>" CRLF \
        "<infos>" CRLF \
        "  <hostname>%s</hostname>" CRLF \
        "  <time>%s</time>" CRLF \
        "  <channels>%ui</channels>" CRLF \
        "  <broadcast_channels>%ui</broadcast_channels>" CRLF \
        "  <published_messages>%ui</published_messages>" CRLF \
        "  <subscribers>%ui</subscribers>" CRLF\
        "  <by_worker>%s</by_worker>" CRLF\
        "</infos>" CRLF);
static ngx_str_t  NGX_HTTP_PUSH_STREAM_CHANNELS_INFO_SUMMARIZED_WORKER_ITEM_XML = ngx_string(NGX_HTTP_PUSH_STREAM_WORKER_INFO_XML_PATTERN);
static ngx_str_t  NGX_HTTP_PUSH_STREAM_CHANNELS_INFO_SUMMARIZED_WORKER_LAST_ITEM_XML = ngx_string(NGX_HTTP_PUSH_STREAM_WORKER_INFO_XML_PATTERN);
static ngx_str_t  NGX_HTTP_PUSH_STREAM_CONTENT_TYPE_XML = ngx_string("application/xml");

static ngx_http_push_stream_content_subtype_t subtypes[] = {
    { "plain" , 5,
            &NGX_HTTP_PUSH_STREAM_CONTENT_TYPE_PLAIN,
            &NGX_HTTP_PUSH_STREAM_CHANNEL_INFO_PLAIN,
            &NGX_HTTP_PUSH_STREAM_CHANNEL_INFO_GROUP_HEAD_PLAIN,
            &NGX_HTTP_PUSH_STREAM_CHANNEL_INFO_GROUP_ITEM_PLAIN,
            &NGX_HTTP_PUSH_STREAM_CHANNEL_INFO_GROUP_LAST_ITEM_PLAIN,
            &NGX_HTTP_PUSH_STREAM_CHANNEL_INFO_GROUP_TAIL_PLAIN,
            &NGX_HTTP_PUSH_STREAM_CHANNELS_INFO_SUMMARIZED_PLAIN,
            &NGX_HTTP_PUSH_STREAM_CHANNELS_INFO_SUMMARIZED_WORKER_ITEM_PLAIN,
            &NGX_HTTP_PUSH_STREAM_CHANNELS_INFO_SUMMARIZED_WORKER_LAST_ITEM_PLAIN},
    { "json"  , 4,
            &NGX_HTTP_PUSH_STREAM_CONTENT_TYPE_JSON,
            &NGX_HTTP_PUSH_STREAM_CHANNEL_INFO_JSON,
            &NGX_HTTP_PUSH_STREAM_CHANNEL_INFO_GROUP_HEAD_JSON,
            &NGX_HTTP_PUSH_STREAM_CHANNEL_INFO_GROUP_ITEM_JSON,
            &NGX_HTTP_PUSH_STREAM_CHANNEL_INFO_GROUP_LAST_ITEM_JSON,
            &NGX_HTTP_PUSH_STREAM_CHANNEL_INFO_GROUP_TAIL_JSON,
            &NGX_HTTP_PUSH_STREAM_CHANNELS_INFO_SUMMARIZED_JSON,
            &NGX_HTTP_PUSH_STREAM_CHANNELS_INFO_SUMMARIZED_WORKER_ITEM_JSON,
            &NGX_HTTP_PUSH_STREAM_CHANNELS_INFO_SUMMARIZED_WORKER_LAST_ITEM_JSON },
    { "yaml"  , 4,
            &NGX_HTTP_PUSH_STREAM_CONTENT_TYPE_YAML,
            &NGX_HTTP_PUSH_STREAM_CHANNEL_INFO_YAML,
            &NGX_HTTP_PUSH_STREAM_CHANNEL_INFO_GROUP_HEAD_YAML,
            &NGX_HTTP_PUSH_STREAM_CHANNEL_INFO_GROUP_ITEM_YAML,
            &NGX_HTTP_PUSH_STREAM_CHANNEL_INFO_GROUP_LAST_ITEM_YAML,
            &NGX_HTTP_PUSH_STREAM_CHANNEL_INFO_GROUP_TAIL_YAML,
            &NGX_HTTP_PUSH_STREAM_CHANNELS_INFO_SUMMARIZED_YAML,
            &NGX_HTTP_PUSH_STREAM_CHANNELS_INFO_SUMMARIZED_WORKER_ITEM_YAML,
            &NGX_HTTP_PUSH_STREAM_CHANNELS_INFO_SUMMARIZED_WORKER_LAST_ITEM_YAML },
    { "xml"   , 3,
            &NGX_HTTP_PUSH_STREAM_CONTENT_TYPE_XML,
            &NGX_HTTP_PUSH_STREAM_CHANNEL_INFO_XML,
            &NGX_HTTP_PUSH_STREAM_CHANNEL_INFO_GROUP_HEAD_XML,
            &NGX_HTTP_PUSH_STREAM_CHANNEL_INFO_GROUP_ITEM_XML,
            &NGX_HTTP_PUSH_STREAM_CHANNEL_INFO_GROUP_LAST_ITEM_XML,
            &NGX_HTTP_PUSH_STREAM_CHANNEL_INFO_GROUP_TAIL_XML,
            &NGX_HTTP_PUSH_STREAM_CHANNELS_INFO_SUMMARIZED_XML,
            &NGX_HTTP_PUSH_STREAM_CHANNELS_INFO_SUMMARIZED_WORKER_ITEM_XML,
            &NGX_HTTP_PUSH_STREAM_CHANNELS_INFO_SUMMARIZED_WORKER_LAST_ITEM_XML },
    { "x-json", 6,
            &NGX_HTTP_PUSH_STREAM_CONTENT_TYPE_X_JSON,
            &NGX_HTTP_PUSH_STREAM_CHANNEL_INFO_JSON,
            &NGX_HTTP_PUSH_STREAM_CHANNEL_INFO_GROUP_HEAD_JSON,
            &NGX_HTTP_PUSH_STREAM_CHANNEL_INFO_GROUP_ITEM_JSON,
            &NGX_HTTP_PUSH_STREAM_CHANNEL_INFO_GROUP_LAST_ITEM_JSON,
            &NGX_HTTP_PUSH_STREAM_CHANNEL_INFO_GROUP_TAIL_JSON,
            &NGX_HTTP_PUSH_STREAM_CHANNELS_INFO_SUMMARIZED_JSON,
            &NGX_HTTP_PUSH_STREAM_CHANNELS_INFO_SUMMARIZED_WORKER_ITEM_JSON,
            &NGX_HTTP_PUSH_STREAM_CHANNELS_INFO_SUMMARIZED_WORKER_LAST_ITEM_JSON },
    { "x-yaml", 6,
            &NGX_HTTP_PUSH_STREAM_CONTENT_TYPE_X_YAML,
            &NGX_HTTP_PUSH_STREAM_CHANNEL_INFO_YAML,
            &NGX_HTTP_PUSH_STREAM_CHANNEL_INFO_GROUP_HEAD_YAML,
            &NGX_HTTP_PUSH_STREAM_CHANNEL_INFO_GROUP_ITEM_YAML,
            &NGX_HTTP_PUSH_STREAM_CHANNEL_INFO_GROUP_LAST_ITEM_YAML,
            &NGX_HTTP_PUSH_STREAM_CHANNEL_INFO_GROUP_TAIL_YAML,
            &NGX_HTTP_PUSH_STREAM_CHANNELS_INFO_SUMMARIZED_YAML,
            &NGX_HTTP_PUSH_STREAM_CHANNELS_INFO_SUMMARIZED_WORKER_ITEM_YAML,
            &NGX_HTTP_PUSH_STREAM_CHANNELS_INFO_SUMMARIZED_WORKER_LAST_ITEM_YAML }
};

static const ngx_int_t  NGX_HTTP_PUSH_STREAM_PING_MESSAGE_ID = -1;
static const ngx_str_t  NGX_HTTP_PUSH_STREAM_PING_MESSAGE_TEXT = ngx_string("");

static const ngx_int_t  NGX_HTTP_PUSH_STREAM_CHANNEL_DELETED_MESSAGE_ID = -2;
static const ngx_str_t  NGX_HTTP_PUSH_STREAM_CHANNEL_DELETED_MESSAGE_TEXT = ngx_string("Channel deleted");

static const ngx_str_t  NGX_HTTP_PUSH_STREAM_TOKEN_MESSAGE_ID = ngx_string("~id~");
static const ngx_str_t  NGX_HTTP_PUSH_STREAM_TOKEN_MESSAGE_CHANNEL = ngx_string("~channel~");
static const ngx_str_t  NGX_HTTP_PUSH_STREAM_TOKEN_MESSAGE_TEXT = ngx_string("~text~");

static const ngx_str_t  NGX_HTTP_PUSH_STREAM_LAST_CHUNK = ngx_string("0" CRLF CRLF);

ngx_event_t         ngx_http_push_stream_ping_event;
ngx_event_t         ngx_http_push_stream_disconnect_event;
ngx_event_t         ngx_http_push_stream_memory_cleanup_event;
ngx_event_t         ngx_http_push_stream_buffer_cleanup_event;

ngx_http_push_stream_msg_t *ngx_http_push_stream_ping_msg = NULL;

// general request handling
ngx_http_push_stream_msg_t *ngx_http_push_stream_convert_buffer_to_msg_on_shared_locked(ngx_buf_t *buf, ngx_http_push_stream_channel_t *channel, ngx_int_t id, ngx_pool_t *temp_pool);
ngx_http_push_stream_msg_t *ngx_http_push_stream_convert_char_to_msg_on_shared_locked(u_char *data, size_t len, ngx_http_push_stream_channel_t *channel, ngx_int_t id, ngx_pool_t *temp_pool);
static ngx_table_elt_t *    ngx_http_push_stream_add_response_header(ngx_http_request_t *r, const ngx_str_t *header_name, const ngx_str_t *header_value);
static ngx_int_t            ngx_http_push_stream_send_only_header_response(ngx_http_request_t *r, ngx_int_t status, const ngx_str_t *explain_error_message);
static u_char *             ngx_http_push_stream_str_replace(u_char *org, u_char *find, u_char *replace, ngx_uint_t offset, ngx_pool_t *temp_pool);
static ngx_str_t *          ngx_http_push_stream_get_formatted_chunk(const u_char *text, off_t len, ngx_pool_t *temp_pool);
static ngx_str_t *          ngx_http_push_stream_get_formatted_message(ngx_http_request_t *r, ngx_http_push_stream_channel_t *channel, ngx_http_push_stream_msg_t *msg, ngx_pool_t *temp_pool);
static ngx_str_t *          ngx_http_push_stream_format_message(ngx_http_push_stream_channel_t *channel, ngx_http_push_stream_msg_t *message, ngx_str_t *message_template, ngx_pool_t *temp_pool);
static ngx_int_t            ngx_http_push_stream_send_response_content_header(ngx_http_request_t *r, ngx_http_push_stream_loc_conf_t *pslcf);
static ngx_int_t            ngx_http_push_stream_send_response_text(ngx_http_request_t *r, const u_char *text, uint len, ngx_flag_t last_buffer);
static ngx_int_t            ngx_http_push_stream_send_ping(ngx_log_t *log, ngx_http_push_stream_loc_conf_t *pslcf);
static ngx_int_t            ngx_http_push_stream_memory_cleanup(ngx_log_t *log, ngx_http_push_stream_main_conf_t *psmcf);
static ngx_int_t            ngx_http_push_stream_buffer_cleanup(ngx_log_t *log, ngx_http_push_stream_loc_conf_t *pslcf);

static void                 ngx_http_push_stream_ping_timer_wake_handler(ngx_event_t *ev);
static void                 ngx_http_push_stream_ping_timer_set(ngx_http_push_stream_loc_conf_t *pslcf);
static void                 ngx_http_push_stream_disconnect_timer_wake_handler(ngx_event_t *ev);
static void                 ngx_http_push_stream_disconnect_timer_set(ngx_http_push_stream_loc_conf_t *pslcf);
static void                 ngx_http_push_stream_memory_cleanup_timer_wake_handler(ngx_event_t *ev);
static void                 ngx_http_push_stream_memory_cleanup_timer_set(ngx_http_push_stream_main_conf_t *psmcf);
static void                 ngx_http_push_stream_buffer_timer_wake_handler(ngx_event_t *ev);
static void                 ngx_http_push_stream_buffer_cleanup_timer_set(ngx_http_push_stream_loc_conf_t *pslcf);

static void                 ngx_http_push_stream_timer_reset(ngx_msec_t timer_interval, ngx_event_t *timer_event);


static void                 ngx_http_push_stream_worker_subscriber_cleanup_locked(ngx_http_push_stream_worker_subscriber_t *worker_subscriber);
u_char *                    ngx_http_push_stream_append_crlf(const ngx_str_t *str, ngx_pool_t *pool);
static ngx_str_t *          ngx_http_push_stream_create_str(ngx_pool_t *pool, uint len);

static void                 ngx_http_push_stream_mark_message_to_delete_locked(ngx_http_push_stream_msg_t *msg);
static void                 ngx_http_push_stream_delete_channel(ngx_str_t *id, ngx_pool_t *temp_pool);
static void                 ngx_http_push_stream_collect_expired_messages(ngx_http_push_stream_shm_data_t *data, ngx_slab_pool_t *shpool, ngx_rbtree_node_t *node, ngx_flag_t force);
static void                 ngx_http_push_stream_collect_expired_messages_and_empty_channels(ngx_http_push_stream_shm_data_t *data, ngx_slab_pool_t *shpool, ngx_rbtree_node_t *node, ngx_flag_t force);
static void                 ngx_http_push_stream_free_message_memory_locked(ngx_slab_pool_t *shpool, ngx_http_push_stream_msg_t *msg);
static ngx_int_t            ngx_http_push_stream_free_memory_of_expired_messages_and_channels(ngx_flag_t force);
static ngx_inline void      ngx_http_push_stream_ensure_qtd_of_messages_locked(ngx_http_push_stream_channel_t *channel, ngx_uint_t max_messages, ngx_flag_t expired);
static ngx_inline void      ngx_http_push_stream_delete_worker_channel(void);

static ngx_http_push_stream_content_subtype_t *     ngx_http_push_stream_match_channel_info_format_and_content_type(ngx_http_request_t *r, ngx_uint_t default_subtype);

static ngx_str_t *          ngx_http_push_stream_get_formatted_current_time(ngx_pool_t *pool);
static ngx_str_t *          ngx_http_push_stream_get_formatted_hostname(ngx_pool_t *pool);

#endif /* NGX_HTTP_PUSH_STREAM_MODULE_UTILS_H_ */
