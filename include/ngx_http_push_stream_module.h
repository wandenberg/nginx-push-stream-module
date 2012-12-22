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
 * ngx_http_push_stream_module.h
 *
 * Created: Oct 26, 2010
 * Authors: Wandenberg Peixoto <wandenberg@gmail.com>, Rogério Carvalho Schneider <stockrt@gmail.com>
 */

#ifndef NGX_HTTP_PUSH_STREAM_MODULE_H_
#define NGX_HTTP_PUSH_STREAM_MODULE_H_

#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>
#include <nginx.h>

typedef struct {
    ngx_queue_t                     queue;
    void                           *value;
} ngx_http_push_stream_queue_elem_t;

typedef struct {
    ngx_queue_t                     queue;
    ngx_regex_t                    *agent;
    ngx_uint_t                      header_min_len;
    ngx_uint_t                      message_min_len;
} ngx_http_push_stream_padding_t;

// template queue
typedef struct {
    ngx_queue_t                     queue; // this MUST be first
    ngx_str_t                      *template;
    ngx_uint_t                      index;
    ngx_flag_t                      eventsource;
    ngx_flag_t                      websocket;
} ngx_http_push_stream_template_queue_t;

typedef struct {
    ngx_flag_t                      enabled;
    size_t                          shm_size;
    ngx_msec_t                      memory_cleanup_interval;
    time_t                          shm_cleanup_objects_ttl;
    ngx_str_t                       channel_deleted_message_text;
    ngx_str_t                       ping_message_text;
    ngx_uint_t                      qtd_templates;
    ngx_str_t                       broadcast_channel_prefix;
    ngx_uint_t                      max_number_of_channels;
    ngx_uint_t                      max_number_of_broadcast_channels;
    time_t                          message_ttl;
    ngx_uint_t                      max_subscribers_per_channel;
    ngx_uint_t                      max_messages_stored_per_channel;
    ngx_uint_t                      max_channel_id_length;
    ngx_http_push_stream_template_queue_t  msg_templates;
    ngx_regex_t                    *backtrack_parser_regex;
} ngx_http_push_stream_main_conf_t;

typedef struct {
    ngx_int_t                       index_channel_id;
    ngx_int_t                       index_channels_path;
    ngx_uint_t                      authorized_channels_only;
    ngx_flag_t                      store_messages;
    ngx_str_t                       header_template;
    ngx_str_t                       message_template;
    ngx_int_t                       message_template_index;
    ngx_str_t                       footer_template;
    ngx_str_t                       content_type;
    ngx_uint_t                      broadcast_channel_max_qtd;
    ngx_uint_t                      keepalive;
    ngx_flag_t                      eventsource_support;
    ngx_uint_t                      location_type;
    ngx_msec_t                      ping_message_interval;
    ngx_msec_t                      subscriber_connection_ttl;
    ngx_msec_t                      longpolling_connection_ttl;
    ngx_flag_t                      websocket_allow_publish;
    ngx_http_complex_value_t       *last_received_message_time;
    ngx_http_complex_value_t       *last_received_message_tag;
    ngx_http_complex_value_t       *user_agent;
    ngx_str_t                       padding_by_user_agent;
    ngx_http_push_stream_padding_t *paddings;
    ngx_str_t                       allowed_origins;
} ngx_http_push_stream_loc_conf_t;

// shared memory segment name
static ngx_str_t    ngx_http_push_stream_shm_name = ngx_string("push_stream_module");

// message queue
typedef struct {
    ngx_queue_t                     queue; // this MUST be first
    time_t                          expires;
    time_t                          time;
    ngx_flag_t                      deleted;
    ngx_int_t                       id;
    ngx_str_t                       raw;
    ngx_int_t                       tag;
    ngx_str_t                      *event_id;
    ngx_str_t                      *event_type;
    ngx_str_t                      *event_id_message;
    ngx_str_t                      *event_type_message;
    ngx_str_t                      *formatted_messages;
    ngx_int_t                       workers_ref_count;
} ngx_http_push_stream_msg_t;

typedef struct ngx_http_push_stream_subscriber_s ngx_http_push_stream_subscriber_t;

typedef struct {
    ngx_queue_t                         queue;
    pid_t                               pid;
    ngx_int_t                           slot;
    ngx_http_push_stream_queue_elem_t   subscribers_sentinel;
} ngx_http_push_stream_pid_queue_t;

// our typecast-friendly rbtree node (channel)
typedef struct {
    ngx_rbtree_node_t                   node; // this MUST be first
    ngx_queue_t                         queue;
    ngx_queue_t                        *queue_sentinel;
    ngx_str_t                           id;
    ngx_uint_t                          last_message_id;
    time_t                              last_message_time;
    ngx_int_t                           last_message_tag;
    ngx_uint_t                          stored_messages;
    ngx_uint_t                          subscribers;
    ngx_http_push_stream_pid_queue_t    workers_with_subscribers;
    ngx_http_push_stream_msg_t          message_queue;
    time_t                              last_activity_time;
    time_t                              expires;
    ngx_flag_t                          deleted;
    ngx_flag_t                          broadcast;
    ngx_http_push_stream_msg_t         *channel_deleted_message;
} ngx_http_push_stream_channel_t;

typedef struct {
    ngx_queue_t                         queue;
    ngx_str_t                           id;
    ngx_uint_t                          published_messages;
    ngx_uint_t                          stored_messages;
    ngx_uint_t                          subscribers;
} ngx_http_push_stream_channel_info_t;


typedef struct {
    ngx_queue_t                         queue;
    ngx_http_push_stream_subscriber_t  *subscriber;
    ngx_http_push_stream_channel_t     *channel;
    ngx_http_push_stream_queue_elem_t  *channel_subscriber_element_ref;
} ngx_http_push_stream_subscription_t;

struct ngx_http_push_stream_subscriber_s {
    ngx_http_request_t                         *request;
    ngx_http_push_stream_subscription_t         subscriptions_sentinel;
    ngx_pid_t                                   worker_subscribed_pid;
    ngx_flag_t                                  longpolling;
    ngx_http_push_stream_queue_elem_t          *worker_subscriber_element_ref;
};

typedef struct {
    ngx_event_t                        *disconnect_timer;
    ngx_event_t                        *ping_timer;
    ngx_http_push_stream_subscriber_t  *subscriber;
    ngx_flag_t                          longpolling;
    ngx_pool_t                         *temp_pool;
    ngx_chain_t                        *free;
    ngx_chain_t                        *busy;
    ngx_http_push_stream_padding_t     *padding;
    ngx_str_t                          *callback;
} ngx_http_push_stream_subscriber_ctx_t;

// messages to worker processes
typedef struct {
    ngx_queue_t                         queue;
    ngx_http_push_stream_msg_t         *msg; // ->shared memory
    ngx_pid_t                           pid;
    ngx_http_push_stream_channel_t     *channel; // ->shared memory
    ngx_http_push_stream_queue_elem_t  *subscribers_sentinel; // ->a worker's local pool
} ngx_http_push_stream_worker_msg_t;

typedef struct {
    ngx_http_push_stream_worker_msg_t           *messages_queue;
    ngx_http_push_stream_queue_elem_t           *subscribers_sentinel;
    ngx_uint_t                                   subscribers; // # of subscribers in the worker
    time_t                                       startup;
    pid_t                                        pid;
} ngx_http_push_stream_worker_data_t;

// shared memory
typedef struct {
    ngx_rbtree_t                            tree;
    ngx_uint_t                              channels;           // # of channels being used
    ngx_uint_t                              broadcast_channels; // # of broadcast channels being used
    ngx_uint_t                              published_messages; // # of published messagens in all channels
    ngx_uint_t                              subscribers;        // # of subscribers in all channels
    ngx_http_push_stream_msg_t              messages_to_delete;
    ngx_queue_t                             channels_queue;
    ngx_queue_t                             channels_trash;
    ngx_queue_t                             channels_to_delete;
    ngx_http_push_stream_worker_data_t      ipc[NGX_MAX_PROCESSES]; // interprocess stuff
    time_t                                  startup;
    time_t                                  last_message_time;
    ngx_int_t                               last_message_tag;
} ngx_http_push_stream_shm_data_t;

ngx_uint_t          ngx_http_push_stream_shm_size;
ngx_shm_zone_t     *ngx_http_push_stream_shm_zone = NULL;

ngx_http_push_stream_main_conf_t *ngx_http_push_stream_module_main_conf = NULL;

ngx_str_t         **ngx_http_push_stream_module_paddings_chunks = NULL;

// channel
static ngx_str_t *      ngx_http_push_stream_get_channel_id(ngx_http_request_t *r, ngx_http_push_stream_loc_conf_t *cf);
static ngx_int_t        ngx_http_push_stream_send_response_channel_info(ngx_http_request_t *r, ngx_http_push_stream_channel_t *channel);
static ngx_int_t        ngx_http_push_stream_send_response_all_channels_info_summarized(ngx_http_request_t *r);
static ngx_int_t        ngx_http_push_stream_send_response_all_channels_info_detailed(ngx_http_request_t *r, ngx_str_t *prefix);

static ngx_int_t        ngx_http_push_stream_find_or_add_template(ngx_conf_t *cf, ngx_str_t template, ngx_flag_t eventsource, ngx_flag_t websocket);

static const ngx_str_t  NGX_HTTP_PUSH_STREAM_ALL_CHANNELS_INFO_ID = ngx_string("ALL");

static const ngx_str_t NGX_HTTP_PUSH_STREAM_NO_CHANNEL_ID_MESSAGE  = ngx_string("No channel id provided.");
static const ngx_str_t NGX_HTTP_PUSH_STREAM_NO_CHANNEL_ID_NOT_AUTHORIZED_MESSAGE = ngx_string("Channel id not authorized for this method.");
static const ngx_str_t NGX_HTTP_PUSH_STREAM_EMPTY_POST_REQUEST_MESSAGE = ngx_string("Empty post requests are not allowed.");
static const ngx_str_t NGX_HTTP_PUSH_STREAM_TOO_LARGE_CHANNEL_ID_MESSAGE = ngx_string("Channel id is too large.");
static const ngx_str_t NGX_HTTP_PUSH_STREAM_TOO_MUCH_BROADCAST_CHANNELS = ngx_string("Subscribed too much broadcast channels.");
static const ngx_str_t NGX_HTTP_PUSH_STREAM_TOO_SUBSCRIBERS_PER_CHANNEL = ngx_string("Subscribers limit per channel has been exceeded.");
static const ngx_str_t NGX_HTTP_PUSH_STREAM_CANNOT_CREATE_CHANNELS = ngx_string("Subscriber could not create channels.");
static const ngx_str_t NGX_HTTP_PUSH_STREAM_NUMBER_OF_CHANNELS_EXCEEDED_MESSAGE = ngx_string("Number of channels were exceeded.");
static const ngx_str_t NGX_HTTP_PUSH_STREAM_NO_MANDATORY_HEADERS_MESSAGE = ngx_string("Don't have at least one of the mandatory headers: Connection, Upgrade, Sec-WebSocket-Key and Sec-WebSocket-Version");
static const ngx_str_t NGX_HTTP_PUSH_STREAM_WRONG_WEBSOCKET_VERSION_MESSAGE = ngx_string("Version not supported. Supported versions: 8, 13");
static const ngx_str_t NGX_HTTP_PUSH_STREAM_CHANNEL_DELETED = ngx_string("Channel deleted.");

#define NGX_HTTP_PUSH_STREAM_UNSET_CHANNEL_ID               (void *) -1
#define NGX_HTTP_PUSH_STREAM_TOO_LARGE_CHANNEL_ID           (void *) -2
#define NGX_HTTP_PUSH_STREAM_NUMBER_OF_CHANNELS_EXCEEDED    (void *) -3

static ngx_str_t        NGX_HTTP_PUSH_STREAM_EMPTY = ngx_string("");
static const ngx_str_t  NGX_HTTP_PUSH_STREAM_BACKTRACK_PATTERN = ngx_string("((\\.b([0-9]+))?(/|$))");
static const ngx_str_t  NGX_HTTP_PUSH_STREAM_CALLBACK = ngx_string("callback");

static const ngx_str_t  NGX_HTTP_PUSH_STREAM_DATE_FORMAT_ISO_8601 = ngx_string("%4d-%02d-%02dT%02d:%02d:%02d");

// headers
static const ngx_str_t  NGX_HTTP_PUSH_STREAM_HEADER_EVENT_ID = ngx_string("Event-Id");
static const ngx_str_t  NGX_HTTP_PUSH_STREAM_HEADER_EVENT_TYPE = ngx_string("Event-Type");
static const ngx_str_t  NGX_HTTP_PUSH_STREAM_HEADER_LAST_EVENT_ID = ngx_string("Last-Event-Id");
static const ngx_str_t  NGX_HTTP_PUSH_STREAM_HEADER_ALLOW = ngx_string("Allow");
static const ngx_str_t  NGX_HTTP_PUSH_STREAM_HEADER_EXPLAIN = ngx_string("X-Nginx-PushStream-Explain");
static const ngx_str_t  NGX_HTTP_PUSH_STREAM_HEADER_MODE = ngx_string("X-Nginx-PushStream-Mode");
static const ngx_str_t  NGX_HTTP_PUSH_STREAM_HEADER_TAG = ngx_string("X-Nginx-PushStream-Tag");
static const ngx_str_t  NGX_HTTP_PUSH_STREAM_HEADER_COMMIT = ngx_string("X-Nginx-PushStream-Commit");
static const ngx_str_t  NGX_HTTP_PUSH_STREAM_HEADER_TRANSFER_ENCODING = ngx_string("Transfer-Encoding");
static const ngx_str_t  NGX_HTTP_PUSH_STREAM_HEADER_CHUNCKED = ngx_string("chunked");
static const ngx_str_t  NGX_HTTP_PUSH_STREAM_HEADER_ETAG = ngx_string("Etag");
static const ngx_str_t  NGX_HTTP_PUSH_STREAM_HEADER_IF_NONE_MATCH = ngx_string("If-None-Match");
static const ngx_str_t  NGX_HTTP_PUSH_STREAM_HEADER_VARY = ngx_string("Vary");
static const ngx_str_t  NGX_HTTP_PUSH_STREAM_HEADER_UPGRADE = ngx_string("Upgrade");
static const ngx_str_t  NGX_HTTP_PUSH_STREAM_HEADER_CONNECTION = ngx_string("Connection");
static const ngx_str_t  NGX_HTTP_PUSH_STREAM_HEADER_SEC_WEBSOCKET_KEY = ngx_string("Sec-WebSocket-Key");
static const ngx_str_t  NGX_HTTP_PUSH_STREAM_HEADER_SEC_WEBSOCKET_VERSION = ngx_string("Sec-WebSocket-Version");
static const ngx_str_t  NGX_HTTP_PUSH_STREAM_HEADER_SEC_WEBSOCKET_ACCEPT = ngx_string("Sec-WebSocket-Accept");
static const ngx_str_t  NGX_HTTP_PUSH_STREAM_HEADER_ACCESS_CONTROL_ALLOW_ORIGIN = ngx_string("Access-Control-Allow-Origin");
static const ngx_str_t  NGX_HTTP_PUSH_STREAM_HEADER_ACCESS_CONTROL_ALLOW_METHODS = ngx_string("Access-Control-Allow-Methods");
static const ngx_str_t  NGX_HTTP_PUSH_STREAM_HEADER_ACCESS_CONTROL_ALLOW_HEADERS = ngx_string("Access-Control-Allow-Headers");

static const ngx_str_t  NGX_HTTP_PUSH_STREAM_WEBSOCKET_UPGRADE = ngx_string("WebSocket");
static const ngx_str_t  NGX_HTTP_PUSH_STREAM_WEBSOCKET_CONNECTION = ngx_string("Upgrade");
static const ngx_str_t  NGX_HTTP_PUSH_STREAM_WEBSOCKET_SIGN_KEY = ngx_string("258EAFA5-E914-47DA-95CA-C5AB0DC85B11");
static const ngx_str_t  NGX_HTTP_PUSH_STREAM_WEBSOCKET_SUPPORTED_VERSIONS = ngx_string("8, 13");

static const ngx_str_t  NGX_HTTP_PUSH_STREAM_101_STATUS_LINE = ngx_string("101 Switching Protocols");


static const ngx_str_t  NGX_HTTP_PUSH_STREAM_MODE_NORMAL   = ngx_string("normal");
static const ngx_str_t  NGX_HTTP_PUSH_STREAM_MODE_ADMIN    = ngx_string("admin");

static const ngx_str_t  NGX_HTTP_PUSH_STREAM_MODE_STREAMING   = ngx_string("streaming");
static const ngx_str_t  NGX_HTTP_PUSH_STREAM_MODE_POLLING     = ngx_string("polling");
static const ngx_str_t  NGX_HTTP_PUSH_STREAM_MODE_LONGPOLLING = ngx_string("long-polling");

#define NGX_HTTP_PUSH_STREAM_SUBSCRIBER_MODE_STREAMING   0
#define NGX_HTTP_PUSH_STREAM_SUBSCRIBER_MODE_POLLING     1
#define NGX_HTTP_PUSH_STREAM_SUBSCRIBER_MODE_LONGPOLLING 2
#define NGX_HTTP_PUSH_STREAM_PUBLISHER_MODE_NORMAL       3
#define NGX_HTTP_PUSH_STREAM_PUBLISHER_MODE_ADMIN        4
#define NGX_HTTP_PUSH_STREAM_STATISTICS_MODE             5
#define NGX_HTTP_PUSH_STREAM_WEBSOCKET_MODE              6


#define NGX_HTTP_PUSH_STREAM_WEBSOCKET_VERSION_8         8
#define NGX_HTTP_PUSH_STREAM_WEBSOCKET_VERSION_13        13

#define NGX_HTTP_PUSH_STREAM_WEBSOCKET_SHA1_SIGNED_HASH_LENGTH 20
#define NGX_HTTP_PUSH_STREAM_WEBSOCKET_FRAME_HEADER_MAX_LENGTH 144

#define NGX_HTTP_PUSH_STREAM_WEBSOCKET_LAST_FRAME   0x8

#define NGX_HTTP_PUSH_STREAM_WEBSOCKET_TEXT_OPCODE  0x1
#define NGX_HTTP_PUSH_STREAM_WEBSOCKET_CLOSE_OPCODE 0x8
#define NGX_HTTP_PUSH_STREAM_WEBSOCKET_PING_OPCODE  0x9
#define NGX_HTTP_PUSH_STREAM_WEBSOCKET_P0NG_OPCODE  0xA

static const u_char NGX_HTTP_PUSH_STREAM_WEBSOCKET_TEXT_LAST_FRAME_BYTE    =  NGX_HTTP_PUSH_STREAM_WEBSOCKET_TEXT_OPCODE  | (NGX_HTTP_PUSH_STREAM_WEBSOCKET_LAST_FRAME << 4);
static const u_char NGX_HTTP_PUSH_STREAM_WEBSOCKET_CLOSE_LAST_FRAME_BYTE[] = {NGX_HTTP_PUSH_STREAM_WEBSOCKET_CLOSE_OPCODE | (NGX_HTTP_PUSH_STREAM_WEBSOCKET_LAST_FRAME << 4), 0x00};
static const u_char NGX_HTTP_PUSH_STREAM_WEBSOCKET_PING_LAST_FRAME_BYTE[]  = {NGX_HTTP_PUSH_STREAM_WEBSOCKET_PING_OPCODE  | (NGX_HTTP_PUSH_STREAM_WEBSOCKET_LAST_FRAME << 4), 0x00};
static const u_char NGX_HTTP_PUSH_STREAM_WEBSOCKET_PAYLOAD_LEN_16_BYTE   = 126;
static const u_char NGX_HTTP_PUSH_STREAM_WEBSOCKET_PAYLOAD_LEN_64_BYTE   = 127;


// other stuff
static const ngx_str_t  NGX_HTTP_PUSH_STREAM_ALLOW_GET_POST_DELETE_METHODS = ngx_string("GET, POST, DELETE");
static const ngx_str_t  NGX_HTTP_PUSH_STREAM_ALLOW_GET_POST_METHODS = ngx_string("GET, POST");
static const ngx_str_t  NGX_HTTP_PUSH_STREAM_ALLOW_GET = ngx_string("GET");

static const ngx_str_t  NGX_HTTP_PUSH_STREAM_ALLOWED_HEADERS = ngx_string("If-Modified-Since,If-None-Match");

#define NGX_HTTP_PUSH_STREAM_CHECK_AND_FINALIZE_REQUEST_ON_ERROR(val, fail, r, errormessage) \
    if (val == fail) {                                                       \
        ngx_log_error(NGX_LOG_ERR, (r)->connection->log, 0, errormessage);   \
        ngx_http_finalize_request(r, NGX_HTTP_INTERNAL_SERVER_ERROR);        \
        return;                                                              \
    }

#define NGX_HTTP_PUSH_STREAM_CHECK_AND_FINALIZE_REQUEST_ON_ERROR_LOCKED(val, fail, r, errormessage) \
    if (val == fail) {                                                       \
        ngx_shmtx_unlock(&(shpool)->mutex);                                  \
        ngx_log_error(NGX_LOG_ERR, (r)->connection->log, 0, errormessage);   \
        ngx_http_finalize_request(r, NGX_HTTP_INTERNAL_SERVER_ERROR);        \
        return;                                                              \
    }

#define NGX_HTTP_PUSH_STREAM_DECREMENT_COUNTER(counter) \
    (counter = (counter > 1) ? counter - 1 : 0)

#define NGX_HTTP_PUSH_STREAM_TIME_FMT_LEN   30 //sizeof("Mon, 28 Sep 1970 06:00:00 GMT")

#endif /* NGX_HTTP_PUSH_STREAM_MODULE_H_ */
