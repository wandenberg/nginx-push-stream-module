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
    ngx_queue_t           queue;
    ngx_str_t            *line;
} ngx_http_push_stream_line_t;

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
#define  NGX_HTTP_PUSH_STREAM_WORKER_INFO_PLAIN_PATTERN "  pid: %d" CRLF"  subscribers: %ui" CRLF"  uptime: %ui"
static ngx_str_t  NGX_HTTP_PUSH_STREAM_CHANNEL_INFO_PLAIN = ngx_string(NGX_HTTP_PUSH_STREAM_CHANNEL_INFO_PLAIN_PATTERN CRLF);
static ngx_str_t  NGX_HTTP_PUSH_STREAM_CHANNEL_INFO_GROUP_HEAD_PLAIN = ngx_string("hostname: %s, time: %s, channels: %ui, wildcard_channels: %ui, uptime: %ui, infos: " CRLF);
static ngx_str_t  NGX_HTTP_PUSH_STREAM_CHANNEL_INFO_GROUP_TAIL_PLAIN = ngx_string(CRLF);
static ngx_str_t  NGX_HTTP_PUSH_STREAM_CHANNEL_INFO_GROUP_ITEM_PLAIN = ngx_string(NGX_HTTP_PUSH_STREAM_CHANNEL_INFO_PLAIN_PATTERN "," CRLF);
static ngx_str_t  NGX_HTTP_PUSH_STREAM_CHANNEL_INFO_GROUP_LAST_ITEM_PLAIN = ngx_string(NGX_HTTP_PUSH_STREAM_CHANNEL_INFO_PLAIN_PATTERN);
static ngx_str_t  NGX_HTTP_PUSH_STREAM_CHANNELS_INFO_SUMMARIZED_PLAIN = ngx_string("hostname: %s" CRLF "time: %s" CRLF "channels: %ui" CRLF "wildcard_channels: %ui" CRLF "published_messages: %ui" CRLF "stored_messages: %ui" CRLF "messages_in_trash: %ui" CRLF "channels_in_delete: %ui" CRLF "channels_in_trash: %ui" CRLF "subscribers: %ui" CRLF "uptime: %ui" CRLF "by_worker:"CRLF"%s" CRLF);
static ngx_str_t  NGX_HTTP_PUSH_STREAM_CHANNELS_INFO_SUMMARIZED_WORKER_ITEM_PLAIN = ngx_string(NGX_HTTP_PUSH_STREAM_WORKER_INFO_PLAIN_PATTERN "," CRLF);
static ngx_str_t  NGX_HTTP_PUSH_STREAM_CHANNELS_INFO_SUMMARIZED_WORKER_LAST_ITEM_PLAIN = ngx_string(NGX_HTTP_PUSH_STREAM_WORKER_INFO_PLAIN_PATTERN);
static ngx_str_t  NGX_HTTP_PUSH_STREAM_CONTENT_TYPE_PLAIN = ngx_string("text/plain");


#define  NGX_HTTP_PUSH_STREAM_CHANNEL_INFO_JSON_PATTERN "{\"channel\": \"%s\", \"published_messages\": %ui, \"stored_messages\": %ui, \"subscribers\": %ui}"
#define  NGX_HTTP_PUSH_STREAM_WORKER_INFO_JSON_PATTERN "{\"pid\": \"%d\", \"subscribers\": %ui, \"uptime\": %ui}"
static ngx_str_t  NGX_HTTP_PUSH_STREAM_CHANNEL_INFO_JSON = ngx_string(NGX_HTTP_PUSH_STREAM_CHANNEL_INFO_JSON_PATTERN CRLF);
static ngx_str_t  NGX_HTTP_PUSH_STREAM_CHANNEL_INFO_GROUP_HEAD_JSON = ngx_string("{\"hostname\": \"%s\", \"time\": \"%s\", \"channels\": %ui, \"wildcard_channels\": %ui, \"uptime\": %ui, \"infos\": [" CRLF);
static ngx_str_t  NGX_HTTP_PUSH_STREAM_CHANNEL_INFO_GROUP_TAIL_JSON = ngx_string("]}" CRLF);
static ngx_str_t  NGX_HTTP_PUSH_STREAM_CHANNEL_INFO_GROUP_ITEM_JSON = ngx_string(NGX_HTTP_PUSH_STREAM_CHANNEL_INFO_JSON_PATTERN "," CRLF);
static ngx_str_t  NGX_HTTP_PUSH_STREAM_CHANNEL_INFO_GROUP_LAST_ITEM_JSON = ngx_string(NGX_HTTP_PUSH_STREAM_CHANNEL_INFO_JSON_PATTERN CRLF);
static ngx_str_t  NGX_HTTP_PUSH_STREAM_CHANNELS_INFO_SUMMARIZED_JSON = ngx_string("{\"hostname\": \"%s\", \"time\": \"%s\", \"channels\": %ui, \"wildcard_channels\": %ui, \"published_messages\": %ui, \"stored_messages\": %ui, \"messages_in_trash\": %ui, \"channels_in_delete\": %ui, \"channels_in_trash\": %ui, \"subscribers\": %ui, \"uptime\": %ui, \"by_worker\": [" CRLF "%s" CRLF"]}" CRLF);
static ngx_str_t  NGX_HTTP_PUSH_STREAM_CHANNELS_INFO_SUMMARIZED_WORKER_ITEM_JSON = ngx_string(NGX_HTTP_PUSH_STREAM_WORKER_INFO_JSON_PATTERN "," CRLF);
static ngx_str_t  NGX_HTTP_PUSH_STREAM_CHANNELS_INFO_SUMMARIZED_WORKER_LAST_ITEM_JSON = ngx_string(NGX_HTTP_PUSH_STREAM_WORKER_INFO_JSON_PATTERN);
static ngx_str_t  NGX_HTTP_PUSH_STREAM_CONTENT_TYPE_JSON = ngx_string("application/json");
static ngx_str_t  NGX_HTTP_PUSH_STREAM_CONTENT_TYPE_X_JSON = ngx_string("text/x-json");

#define  NGX_HTTP_PUSH_STREAM_CHANNEL_INFO_YAML_PATTERN "  channel: %s" CRLF"  published_messages: %ui" CRLF"  stored_messages: %ui" CRLF"  subscribers: %ui"
#define  NGX_HTTP_PUSH_STREAM_WORKER_INFO_YAML_PATTERN "    pid: %d" CRLF"    subscribers: %ui" CRLF"    uptime: %ui"
static ngx_str_t  NGX_HTTP_PUSH_STREAM_CHANNEL_INFO_YAML = ngx_string(NGX_HTTP_PUSH_STREAM_CHANNEL_INFO_YAML_PATTERN CRLF);
static ngx_str_t  NGX_HTTP_PUSH_STREAM_CHANNEL_INFO_GROUP_HEAD_YAML = ngx_string("hostname: %s" CRLF"time: %s" CRLF"channels: %ui" CRLF"wildcard_channels: %ui" CRLF"uptime: %ui" CRLF"infos: "CRLF);
static ngx_str_t  NGX_HTTP_PUSH_STREAM_CHANNEL_INFO_GROUP_TAIL_YAML = ngx_string(CRLF);
static ngx_str_t  NGX_HTTP_PUSH_STREAM_CHANNEL_INFO_GROUP_ITEM_YAML = ngx_string(" -" CRLF NGX_HTTP_PUSH_STREAM_CHANNEL_INFO_YAML_PATTERN CRLF);
static ngx_str_t  NGX_HTTP_PUSH_STREAM_CHANNEL_INFO_GROUP_LAST_ITEM_YAML = ngx_string(" -" CRLF NGX_HTTP_PUSH_STREAM_CHANNEL_INFO_YAML_PATTERN);
static ngx_str_t  NGX_HTTP_PUSH_STREAM_CHANNELS_INFO_SUMMARIZED_YAML = ngx_string("  hostname: %s" CRLF"  time: %s" CRLF"  channels: %ui" CRLF"  wildcard_channels: %ui" CRLF"  published_messages: %ui" CRLF"  stored_messages: %ui" CRLF"  messages_in_trash: %ui" CRLF"  channels_in_delete: %ui" CRLF"  channels_in_trash: %ui" CRLF"  subscribers: %ui" CRLF"  uptime: %ui" CRLF"  by_worker:"CRLF"%s" CRLF);
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
    "  <uptime>%ui</uptime>" CRLF \
    "</worker>" CRLF
static ngx_str_t  NGX_HTTP_PUSH_STREAM_CHANNEL_INFO_XML = ngx_string("<?xml version=\"1.0\" encoding=\"UTF-8\" ?>" CRLF NGX_HTTP_PUSH_STREAM_CHANNEL_INFO_XML_PATTERN CRLF);
static ngx_str_t  NGX_HTTP_PUSH_STREAM_CHANNEL_INFO_GROUP_HEAD_XML = ngx_string("<?xml version=\"1.0\" encoding=\"UTF-8\" ?>" CRLF "<root>" CRLF"  <hostname>%s</hostname>" CRLF"  <time>%s</time>" CRLF"  <channels>%ui</channels>" CRLF"  <wildcard_channels>%ui</wildcard_channels>" CRLF"  <uptime>%ui</uptime>" CRLF"  <infos>" CRLF);
static ngx_str_t  NGX_HTTP_PUSH_STREAM_CHANNEL_INFO_GROUP_TAIL_XML = ngx_string("  </infos>" CRLF"</root>" CRLF);
static ngx_str_t  NGX_HTTP_PUSH_STREAM_CHANNEL_INFO_GROUP_ITEM_XML = ngx_string(NGX_HTTP_PUSH_STREAM_CHANNEL_INFO_XML_PATTERN);
static ngx_str_t  NGX_HTTP_PUSH_STREAM_CHANNEL_INFO_GROUP_LAST_ITEM_XML = ngx_string(NGX_HTTP_PUSH_STREAM_CHANNEL_INFO_XML_PATTERN);
static ngx_str_t  NGX_HTTP_PUSH_STREAM_CHANNELS_INFO_SUMMARIZED_XML = ngx_string(
        "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>" CRLF \
        "<infos>" CRLF \
        "  <hostname>%s</hostname>" CRLF \
        "  <time>%s</time>" CRLF \
        "  <channels>%ui</channels>" CRLF \
        "  <wildcard_channels>%ui</wildcard_channels>" CRLF \
        "  <published_messages>%ui</published_messages>" CRLF \
        "  <stored_messages>%ui</stored_messages>" CRLF \
        "  <messages_in_trash>%ui</messages_in_trash>" CRLF \
        "  <channels_in_delete>%ui</channels_in_delete>" CRLF \
        "  <channels_in_trash>%ui</channels_in_trash>" CRLF \
        "  <subscribers>%ui</subscribers>" CRLF \
        "  <uptime>%ui</uptime>" CRLF \
        "  <by_worker>%s</by_worker>" CRLF \
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
#define NGX_HTTP_PUSH_STREAM_PING_MESSAGE_TEXT " "

static const ngx_int_t  NGX_HTTP_PUSH_STREAM_CHANNEL_DELETED_MESSAGE_ID = -2;
#define NGX_HTTP_PUSH_STREAM_CHANNEL_DELETED_MESSAGE_TEXT "Channel deleted"

static const ngx_int_t  NGX_HTTP_PUSH_STREAM_LONGPOOLING_TIMEOUT_MESSAGE_ID = -3;
#define NGX_HTTP_PUSH_STREAM_LONGPOOLING_TIMEOUT_MESSAGE_TEXT "Timed out"

static const ngx_str_t  NGX_HTTP_PUSH_STREAM_TOKEN_MESSAGE_ID = ngx_string("~id~");
static const ngx_str_t  NGX_HTTP_PUSH_STREAM_TOKEN_MESSAGE_EVENT_ID = ngx_string("~event-id~");
static const ngx_str_t  NGX_HTTP_PUSH_STREAM_TOKEN_MESSAGE_EVENT_TYPE = ngx_string("~event-type~");
static const ngx_str_t  NGX_HTTP_PUSH_STREAM_TOKEN_MESSAGE_CHANNEL = ngx_string("~channel~");
static const ngx_str_t  NGX_HTTP_PUSH_STREAM_TOKEN_MESSAGE_TEXT = ngx_string("~text~");
static const ngx_str_t  NGX_HTTP_PUSH_STREAM_TOKEN_MESSAGE_SIZE = ngx_string("~size~");
static const ngx_str_t  NGX_HTTP_PUSH_STREAM_TOKEN_MESSAGE_TAG = ngx_string("~tag~");
static const ngx_str_t  NGX_HTTP_PUSH_STREAM_TOKEN_MESSAGE_TIME = ngx_string("~time~");

static const ngx_str_t  NGX_HTTP_PUSH_STREAM_EVENTSOURCE_COMMENT_PREFIX = ngx_string(": ");
static const ngx_str_t  NGX_HTTP_PUSH_STREAM_EVENTSOURCE_DEFAULT_HEADER_TEMPLATE = ngx_string(": \n");
static const ngx_str_t  NGX_HTTP_PUSH_STREAM_EVENTSOURCE_COMMENT_TEMPLATE = ngx_string(": ~text~\n");
static const ngx_str_t  NGX_HTTP_PUSH_STREAM_EVENTSOURCE_MESSAGE_PREFIX = ngx_string("data: ");
static const ngx_str_t  NGX_HTTP_PUSH_STREAM_EVENTSOURCE_ID_TEMPLATE = ngx_string("id: ~event-id~\n");
static const ngx_str_t  NGX_HTTP_PUSH_STREAM_EVENTSOURCE_EVENT_TEMPLATE = ngx_string("event: ~event-type~\n");
static const ngx_str_t  NGX_HTTP_PUSH_STREAM_EVENTSOURCE_CONTENT_TYPE = ngx_string("text/event-stream; charset=utf-8");
static const ngx_str_t  NGX_HTTP_PUSH_STREAM_EVENTSOURCE_PING_MESSAGE_CHUNK = ngx_string(": -1\n");

static const ngx_str_t  NGX_HTTP_PUSH_STREAM_CALLBACK_INIT_CHUNK = ngx_string("([");
static const ngx_str_t  NGX_HTTP_PUSH_STREAM_CALLBACK_MID_CHUNK = ngx_string(",");
static const ngx_str_t  NGX_HTTP_PUSH_STREAM_CALLBACK_END_CHUNK = ngx_string("]);");
static const ngx_str_t  NGX_HTTP_PUSH_STREAM_CALLBACK_CONTENT_TYPE = ngx_string("application/javascript");

static const ngx_str_t  NGX_HTTP_PUSH_STREAM_PADDING_BY_USER_AGENT_PATTERN = ngx_string("([^:]+),(\\d+),(\\d+)");

#define NGX_HTTP_PUSH_STREAM_EVENT_TEMPLATE "{\"type\": \"%V\", \"channel\": \"%V\"}%Z"
static ngx_str_t  NGX_HTTP_PUSH_STREAM_EVENT_TYPE_CHANNEL_CREATED = ngx_string("channel_created");
static ngx_str_t  NGX_HTTP_PUSH_STREAM_EVENT_TYPE_CHANNEL_DESTROYED = ngx_string("channel_destroyed");
static ngx_str_t  NGX_HTTP_PUSH_STREAM_EVENT_TYPE_CLIENT_SUBSCRIBED = ngx_string("client_subscribed");
static ngx_str_t  NGX_HTTP_PUSH_STREAM_EVENT_TYPE_CLIENT_UNSUBSCRIBED = ngx_string("client_unsubscribed");


ngx_event_t         ngx_http_push_stream_memory_cleanup_event;
ngx_event_t         ngx_http_push_stream_buffer_cleanup_event;

// general request handling
ngx_http_push_stream_msg_t *ngx_http_push_stream_convert_char_to_msg_on_shared(ngx_http_push_stream_main_conf_t *mcf, u_char *data, size_t len, ngx_http_push_stream_channel_t *channel, ngx_int_t id, ngx_str_t *event_id, ngx_str_t *event_type, time_t time, ngx_int_t tag, ngx_pool_t *temp_pool);
static ngx_int_t            ngx_http_push_stream_send_only_added_headers(ngx_http_request_t *r);
static void                 ngx_http_push_stream_add_polling_headers(ngx_http_request_t *r, time_t last_modified_time, ngx_int_t tag, ngx_pool_t *temp_pool);
static void                 ngx_http_push_stream_get_last_received_message_values(ngx_http_request_t *r, time_t *if_modified_since, ngx_int_t *tag, ngx_str_t **last_event_id);
static ngx_table_elt_t *    ngx_http_push_stream_add_response_header(ngx_http_request_t *r, const ngx_str_t *header_name, const ngx_str_t *header_value);
static ngx_str_t *          ngx_http_push_stream_get_header(ngx_http_request_t *r, const ngx_str_t *header_name);
static ngx_int_t            ngx_http_push_stream_send_only_header_response(ngx_http_request_t *r, ngx_int_t status, const ngx_str_t *explain_error_message);
static ngx_int_t            ngx_http_push_stream_send_only_header_response_and_finalize(ngx_http_request_t *r, ngx_int_t status, const ngx_str_t *explain_error_message);
static ngx_str_t *          ngx_http_push_stream_str_replace(const ngx_str_t *org, const ngx_str_t *find, const ngx_str_t *replace, off_t offset, ngx_pool_t *temp_pool);
static ngx_str_t *          ngx_http_push_stream_get_formatted_websocket_frame(const u_char *opcode, off_t opcode_len, const u_char *text, off_t text_len, ngx_pool_t *temp_pool);
static ngx_str_t *          ngx_http_push_stream_get_formatted_message(ngx_http_request_t *r, ngx_http_push_stream_channel_t *channel, ngx_http_push_stream_msg_t *msg);
static ngx_str_t *          ngx_http_push_stream_format_message(ngx_http_push_stream_channel_t *channel, ngx_http_push_stream_msg_t *message, ngx_str_t *text, ngx_http_push_stream_template_t *template, ngx_pool_t *temp_pool);
static ngx_str_t *          ngx_http_push_stream_apply_template_to_each_line(ngx_str_t *text, const ngx_str_t *message_template, ngx_pool_t *temp_pool);
static ngx_int_t            ngx_http_push_stream_send_response_content_header(ngx_http_request_t *r, ngx_http_push_stream_loc_conf_t *pslcf);
static ngx_int_t            ngx_http_push_stream_send_response(ngx_http_request_t *r, ngx_str_t *text, const ngx_str_t *content_type, ngx_int_t status_code);
static ngx_int_t            ngx_http_push_stream_send_response_message(ngx_http_request_t *r, ngx_http_push_stream_channel_t *channel, ngx_http_push_stream_msg_t *msg, ngx_flag_t send_callback, ngx_flag_t send_separator);
static ngx_int_t            ngx_http_push_stream_send_response_text(ngx_http_request_t *r, const u_char *text, uint len, ngx_flag_t last_buffer);
static void                 ngx_http_push_stream_send_response_finalize(ngx_http_request_t *r);
static void                 ngx_http_push_stream_send_response_finalize_for_longpolling_by_timeout(ngx_http_request_t *r);
static ngx_int_t            ngx_http_push_stream_send_websocket_close_frame(ngx_http_request_t *r, ngx_uint_t http_status, const ngx_str_t *reason);
static ngx_int_t            ngx_http_push_stream_memory_cleanup(void);

ngx_chain_t *               ngx_http_push_stream_get_buf(ngx_http_request_t *r);
static void                 ngx_http_push_stream_unescape_uri(ngx_str_t *value);
static void                 ngx_http_push_stream_complex_value(ngx_http_request_t *r, ngx_http_complex_value_t *val, ngx_str_t *value);


ngx_int_t                   ngx_http_push_stream_add_msg_to_channel(ngx_http_push_stream_main_conf_t *mcf, ngx_log_t *log, ngx_http_push_stream_channel_t *channel, u_char *text, size_t len, ngx_str_t *event_id, ngx_str_t *event_type, ngx_flag_t store_messages, ngx_pool_t *temp_pool);
ngx_int_t                   ngx_http_push_stream_send_event(ngx_http_push_stream_main_conf_t *mcf, ngx_log_t *log, ngx_http_push_stream_channel_t *channel, ngx_str_t *event_id, ngx_pool_t *temp_pool);

static void                 ngx_http_push_stream_ping_timer_wake_handler(ngx_event_t *ev);
static void                 ngx_http_push_stream_disconnect_timer_wake_handler(ngx_event_t *ev);
static void                 ngx_http_push_stream_memory_cleanup_timer_wake_handler(ngx_event_t *ev);
static void                 ngx_http_push_stream_buffer_timer_wake_handler(ngx_event_t *ev);

static void                 ngx_http_push_stream_timer_set(ngx_msec_t timer_interval, ngx_event_t *event, ngx_event_handler_pt event_handler, ngx_flag_t start_timer);
static void                 ngx_http_push_stream_timer_reset(ngx_msec_t timer_interval, ngx_event_t *timer_event);

#define ngx_http_push_stream_memory_cleanup_timer_set(void) ngx_http_push_stream_timer_set(NGX_HTTP_PUSH_STREAM_DEFAULT_SHM_MEMORY_CLEANUP_INTERVAL, &ngx_http_push_stream_memory_cleanup_event, ngx_http_push_stream_memory_cleanup_timer_wake_handler, 1);
#define ngx_http_push_stream_buffer_cleanup_timer_set(void) ngx_http_push_stream_timer_set(NGX_HTTP_PUSH_STREAM_MESSAGE_BUFFER_CLEANUP_INTERVAL, &ngx_http_push_stream_buffer_cleanup_event, ngx_http_push_stream_buffer_timer_wake_handler, 1);

static void                 ngx_http_push_stream_worker_subscriber_cleanup(ngx_http_push_stream_subscriber_t *worker_subscriber);
static ngx_str_t *          ngx_http_push_stream_create_str(ngx_pool_t *pool, uint len);

static void                 ngx_http_push_stream_throw_the_message_away(ngx_http_push_stream_msg_t *msg, ngx_http_push_stream_shm_data_t *data);
static ngx_int_t            ngx_http_push_stream_delete_channel(ngx_http_push_stream_main_conf_t *mcf, ngx_http_push_stream_channel_t *channel, u_char *text, size_t len, ngx_pool_t *temp_pool);
static void                 ngx_http_push_stream_collect_expired_messages_data(ngx_http_push_stream_shm_data_t *data, ngx_flag_t force);
static void                 ngx_http_push_stream_collect_expired_messages_and_empty_channels(ngx_flag_t force);
static void                 ngx_http_push_stream_free_message_memory(ngx_slab_pool_t *shpool, ngx_http_push_stream_msg_t *msg);
static void                 ngx_http_push_stream_free_worker_message_memory(ngx_slab_pool_t *shpool, ngx_http_push_stream_worker_msg_t *worker_msg);
static ngx_int_t            ngx_http_push_stream_free_memory_of_expired_messages_and_channels(ngx_flag_t force);
ngx_uint_t                  ngx_http_push_stream_ensure_qtd_of_messages(ngx_http_push_stream_shm_data_t *data, ngx_http_push_stream_channel_t *channel, ngx_uint_t max_messages, ngx_flag_t expired);
static ngx_inline void      ngx_http_push_stream_delete_worker_channel(void);

static ngx_http_push_stream_content_subtype_t *     ngx_http_push_stream_match_channel_info_format_and_content_type(ngx_http_request_t *r, ngx_uint_t default_subtype);

static ngx_queue_t *                                ngx_http_push_stream_split_by_crlf(ngx_str_t *msg, ngx_pool_t *temp_pool);
static ngx_str_t *                                  ngx_http_push_stream_join_with_crlf(ngx_queue_t *lines, ngx_pool_t *temp_pool);

static ngx_http_push_stream_module_ctx_t *          ngx_http_push_stream_add_request_context(ngx_http_request_t *r);

static ngx_queue_t *        ngx_http_push_stream_parse_paddings(ngx_conf_t *cf, ngx_str_t *paddings_by_user_agent);

static ngx_str_t *          ngx_http_push_stream_get_formatted_current_time(ngx_pool_t *pool);
static ngx_str_t *          ngx_http_push_stream_get_formatted_hostname(ngx_pool_t *pool);

uint64_t                    ngx_http_push_stream_htonll(uint64_t value);
uint64_t                    ngx_http_push_stream_ntohll(uint64_t value);

static ngx_int_t            ngx_http_push_stream_set_expires(ngx_http_request_t *r, ngx_http_push_stream_expires_t expires, time_t expires_time);

ngx_http_push_stream_requested_channel_t *ngx_http_push_stream_parse_channels_ids_from_path(ngx_http_request_t *r, ngx_pool_t *pool);

ngx_int_t                   ngx_http_push_stream_create_shmtx(ngx_shmtx_t *mtx, ngx_shmtx_sh_t *addr, u_char *name);

ngx_flag_t                  ngx_http_push_stream_is_utf8(u_char *p, size_t n);

#endif /* NGX_HTTP_PUSH_STREAM_MODULE_UTILS_H_ */
