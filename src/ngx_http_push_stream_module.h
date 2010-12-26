#ifndef NGX_HTTP_PUSH_STREAM_MODULE_H_
#define NGX_HTTP_PUSH_STREAM_MODULE_H_


#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>
#include <ngx_channel.h>
#include <nginx.h>


typedef struct {
    size_t                          shm_size;
} ngx_http_push_stream_main_conf_t;

typedef struct {
    ngx_int_t                       index_channel_id;
    ngx_int_t                       index_channels_path;
    time_t                          buffer_timeout;
    ngx_int_t                       max_messages;
    ngx_int_t                       authorize_channel;
    ngx_int_t                       store_messages;
    ngx_int_t                       max_channel_id_length;
    ngx_str_t                       header_template;
    ngx_str_t                       message_template;
    ngx_str_t                       content_type;
    ngx_msec_t                      ping_message_interval;
    ngx_msec_t                      subscriber_disconnect_interval;
    time_t                          subscriber_connection_timeout;
    ngx_str_t                       broadcast_channel_prefix;
    ngx_int_t                       broadcast_channel_max_qtd;
} ngx_http_push_stream_loc_conf_t;

// variables
static ngx_str_t    ngx_http_push_stream_channel_id = ngx_string("push_stream_channel_id");
static ngx_str_t    ngx_http_push_stream_channels_path = ngx_string("push_stream_channels_path");

// shared memory segment name
static ngx_str_t    ngx_push_stream_shm_name = ngx_string("push_stream_module");

// message queue
typedef struct {
    ngx_queue_t                     queue; // this MUST be first
    ngx_buf_t                      *buf;
    time_t                          expires;
    ngx_int_t                       refcount;
    ngx_flag_t                      persistent;
} ngx_http_push_stream_msg_t;

typedef struct ngx_http_push_stream_subscriber_cleanup_s ngx_http_push_stream_subscriber_cleanup_t;

// subscriber request queue
typedef struct {
    ngx_queue_t                                 queue; // this MUST be first
    ngx_http_request_t                         *request;
    ngx_http_push_stream_subscriber_cleanup_t  *clndata;
} ngx_http_push_stream_subscriber_t;

typedef struct {
    ngx_queue_t                         queue;
    pid_t                               pid;
    ngx_int_t                           slot;
    ngx_http_push_stream_subscriber_t  *subscriber_sentinel;
} ngx_http_push_stream_pid_queue_t;

// our typecast-friendly rbtree node (channel)
typedef struct {
    ngx_rbtree_node_t                   node; // this MUST be first
    ngx_str_t                           id;
    ngx_http_push_stream_msg_t         *message_queue;
    ngx_uint_t                          last_message_id;
    ngx_uint_t                          stored_messages;
    ngx_http_push_stream_pid_queue_t    workers_with_subscribers;
    ngx_uint_t                          subscribers;
} ngx_http_push_stream_channel_t;

typedef struct {
    ngx_queue_t                         queue;
    ngx_http_push_stream_subscriber_t  *subscriber;
    ngx_http_push_stream_channel_t     *channel;
} ngx_http_push_stream_subscription_t;

typedef struct {
    ngx_queue_t                                 queue; // this MUST be first
    ngx_http_request_t                         *request;
    ngx_http_push_stream_subscription_t        *subscriptions_sentinel;
    ngx_http_push_stream_subscriber_cleanup_t  *clndata;
    ngx_pid_t                                   worker_subscribed_pid;
    time_t                                      expires;
} ngx_http_push_stream_worker_subscriber_t;

// cleaning supplies
struct ngx_http_push_stream_subscriber_cleanup_s {
    ngx_http_push_stream_worker_subscriber_t    *worker_subscriber;
};

typedef struct {
    ngx_queue_t                     queue;
    ngx_http_push_stream_msg_t     *msg;
} ngx_http_push_stream_msg_queue_t;

// garbage collecting goodness
typedef struct {
    ngx_queue_t                         queue;
    ngx_http_push_stream_channel_t     *channel;
} ngx_http_push_stream_channel_queue_t;

// messages to worker processes
typedef struct {
    ngx_queue_t                         queue;
    ngx_http_push_stream_msg_t         *msg; // ->shared memory
    ngx_int_t                           status_code;
    ngx_pid_t                           pid;
    ngx_http_push_stream_channel_t     *channel; // ->shared memory
    ngx_http_push_stream_subscriber_t  *subscriber_sentinel; // ->a worker's local pool
} ngx_http_push_stream_worker_msg_t;

typedef struct {
    ngx_queue_t                                 messages_queue;
    ngx_http_push_stream_worker_subscriber_t   *worker_subscribers_sentinel;
} ngx_http_push_stream_worker_data_t;

// shared memory
typedef struct {
    ngx_rbtree_t                            tree;
    ngx_uint_t                              channels; // # of channels being used
    ngx_http_push_stream_worker_data_t     *ipc; // interprocess stuff
} ngx_http_push_stream_shm_data_t;

typedef struct {
    char                       *subtype;
    size_t                      len;
    const ngx_str_t            *format;
} ngx_http_push_stream_content_subtype_t;

ngx_event_t         ngx_http_push_stream_ping_event;
ngx_event_t         ngx_http_push_stream_disconnect_event;
ngx_int_t           ngx_http_push_stream_worker_processes;
ngx_pool_t         *ngx_http_push_stream_pool;
ngx_slab_pool_t    *ngx_http_push_stream_shpool;
ngx_shm_zone_t     *ngx_http_push_stream_shm_zone = NULL;
ngx_chain_t        *ngx_http_push_stream_header_chain = NULL;
ngx_chain_t        *ngx_http_push_stream_crlf_chain = NULL;

ngx_http_push_stream_msg_t  *ngx_http_push_stream_ping_msg = NULL;
ngx_buf_t                   *ngx_http_push_stream_ping_buf = NULL;

// emergency garbage collecting goodness;
ngx_http_push_stream_channel_queue_t channel_gc_sentinel;

// garbage-collecting shared memory slab allocation
void *              ngx_http_push_stream_slab_alloc_locked(size_t size);
static ngx_int_t    ngx_http_push_stream_channel_collector(ngx_http_push_stream_channel_t *channel, ngx_slab_pool_t *shpool);

// setup
static ngx_int_t    ngx_http_push_stream_init_module(ngx_cycle_t *cycle);
static ngx_int_t    ngx_http_push_stream_init_worker(ngx_cycle_t *cycle);
static void         ngx_http_push_stream_exit_worker(ngx_cycle_t *cycle);
static void         ngx_http_push_stream_exit_master(ngx_cycle_t *cycle);
static ngx_int_t    ngx_http_push_stream_postconfig(ngx_conf_t *cf);
static void *       ngx_http_push_stream_create_main_conf(ngx_conf_t *cf);
static void *       ngx_http_push_stream_create_loc_conf(ngx_conf_t *cf);
static char *       ngx_http_push_stream_merge_loc_conf(ngx_conf_t *cf, void *parent, void *child);

// subscriber
static char *       ngx_http_push_stream_subscriber(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
static ngx_int_t    ngx_http_push_stream_subscriber_handler(ngx_http_request_t *r);
static ngx_int_t    ngx_http_push_stream_broadcast_locked(ngx_http_push_stream_channel_t *channel, ngx_http_push_stream_msg_t *msg, ngx_int_t status_code, const ngx_str_t *status_line, ngx_log_t *log, ngx_slab_pool_t *shpool);
#define ngx_http_push_stream_broadcast_status_locked(channel, status_code, status_line, log, shpool) ngx_http_push_stream_broadcast_locked(channel, NULL, status_code, status_line, log, shpool)
#define ngx_http_push_stream_broadcast_message_locked(channel, msg, log, shpool) ngx_http_push_stream_broadcast_locked(channel, msg, 0, NULL, log, shpool)
static ngx_int_t    ngx_http_push_stream_respond_to_subscribers(ngx_http_push_stream_channel_t *channel, ngx_http_push_stream_subscriber_t *sentinel, ngx_http_push_stream_msg_t *msg, ngx_int_t status_code, const ngx_str_t *status_line);
static void         ngx_http_push_stream_subscriber_cleanup(ngx_http_push_stream_subscriber_cleanup_t *data);
static void         ngx_http_push_stream_worker_subscriber_cleanup_locked(ngx_http_push_stream_worker_subscriber_t *worker_subscriber);

// publisher
static char *       ngx_http_push_stream_publisher(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
static ngx_int_t    ngx_http_push_stream_publisher_handler(ngx_http_request_t *r);
static void         ngx_http_push_stream_publisher_body_handler(ngx_http_request_t *r);

// channel
static void             ngx_http_push_stream_send_response_channel_id_not_provided(ngx_http_request_t *r);
static ngx_str_t *      ngx_http_push_stream_get_channel_id(ngx_http_request_t *r, ngx_http_push_stream_loc_conf_t *cf);
static ngx_int_t        ngx_http_push_stream_channel_info(ngx_http_request_t *r, ngx_str_t channelId, ngx_uint_t published_message_queue_size, ngx_uint_t stored_message_queue_size, ngx_uint_t subscriber_queue_size);
static ngx_int_t        ngx_http_push_stream_all_channels_info(ngx_http_request_t *r);
static ngx_http_push_stream_channel_t *     ngx_http_push_stream_get_channel(ngx_str_t *id, ngx_log_t *log);
static ngx_http_push_stream_channel_t *     ngx_http_push_stream_find_channel(ngx_str_t *id, ngx_log_t *log);
static ngx_int_t                            ngx_http_push_stream_delete_channel_locked(ngx_http_push_stream_channel_t *trash);
static ngx_http_push_stream_channel_t *     ngx_http_push_stream_clean_channel_locked(ngx_http_push_stream_channel_t *channel);

// channel messages
static void     ngx_http_push_stream_reserve_message_locked(ngx_http_push_stream_channel_t *channel, ngx_http_push_stream_msg_t *msg);
static void     ngx_http_push_stream_release_message_locked(ngx_http_push_stream_channel_t *channel, ngx_http_push_stream_msg_t *msg);
static ngx_inline void      ngx_http_push_stream_general_delete_message_locked(ngx_http_push_stream_channel_t *channel, ngx_http_push_stream_msg_t *msg, ngx_int_t force, ngx_slab_pool_t *shpool);
#define ngx_http_push_stream_delete_message_locked(channel, msg, shpool) ngx_http_push_stream_general_delete_message_locked(channel, msg, 0, shpool)
#define ngx_http_push_stream_force_delete_message_locked(channel, msg, shpool) ngx_http_push_stream_general_delete_message_locked(channel, msg, 1, shpool)
static ngx_inline void      ngx_http_push_stream_free_message_locked(ngx_http_push_stream_msg_t *msg, ngx_slab_pool_t *shpool);

// utilities
// general request handling
static void                 ngx_http_push_stream_copy_preallocated_buffer(ngx_buf_t *buf, ngx_buf_t *cbuf);
static ngx_table_elt_t *    ngx_http_push_stream_add_response_header(ngx_http_request_t *r, const ngx_str_t *header_name, const ngx_str_t *header_value);
static ngx_int_t            ngx_http_push_stream_respond_status_only(ngx_http_request_t *r, ngx_int_t status_code, const ngx_str_t *statusline);
static ngx_chain_t *        ngx_http_push_stream_create_output_chain_general(ngx_buf_t *buf, ngx_pool_t *pool, ngx_log_t *log, ngx_slab_pool_t *shpool);
#define ngx_http_push_stream_create_output_chain(buf, pool, log) ngx_http_push_stream_create_output_chain_general(buf, pool, log, NULL)
#define ngx_http_push_stream_create_output_chain_locked(buf, pool, log, shpool) ngx_http_push_stream_create_output_chain_general(buf, pool, log, shpool)
static ngx_int_t            ngx_http_push_stream_send_body_header(ngx_http_request_t *r, ngx_http_push_stream_loc_conf_t *pslcf);
static ngx_int_t            ngx_http_push_stream_send_ping(ngx_log_t *log, ngx_http_push_stream_loc_conf_t *pslcf);
static void                 ngx_http_push_stream_ping_timer_wake_handler(ngx_event_t *ev);
static void                 ngx_http_push_stream_ping_timer_set(ngx_http_push_stream_loc_conf_t *pslcf);
static void                 ngx_http_push_stream_ping_timer_reset(ngx_http_push_stream_loc_conf_t *pslcf);
static void                 ngx_http_push_stream_disconnect_timer_wake_handler(ngx_event_t *ev);
static void                 ngx_http_push_stream_disconnect_timer_set(ngx_http_push_stream_loc_conf_t *pslcf);
static void                 ngx_http_push_stream_disconnect_timer_reset(ngx_http_push_stream_loc_conf_t *pslcf);
static u_char *             ngx_http_push_stream_str_replace_locked(u_char *org, u_char *find, u_char *replace, ngx_pool_t *temp_pool);
static ngx_buf_t *          ngx_http_push_stream_get_formatted_message_locked(ngx_http_push_stream_loc_conf_t *pslcf, ngx_http_push_stream_channel_t *channel, ngx_buf_t *buf, ngx_pool_t *temp_pool);

// shared memory
static ngx_int_t    ngx_http_push_stream_set_up_shm(ngx_conf_t *cf, size_t shm_size);
static ngx_int_t    ngx_http_push_stream_init_shm_zone(ngx_shm_zone_t *shm_zone, void *data);
static ngx_int_t    ngx_http_push_stream_movezig_channel_locked(ngx_http_push_stream_channel_t *channel, ngx_slab_pool_t *shpool);

// ipc
static ngx_int_t        ngx_http_push_stream_init_ipc(ngx_cycle_t *cycle, ngx_int_t workers);
static void             ngx_http_push_stream_ipc_exit_worker(ngx_cycle_t *cycle);
static ngx_int_t        ngx_http_push_stream_init_ipc_shm(ngx_int_t workers);
static void             ngx_http_push_stream_channel_handler(ngx_event_t *ev);
static ngx_inline void  ngx_http_push_stream_process_worker_message(void);
static ngx_inline void  ngx_http_push_stream_send_worker_ping_message(void);
static ngx_inline void  ngx_http_push_stream_disconnect_worker_subscribers(ngx_flag_t force_disconnect);
static ngx_inline void  ngx_http_push_stream_census_worker_subscribers(void);
static void             ngx_http_push_stream_ipc_exit_worker(ngx_cycle_t *cycle);
static ngx_int_t        ngx_http_push_stream_send_worker_message(ngx_http_push_stream_channel_t *channel, ngx_http_push_stream_subscriber_t *subscriber_sentinel, ngx_pid_t pid, ngx_int_t worker_slot, ngx_http_push_stream_msg_t *msg, ngx_int_t status_code, ngx_log_t *log);
static ngx_int_t        ngx_http_push_stream_alert_worker(ngx_pid_t pid, ngx_int_t slot, ngx_log_t *log);
static ngx_int_t        ngx_http_push_stream_alert_worker_send_ping(ngx_pid_t pid, ngx_int_t slot, ngx_log_t *log);
static ngx_int_t        ngx_http_push_stream_alert_worker_disconnect_subscribers(ngx_pid_t pid, ngx_int_t slot, ngx_log_t *log);
static ngx_int_t        ngx_http_push_stream_alert_worker_census_subscribers(ngx_pid_t pid, ngx_int_t slot, ngx_log_t *log);

// constants
#define NGX_CMD_HTTP_PUSH_STREAM_CHECK_MESSAGES         49
#define NGX_CMD_HTTP_PUSH_STREAM_SEND_PING              50
#define NGX_CMD_HTTP_PUSH_STREAM_DISCONNECT_SUBSCRIBERS 51
#define NGX_CMD_HTTP_PUSH_STREAM_CENSUS_SUBSCRIBERS     52

static const ngx_str_t  NGX_HTTP_PUSH_STREAM_ALL_CHANNELS_INFO_ID = ngx_string("ALL");

#define NGX_HTTP_PUSH_STREAM_NO_CHANNEL_ID_MESSAGE "No channel id provided."

#define NGX_HTTP_PUSH_STREAM_MAX_CHANNEL_ID_LENGTH 1024 // bytes

#define NGX_HTTP_PUSH_STREAM_DEFAULT_SHM_SIZE       33554432 // 32 megs
#define NGX_HTTP_PUSH_STREAM_DEFAULT_BUFFER_TIMEOUT 7200

#define NGX_HTTP_PUSH_STREAM_DEFAULT_MAX_MESSAGES 10

#define NGX_HTTP_PUSH_STREAM_DEFAULT_HEADER_TEMPLATE  ""
#define NGX_HTTP_PUSH_STREAM_DEFAULT_MESSAGE_TEMPLATE ""

#define NGX_HTTP_PUSH_STREAM_DEFAULT_CONTENT_TYPE "text/plain"

#define NGX_HTTP_PUSH_STREAM_DEFAULT_BROADCAST_CHANNEL_PREFIX ""

static const ngx_str_t  NGX_HTTP_PUSH_STREAM_BACKTRACK_SEP = ngx_string(".b");
static const ngx_str_t  NGX_HTTP_PUSH_STREAM_SLASH = ngx_string("/");
static const ngx_str_t  NGX_HTTP_PUSH_STREAM_CRLF = ngx_string("\r\n");
static const ngx_str_t  NGX_HTTP_PUSH_STREAM_CONTENT_TYPE_TEXT_PLAIN = ngx_string("text/plain");
static const ngx_str_t  NGX_HTTP_PUSH_STREAM_CONTENT_TYPE_JSON = ngx_string("application/json");

static const ngx_str_t  NGX_PUSH_STREAM_TOKEN_MESSAGE_ID = ngx_string("~id~");
static const ngx_str_t  NGX_PUSH_STREAM_TOKEN_MESSAGE_CHANNEL = ngx_string("~channel~");
static const ngx_str_t  NGX_PUSH_STREAM_TOKEN_MESSAGE_TEXT = ngx_string("~text~");

static const ngx_str_t  NGX_PUSH_STREAM_PING_MESSAGE_ID = ngx_string("-1");
static const ngx_str_t  NGX_PUSH_STREAM_PING_MESSAGE_TEXT = ngx_string("");
static const ngx_str_t  NGX_PUSH_STREAM_PING_CHANNEL_ID = ngx_string("");

static const ngx_str_t  NGX_PUSH_STREAM_DATE_FORMAT_ISO_8601 = ngx_string("%4d-%02d-%02dT%02d:%02d:%02d");

// message codes
#define NGX_HTTP_PUSH_STREAM_MESSAGE_RECEIVED   9000
#define NGX_HTTP_PUSH_STREAM_MESSAGE_QUEUED     9001

#define NGX_HTTP_PUSH_STREAM_MESSAGE_FOUND      1000
#define NGX_HTTP_PUSH_STREAM_MESSAGE_EXPECTED   1001
#define NGX_HTTP_PUSH_STREAM_MESSAGE_EXPIRED    1002

#ifndef NGX_HTTP_CONFLICT
#define NGX_HTTP_CONFLICT 409
#endif

#ifndef NGX_HTTP_GONE
#define NGX_HTTP_GONE 410
#endif

#ifndef NGX_HTTP_CREATED
#define NGX_HTTP_CREATED 201
#endif

#ifndef NGX_HTTP_ACCEPTED
#define NGX_HTTP_ACCEPTED 202
#endif

// headers
static const ngx_str_t  NGX_HTTP_PUSH_STREAM_HEADER_ETAG = ngx_string("Etag");
static const ngx_str_t  NGX_HTTP_PUSH_STREAM_HEADER_VARY = ngx_string("Vary");
static const ngx_str_t  NGX_HTTP_PUSH_STREAM_HEADER_ALLOW = ngx_string("Allow");

// header values
//const ngx_str_t   NGX_HTTP_PUSH_CACHE_CONTROL_VALUE = ngx_string("no-cache");

// status strings
static const ngx_str_t  NGX_HTTP_PUSH_STREAM_HTTP_STATUS_409 = ngx_string("409 Conflict");
static const ngx_str_t  NGX_HTTP_PUSH_STREAM_HTTP_STATUS_410 = ngx_string("410 Gone");

// other stuff
static const ngx_str_t  NGX_HTTP_PUSH_STREAM_ALLOW_GET_POST_PUT_DELETE = ngx_string("GET, POST, PUT, DELETE");
static const ngx_str_t  NGX_HTTP_PUSH_STREAM_ALLOW_GET = ngx_string("GET");
static const ngx_str_t  NGX_HTTP_PUSH_STREAM_VARY_HEADER_VALUE = ngx_string("If-None-Match, If-Modified-Since");

static const ngx_str_t  NGX_HTTP_PUSH_STREAM_CHANNEL_INFO_PLAIN = ngx_string(
    "channel: %s" CRLF
    "published_messages: %ui" CRLF
    "stored_messages: %ui" CRLF
    "active_subscribers: %ui" CRLF
    "\0");

static const ngx_str_t  NGX_HTTP_PUSH_STREAM_CHANNEL_INFO_GROUP_HEAD_JSON = ngx_string("{\"hostname\": \"%s\", \"time\": \"%s\", \"infos\": [" CRLF);
static const ngx_str_t  NGX_HTTP_PUSH_STREAM_CHANNEL_INFO_GROUP_TAIL_JSON = ngx_string("]}" CRLF);
static const ngx_str_t  NGX_HTTP_PUSH_STREAM_CHANNEL_INFO_ITEM_SEP_JSON = ngx_string("," CRLF);
// have to be the same size of NGX_HTTP_PUSH_STREAM_CHANNEL_INFO_ITEM_SEP_JSON
static const ngx_str_t  NGX_HTTP_PUSH_STREAM_CHANNEL_INFO_ITEM_SEP_LAST_ITEM_JSON = ngx_string(" " CRLF);
static const ngx_str_t  NGX_HTTP_PUSH_STREAM_CHANNEL_INFO_JSON = ngx_string(
    "{\"channel\": \"%s\", "
    "\"published_messages\": \"%ui\", "
    "\"stored_messages\": \"%ui\", "
    "\"subscribers\": \"%ui\"}"
    "\0");

static const ngx_str_t  NGX_HTTP_PUSH_STREAM_CHANNEL_INFO_XML = ngx_string(
    "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>" CRLF
    "<channel>" CRLF
    "  <name>%s</name>" CRLF
    "  <published_messages>%ui</published_messages>" CRLF
    "  <stored_messages>%ui</stored_messages>" CRLF
    "  <subscribers>%ui</subscribers>" CRLF
    "</channel>" CRLF
    "\0");

static const ngx_str_t  NGX_HTTP_PUSH_STREAM_CHANNEL_INFO_YAML = ngx_string(
    "---" CRLF
    "channel: %s" CRLF
    "published_messages: %ui" CRLF
    "stored_messages: %ui" CRLF
    "subscribers %ui" CRLF
    "\0");

#endif /* NGX_HTTP_PUSH_STREAM_MODULE_H_ */
