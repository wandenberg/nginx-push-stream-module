/*
 * ngx_http_push_stream_module_publisher.h
 *
 *  Created on: Oct 26, 2010
 *      Authors: Wandenberg Peixoto <wandenberg@gmail.com> & Rogério Schneider <stockrt@gmail.com>
 */

#ifndef NGX_HTTP_PUSH_STREAM_MODULE_PUBLISHER_H_
#define NGX_HTTP_PUSH_STREAM_MODULE_PUBLISHER_H_

#include <ngx_http_push_stream_module.h>

static ngx_int_t    push_stream_channels_statistics_handler(ngx_http_request_t *r);
static ngx_int_t    ngx_http_push_stream_publisher_handler(ngx_http_request_t *r);
static void         ngx_http_push_stream_publisher_body_handler(ngx_http_request_t *r);

#endif /* NGX_HTTP_PUSH_STREAM_MODULE_PUBLISHER_H_ */
