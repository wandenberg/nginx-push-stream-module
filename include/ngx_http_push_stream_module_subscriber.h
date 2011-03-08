/*
 * ngx_http_push_stream_module_subscriber.h
 *
 *  Created on: Oct 26, 2010
 *      Authors: Wandenberg Peixoto <wandenberg@gmail.com> & Rogério Schneider <stockrt@gmail.com>
 */

#ifndef NGX_HTTP_PUSH_STREAM_MODULE_SUBSCRIBER_H_
#define NGX_HTTP_PUSH_STREAM_MODULE_SUBSCRIBER_H_

typedef struct {
    ngx_queue_t                     queue; // this MUST be first
    ngx_str_t                      *id;
    ngx_uint_t                      backtrack_messages;
} ngx_http_push_stream_requested_channel_t;

static ngx_int_t    ngx_http_push_stream_subscriber_handler(ngx_http_request_t *r);
ngx_http_push_stream_requested_channel_t * ngx_http_push_stream_parse_channels_ids_from_path(ngx_http_request_t *r, ngx_pool_t *pool);

static void         ngx_http_push_stream_subscriber_cleanup(ngx_http_push_stream_subscriber_cleanup_t *data);

static ngx_int_t ngx_http_push_stream_subscriber_assign_channel(ngx_slab_pool_t *shpool, ngx_http_push_stream_loc_conf_t *cf, ngx_http_request_t *r, ngx_http_push_stream_requested_channel_t *requested_channel, ngx_http_push_stream_subscription_t *subscriptions_sentinel, ngx_pool_t *temp_pool);


#endif /* NGX_HTTP_PUSH_STREAM_MODULE_SUBSCRIBER_H_ */
