/*
 * ngx_http_push_stream_module.h
 *
 *  Created on: Oct 26, 2010
 *      Authors: Wandenberg Peixoto <wandenberg@gmail.com> & Rogério Schneider <stockrt@gmail.com>
 */

#ifndef NGX_HTTP_PUSH_STREAM_MODULE_H_
#define NGX_HTTP_PUSH_STREAM_MODULE_H_

#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>
#include <nginx.h>


typedef struct {
    size_t                          shm_size;
} ngx_http_push_stream_main_conf_t;

typedef struct {
    ngx_int_t                       index_channel_id;
    ngx_int_t                       index_channels_path;
    time_t                          buffer_timeout;
    ngx_uint_t                      max_messages;
    ngx_uint_t                      authorized_channels_only;
    ngx_uint_t                      store_messages;
    ngx_uint_t                      max_channel_id_length;
    ngx_str_t                       header_template;
    ngx_str_t                       message_template;
    ngx_str_t                       content_type;
    ngx_msec_t                      ping_message_interval;
    ngx_msec_t                      subscriber_disconnect_interval;
    time_t                          subscriber_connection_timeout;
    ngx_str_t                       broadcast_channel_prefix;
    ngx_uint_t                      broadcast_channel_max_qtd;
    ngx_uint_t                      max_number_of_channels;
    ngx_uint_t                      max_number_of_broadcast_channels;
    ngx_msec_t                      memory_cleanup_interval;
    time_t                          memory_cleanup_timeout;
} ngx_http_push_stream_loc_conf_t;

// shared memory segment name
static ngx_str_t    ngx_push_stream_shm_name = ngx_string("push_stream_module");

// message queue
typedef struct {
    ngx_queue_t                     queue; // this MUST be first
    ngx_buf_t                      *buf;
    time_t                          expires;
    ngx_flag_t                      deleted;
} ngx_http_push_stream_msg_t;

typedef struct ngx_http_push_stream_subscriber_cleanup_s ngx_http_push_stream_subscriber_cleanup_t;

// subscriber request queue
typedef struct {
    ngx_queue_t                                 queue; // this MUST be first
    ngx_http_request_t                         *request;
} ngx_http_push_stream_subscriber_t;

typedef struct {
    ngx_queue_t                         queue;
    pid_t                               pid;
    ngx_int_t                           slot;
    ngx_http_push_stream_subscriber_t   subscriber_sentinel;
} ngx_http_push_stream_pid_queue_t;

// our typecast-friendly rbtree node (channel)
typedef struct {
    ngx_rbtree_node_t                   node; // this MUST be first
    ngx_str_t                           id;
    ngx_uint_t                          last_message_id;
    ngx_uint_t                          stored_messages;
    ngx_uint_t                          subscribers;
    ngx_http_push_stream_pid_queue_t    workers_with_subscribers;
    ngx_http_push_stream_msg_t          message_queue;
    time_t                              expires;
    ngx_flag_t                          deleted;
    ngx_flag_t                          broadcast;
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
} ngx_http_push_stream_subscription_t;

typedef struct {
    ngx_queue_t                                 queue; // this MUST be first
    ngx_http_request_t                         *request;
    ngx_http_push_stream_subscription_t         subscriptions_sentinel;
    ngx_http_push_stream_subscriber_cleanup_t  *clndata;
    ngx_pid_t                                   worker_subscribed_pid;
    time_t                                      expires;
} ngx_http_push_stream_worker_subscriber_t;

// cleaning supplies
struct ngx_http_push_stream_subscriber_cleanup_s {
    ngx_http_push_stream_worker_subscriber_t    *worker_subscriber;
};

// messages to worker processes
typedef struct {
    ngx_queue_t                         queue;
    ngx_http_push_stream_msg_t         *msg; // ->shared memory
    ngx_pid_t                           pid;
    ngx_http_push_stream_channel_t     *channel; // ->shared memory
    ngx_http_push_stream_subscriber_t  *subscriber_sentinel; // ->a worker's local pool
} ngx_http_push_stream_worker_msg_t;

typedef struct {
    ngx_http_push_stream_worker_msg_t            messages_queue;
    ngx_http_push_stream_worker_subscriber_t     worker_subscribers_sentinel;
    ngx_uint_t                                   subscribers; // # of subscribers in the worker
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
    ngx_rbtree_t                            channels_to_delete;
    ngx_http_push_stream_worker_data_t     *ipc; // interprocess stuff
} ngx_http_push_stream_shm_data_t;

ngx_int_t           ngx_http_push_stream_worker_processes;
ngx_shm_zone_t     *ngx_http_push_stream_shm_zone = NULL;

// channel
static ngx_str_t *      ngx_http_push_stream_get_channel_id(ngx_http_request_t *r, ngx_http_push_stream_loc_conf_t *cf);
static ngx_int_t        ngx_http_push_stream_send_response_channel_info(ngx_http_request_t *r, ngx_http_push_stream_channel_t *channel);
static ngx_int_t        ngx_http_push_stream_send_response_all_channels_info_summarized(ngx_http_request_t *r);
static ngx_int_t        ngx_http_push_stream_send_response_all_channels_info_detailed(ngx_http_request_t *r);

static const ngx_str_t  NGX_HTTP_PUSH_STREAM_ALL_CHANNELS_INFO_ID = ngx_string("ALL");

static const ngx_str_t NGX_HTTP_PUSH_STREAM_NO_CHANNEL_ID_MESSAGE  = ngx_string("No channel id provided.");
static const ngx_str_t NGX_HTTP_PUSH_STREAM_NO_CHANNEL_ID_NOT_AUTHORIZED_MESSAGE = ngx_string("Channel id not authorized for this method.");
static const ngx_str_t NGX_HTTP_PUSH_STREAM_EMPTY_POST_REQUEST_MESSAGE = ngx_string("Empty post requests are not allowed.");
static const ngx_str_t NGX_HTTP_PUSH_STREAM_TOO_LARGE_CHANNEL_ID_MESSAGE = ngx_string("Channel id is too large.");
static const ngx_str_t NGX_HTTP_PUSH_STREAM_TOO_MUCH_BROADCAST_CHANNELS = ngx_string("Subscribed too much broadcast channels.");
static const ngx_str_t NGX_HTTP_PUSH_STREAM_CANNOT_CREATE_CHANNELS = ngx_string("Subscriber could not create channels.");
static const ngx_str_t NGX_HTTP_PUSH_STREAM_NUMBER_OF_CHANNELS_EXCEEDED_MESSAGE = ngx_string("Number of channels were exceeded.");

#define NGX_HTTP_PUSH_STREAM_UNSET_CHANNEL_ID               (void *) -1
#define NGX_HTTP_PUSH_STREAM_TOO_LARGE_CHANNEL_ID           (void *) -2
#define NGX_HTTP_PUSH_STREAM_NUMBER_OF_CHANNELS_EXCEEDED    (void *) -3

static ngx_str_t        NGX_HTTP_PUSH_STREAM_EMPTY = ngx_string("");
static const ngx_str_t  NGX_HTTP_PUSH_STREAM_BACKTRACK_SEP = ngx_string(".b");
static const ngx_str_t  NGX_HTTP_PUSH_STREAM_SLASH = ngx_string("/");

static const ngx_str_t  NGX_PUSH_STREAM_DATE_FORMAT_ISO_8601 = ngx_string("%4d-%02d-%02dT%02d:%02d:%02d");

//// headers
static const ngx_str_t  NGX_HTTP_PUSH_STREAM_HEADER_ALLOW = ngx_string("Allow");
static const ngx_str_t  NGX_HTTP_PUSH_STREAM_HEADER_EXPLAIN = ngx_string("X-Nginx-PushStream-Explain");

// other stuff
static const ngx_str_t  NGX_HTTP_PUSH_STREAM_ALLOWED_METHODS = ngx_string("GET, POST");
static const ngx_str_t  NGX_HTTP_PUSH_STREAM_ALLOW_GET = ngx_string("GET");

#define NGX_HTTP_PUSH_STREAM_CHECK_AND_FINALIZE_REQUEST_ON_ERROR(val, fail, r, errormessage) \
    if (val == fail) {                                                       \
        ngx_log_error(NGX_LOG_ERR, (r)->connection->log, 0, errormessage);   \
        ngx_http_finalize_request(r, NGX_HTTP_INTERNAL_SERVER_ERROR);        \
        return;                                                              \
    }

#endif /* NGX_HTTP_PUSH_STREAM_MODULE_H_ */
