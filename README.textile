h1(#nginx_push_stream_module). Nginx Push Stream Module

A pure stream http push technology for your Nginx setup.

"Comet":comet_ref made easy and *really scalable*.

Supports "EventSource":eventsource_ref, "WebSocket":websocket_ref, Long Polling, and Forever Iframe. See "some examples":examples bellow.

_This module is not distributed with the Nginx source. See "the installation instructions":installation._

Available on github at "nginx_push_stream_module":repository

h1(#changelog). Changelog

Always take a look at "CHANGELOG.textile":changelog to see what's new.


h1(#contribute). Contribute

After you try this module and like it, feel free to "give something back":donate, and help in the maintenance of the project ;)
!https://www.paypalobjects.com/WEBSCR-640-20110429-1/en_US/i/btn/btn_donate_LG.gif!:donate

h1(#status). Status

This module is considered production ready.

h1(#basic-configuration). Basic Configuration

<pre>
    # add the push_stream_shared_memory_size to your http context
    http {
       push_stream_shared_memory_size 32M;

        # define publisher and subscriber endpoints in your server context
        server {
           location /channels-stats {
                # activate channels statistics mode for this location
                push_stream_channels_statistics;

                # query string based channel id
                push_stream_channels_path               $arg_id;
            }

            location /pub {
               # activate publisher (admin) mode for this location
               push_stream_publisher admin;

                # query string based channel id
                push_stream_channels_path               $arg_id;
            }

            location ~ /sub/(.*) {
                # activate subscriber (streaming) mode for this location
                push_stream_subscriber;

                # positional channel path
                push_stream_channels_path                   $1;
            }
        }
    }
</pre>


h1(#basic-usage). Basic Usage

You can feel the flavor right now at the command line. Try using more than
one terminal and start playing http pubsub:

<pre>
    # Subs
    curl -s -v --no-buffer 'http://localhost/sub/my_channel_1'
    curl -s -v --no-buffer 'http://localhost/sub/your_channel_1'
    curl -s -v --no-buffer 'http://localhost/sub/your_channel_2'

    # Pubs
    curl -s -v -X POST 'http://localhost/pub?id=my_channel_1' -d 'Hello World!'
    curl -s -v -X POST 'http://localhost/pub?id=your_channel_1' -d 'Hi everybody!'
    curl -s -v -X POST 'http://localhost/pub?id=your_channel_2' -d 'Goodbye!'

    # Channels Stats for publisher (json format)
    curl -s -v 'http://localhost/pub?id=my_channel_1'

    # All Channels Stats summarized (json format)
    curl -s -v 'http://localhost/channels-stats'

    # All Channels Stats detailed (json format)
    curl -s -v 'http://localhost/channels-stats?id=ALL'

    # Prefixed Channels Stats detailed (json format)
    curl -s -v 'http://localhost/channels-stats?id=your_channel_*'

    # Channels Stats (json format)
    curl -s -v 'http://localhost/channels-stats?id=my_channel_1'

    # Delete Channels
    curl -s -v -X DELETE 'http://localhost/pub?id=my_channel_1'
</pre>


h1(#examples). Some Examples <a name="examples" href="#">&nbsp;</a>

* "Curl examples":curl
* "Forever (hidden) iFrame":forever_iframe
* "Event Source":event_source
* "WebSocket":websocket
* "Long Polling":long_polling
* "JSONP":jsonp
* "M-JPEG":m-jpeg
* "Other examples":wiki


h1(#FAQ). FAQ <a names="faq" href="#">&nbsp;</a>

Doubts?! Check the "FAQ":wiki.

h1(#bug_report). Bug report <a name="bug_report" href="#">&nbsp;</a>

To report a bug, please provide the following information when applicable

# Which push stream module version is been used (commit sha1)?
# Which nginx version is been used?
# Nginx configuration in use
# "nginx -V" command outuput
# Core dump indicating a failure on the module code. Check "here":nginx_debugging how to produce one.
# Step by step description to reproduce the error.

h1(#who). Who is using the module? <a names="faq" href="#">&nbsp;</a>

Do you use this module? Put your name on the "list":wiki.


h1(#javascript_client). Javascript Client <a name="javascript_client" href="#">&nbsp;</a>

There is a javascript client implementation "here":javascript_client, which is framework independent. Try and help improve it. ;)

h1(#directives). Directives

(1) Defining locations, (2) Main configuration, (3) Subscribers configuration, (4) Publishers configuration, (5) Channels Statistics configuration, (6) WebSocket configuration

(head). | Directive | (1) | (2) | (3) | (4) | (5) | (6) |
| "push_stream_channels_statistics":push_stream_channels_statistics | &nbsp;&nbsp;x | &nbsp;&nbsp;- | &nbsp;&nbsp;- | &nbsp;&nbsp;- | &nbsp;&nbsp;- | &nbsp;&nbsp;- |
| "push_stream_publisher":push_stream_publisher | &nbsp;&nbsp;x | &nbsp;&nbsp;- | &nbsp;&nbsp;- | &nbsp;&nbsp;- | &nbsp;&nbsp;- | &nbsp;&nbsp;- |
| "push_stream_subscriber":push_stream_subscriber | &nbsp;&nbsp;x | &nbsp;&nbsp;- | &nbsp;&nbsp;- | &nbsp;&nbsp;- | &nbsp;&nbsp;- | &nbsp;&nbsp;- |
| "push_stream_shared_memory_size":push_stream_shared_memory_size | &nbsp;&nbsp;- | &nbsp;&nbsp;x | &nbsp;&nbsp;- | &nbsp;&nbsp;- | &nbsp;&nbsp;- | &nbsp;&nbsp;- |
| "push_stream_channel_deleted_message_text":push_stream_channel_deleted_message_text | &nbsp;&nbsp;- | &nbsp;&nbsp;x | &nbsp;&nbsp;- | &nbsp;&nbsp;- | &nbsp;&nbsp;- | &nbsp;&nbsp;- |
| "push_stream_channel_inactivity_time":push_stream_channel_inactivity_time | &nbsp;&nbsp;- | &nbsp;&nbsp;x | &nbsp;&nbsp;- | &nbsp;&nbsp;- | &nbsp;&nbsp;- | &nbsp;&nbsp;- |
| "push_stream_ping_message_text":push_stream_ping_message_text | &nbsp;&nbsp;- | &nbsp;&nbsp;x | &nbsp;&nbsp;- | &nbsp;&nbsp;- | &nbsp;&nbsp;- | &nbsp;&nbsp;- |
| "push_stream_timeout_with_body":push_stream_timeout_with_body | &nbsp;&nbsp;- | &nbsp;&nbsp;x | &nbsp;&nbsp;- | &nbsp;&nbsp;- | &nbsp;&nbsp;- | &nbsp;&nbsp;- |
| "push_stream_message_ttl":push_stream_message_ttl | &nbsp;&nbsp;- | &nbsp;&nbsp;x | &nbsp;&nbsp;- | &nbsp;&nbsp;- | &nbsp;&nbsp;- | &nbsp;&nbsp;- |
| "push_stream_max_subscribers_per_channel":push_stream_max_subscribers_per_channel | &nbsp;&nbsp;- | &nbsp;&nbsp;x | &nbsp;&nbsp;- | &nbsp;&nbsp;- | &nbsp;&nbsp;- | &nbsp;&nbsp;- |
| "push_stream_max_messages_stored_per_channel":push_stream_max_messages_stored_per_channel | &nbsp;&nbsp;- | &nbsp;&nbsp;x | &nbsp;&nbsp;- | &nbsp;&nbsp;- | &nbsp;&nbsp;- | &nbsp;&nbsp;- |
| "push_stream_max_channel_id_length":push_stream_max_channel_id_length | &nbsp;&nbsp;- | &nbsp;&nbsp;x | &nbsp;&nbsp;- | &nbsp;&nbsp;- | &nbsp;&nbsp;- | &nbsp;&nbsp;- |
| "push_stream_max_number_of_channels":push_stream_max_number_of_channels | &nbsp;&nbsp;- | &nbsp;&nbsp;x | &nbsp;&nbsp;- | &nbsp;&nbsp;- | &nbsp;&nbsp;- | &nbsp;&nbsp;- |
| "push_stream_max_number_of_wildcard_channels":push_stream_max_number_of_wildcard_channels | &nbsp;&nbsp;- | &nbsp;&nbsp;x | &nbsp;&nbsp;- | &nbsp;&nbsp;- | &nbsp;&nbsp;- | &nbsp;&nbsp;- |
| "push_stream_wildcard_channel_prefix":push_stream_wildcard_channel_prefix | &nbsp;&nbsp;- | &nbsp;&nbsp;x | &nbsp;&nbsp;- | &nbsp;&nbsp;- | &nbsp;&nbsp;- | &nbsp;&nbsp;- |
| "push_stream_events_channel_id":push_stream_events_channel_id | &nbsp;&nbsp;- | &nbsp;&nbsp;x | &nbsp;&nbsp;- | &nbsp;&nbsp;- | &nbsp;&nbsp;- | &nbsp;&nbsp;- |
| "push_stream_channels_path":push_stream_channels_path | &nbsp;&nbsp;- | &nbsp;&nbsp;- | &nbsp;&nbsp;x | &nbsp;&nbsp;x | &nbsp;&nbsp;x | &nbsp;&nbsp;x |
| "push_stream_store_messages":push_stream_store_messages | &nbsp;&nbsp;- | &nbsp;&nbsp;- | &nbsp;&nbsp;- | &nbsp;&nbsp;x | &nbsp;&nbsp;- | &nbsp;&nbsp;x |
| "push_stream_channel_info_on_publish":push_stream_channel_info_on_publish | &nbsp;&nbsp;- | &nbsp;&nbsp;- | &nbsp;&nbsp;- | &nbsp;&nbsp;x | &nbsp;&nbsp;- | &nbsp;&nbsp;- |
| "push_stream_authorized_channels_only":push_stream_authorized_channels_only | &nbsp;&nbsp;- | &nbsp;&nbsp;- | &nbsp;&nbsp;x | &nbsp;&nbsp;- | &nbsp;&nbsp;- | &nbsp;&nbsp;x |
| "push_stream_header_template_file":push_stream_header_template_file | &nbsp;&nbsp;- | &nbsp;&nbsp;- | &nbsp;&nbsp;x | &nbsp;&nbsp;- | &nbsp;&nbsp;- | &nbsp;&nbsp;x |
| "push_stream_header_template":push_stream_header_template | &nbsp;&nbsp;- | &nbsp;&nbsp;- | &nbsp;&nbsp;x | &nbsp;&nbsp;- | &nbsp;&nbsp;- | &nbsp;&nbsp;x |
| "push_stream_message_template":push_stream_message_template | &nbsp;&nbsp;- | &nbsp;&nbsp;- | &nbsp;&nbsp;x | &nbsp;&nbsp;- | &nbsp;&nbsp;- | &nbsp;&nbsp;x |
| "push_stream_footer_template":push_stream_footer_template | &nbsp;&nbsp;- | &nbsp;&nbsp;- | &nbsp;&nbsp;x | &nbsp;&nbsp;- | &nbsp;&nbsp;- | &nbsp;&nbsp;x |
| "push_stream_wildcard_channel_max_qtd":push_stream_wildcard_channel_max_qtd | &nbsp;&nbsp;- | &nbsp;&nbsp;- | &nbsp;&nbsp;x | &nbsp;&nbsp;- | &nbsp;&nbsp;- | &nbsp;&nbsp;x |
| "push_stream_ping_message_interval":push_stream_ping_message_interval | &nbsp;&nbsp;- | &nbsp;&nbsp;- | &nbsp;&nbsp;x | &nbsp;&nbsp;- | &nbsp;&nbsp;- | &nbsp;&nbsp;x |
| "push_stream_subscriber_connection_ttl":push_stream_subscriber_connection_ttl | &nbsp;&nbsp;- | &nbsp;&nbsp;- | &nbsp;&nbsp;x | &nbsp;&nbsp;- | &nbsp;&nbsp;- | &nbsp;&nbsp;x |
| "push_stream_longpolling_connection_ttl":push_stream_longpolling_connection_ttl | &nbsp;&nbsp;- | &nbsp;&nbsp;- | &nbsp;&nbsp;x | &nbsp;&nbsp;- | &nbsp;&nbsp;- | &nbsp;&nbsp;- |
| "push_stream_websocket_allow_publish":push_stream_websocket_allow_publish | &nbsp;&nbsp;- | &nbsp;&nbsp;- | &nbsp;&nbsp;- | &nbsp;&nbsp;- | &nbsp;&nbsp;- | &nbsp;&nbsp;x |
| "push_stream_last_received_message_time":push_stream_last_received_message_time | &nbsp;&nbsp;- | &nbsp;&nbsp;- | &nbsp;&nbsp;x | &nbsp;&nbsp;- | &nbsp;&nbsp;- | &nbsp;&nbsp;- |
| "push_stream_last_received_message_tag":push_stream_last_received_message_tag | &nbsp;&nbsp;- | &nbsp;&nbsp;- | &nbsp;&nbsp;x | &nbsp;&nbsp;- | &nbsp;&nbsp;- | &nbsp;&nbsp;- |
| "push_stream_last_event_id":push_stream_last_event_id | &nbsp;&nbsp;- | &nbsp;&nbsp;- | &nbsp;&nbsp;x | &nbsp;&nbsp;- | &nbsp;&nbsp;- | &nbsp;&nbsp;- |
| "push_stream_user_agent":push_stream_user_agent | &nbsp;&nbsp;- | &nbsp;&nbsp;- | &nbsp;&nbsp;x | &nbsp;&nbsp;- | &nbsp;&nbsp;- | &nbsp;&nbsp;- |
| "push_stream_padding_by_user_agent":push_stream_padding_by_user_agent | &nbsp;&nbsp;- | &nbsp;&nbsp;- | &nbsp;&nbsp;x | &nbsp;&nbsp;- | &nbsp;&nbsp;- | &nbsp;&nbsp;- |
| "push_stream_allowed_origins":push_stream_allowed_origins | &nbsp;&nbsp;- | &nbsp;&nbsp;- | &nbsp;&nbsp;x | &nbsp;&nbsp;- | &nbsp;&nbsp;- | &nbsp;&nbsp;- |
| "push_stream_allow_connections_to_events_channel":push_stream_allow_connections_to_events_channel | &nbsp;&nbsp;- | &nbsp;&nbsp;- | &nbsp;&nbsp;x | &nbsp;&nbsp;- | &nbsp;&nbsp;- | &nbsp;&nbsp;x |

h1(#installation). Installation <a name="installation" href="#">&nbsp;</a>

<pre>
    # clone the project
    git clone https://github.com/wandenberg/nginx-push-stream-module.git
    NGINX_PUSH_STREAM_MODULE_PATH=$PWD/nginx-push-stream-module

    # get desired nginx version (works with 1.2.0+)
    wget http://nginx.org/download/nginx-1.2.0.tar.gz

    # unpack, configure and build
    tar xzvf nginx-1.2.0.tar.gz
    cd nginx-1.2.0
    ./configure --add-module=../nginx-push-stream-module
    make

    # install and finish
    sudo make install

    # check
    sudo /usr/local/nginx/sbin/nginx -v
        nginx version: nginx/1.2.0

    # test configuration
    sudo /usr/local/nginx/sbin/nginx -c $NGINX_PUSH_STREAM_MODULE_PATH/misc/nginx.conf -t
        the configuration file $NGINX_PUSH_STREAM_MODULE_PATH/misc/nginx.conf syntax is ok
        configuration file $NGINX_PUSH_STREAM_MODULE_PATH/misc/nginx.conf test is successful

    # run
    sudo /usr/local/nginx/sbin/nginx -c $NGINX_PUSH_STREAM_MODULE_PATH/misc/nginx.conf
</pre>

h1(#memory-usage). Memory usage

Just as information is listed below the minimum amount of memory used for each object:

* message on shared = 200 bytes
* channel on shared = 270 bytes
* subscriber
  ** on shared = 160 bytes
  ** on system = 6550 bytes

h1(#tests). Tests

The server tests for this module are written in Ruby, and are acceptance tests, click "here":tests for more details.

h1(#discussion). Discussion

Nginx Push Stream Module "Discussion Group":discussion

h1(#contributors). Contributors

"People":contributors

[discussion]https://groups.google.com/group/nginxpushstream
[donate]https://www.paypal.com/cgi-bin/webscr?cmd=_s-xclick&hosted_button_id=4LP6P9A7BC37S
[eventsource_ref]http://dev.w3.org/html5/eventsource/
[websocket_ref]http://dev.w3.org/html5/websockets/
[comet_ref]http://en.wikipedia.org/wiki/Comet_%28programming%29
[installation]#installation
[examples]#examples
[javascript_client]docs/javascript_client.textile#javascript_client
[repository]https://github.com/wandenberg/nginx-push-stream-module
[contributors]https://github.com/wandenberg/nginx-push-stream-module/contributors
[changelog]CHANGELOG.textile
[curl]docs/examples/curl.textile#curl
[forever_iframe]docs/examples/forever_iframe.textile#forever_iframe
[event_source]docs/examples/event_source.textile#event_source
[websocket]docs/examples/websocket.textile#websocket
[long_polling]docs/examples/long_polling.textile#long_polling
[jsonp]docs/examples/long_polling.textile#jsonp
[m-jpeg]docs/examples/m_jpeg.textile#m_jpeg
[tests]docs/server_tests.textile
[push_stream_channels_statistics]docs/directives/channels_statistics.textile#push_stream_channels_statistics
[push_stream_publisher]docs/directives/publishers.textile#push_stream_publisher
[push_stream_subscriber]docs/directives/subscribers.textile#push_stream_subscriber
[push_stream_shared_memory_size]docs/directives/main.textile#push_stream_shared_memory_size
[push_stream_channel_deleted_message_text]docs/directives/main.textile#push_stream_channel_deleted_message_text
[push_stream_ping_message_text]docs/directives/main.textile#push_stream_ping_message_text
[push_stream_channel_inactivity_time]docs/directives/main.textile#push_stream_channel_inactivity_time
[push_stream_message_ttl]docs/directives/main.textile#push_stream_message_ttl
[push_stream_max_subscribers_per_channel]docs/directives/main.textile#push_stream_max_subscribers_per_channel
[push_stream_max_messages_stored_per_channel]docs/directives/main.textile#push_stream_max_messages_stored_per_channel
[push_stream_max_channel_id_length]docs/directives/main.textile#push_stream_max_channel_id_length
[push_stream_max_number_of_channels]docs/directives/main.textile#push_stream_max_number_of_channels
[push_stream_max_number_of_wildcard_channels]docs/directives/main.textile#push_stream_max_number_of_wildcard_channels
[push_stream_wildcard_channel_prefix]docs/directives/main.textile#push_stream_wildcard_channel_prefix
[push_stream_events_channel_id]docs/directives/main.textile#push_stream_events_channel_id
[push_stream_channels_path]docs/directives/subscribers.textile#push_stream_channels_path
[push_stream_authorized_channels_only]docs/directives/subscribers.textile#push_stream_authorized_channels_only
[push_stream_header_template_file]docs/directives/subscribers.textile#push_stream_header_template_file
[push_stream_header_template]docs/directives/subscribers.textile#push_stream_header_template
[push_stream_message_template]docs/directives/subscribers.textile#push_stream_message_template
[push_stream_footer_template]docs/directives/subscribers.textile#push_stream_footer_template
[push_stream_wildcard_channel_max_qtd]docs/directives/subscribers.textile#push_stream_wildcard_channel_max_qtd
[push_stream_ping_message_interval]docs/directives/subscribers.textile#push_stream_ping_message_interval
[push_stream_subscriber_connection_ttl]docs/directives/subscribers.textile#push_stream_subscriber_connection_ttl
[push_stream_longpolling_connection_ttl]docs/directives/subscribers.textile#push_stream_longpolling_connection_ttl
[push_stream_timeout_with_body]docs/directives/subscribers.textile#push_stream_timeout_with_body
[push_stream_last_received_message_time]docs/directives/subscribers.textile#push_stream_last_received_message_time
[push_stream_last_received_message_tag]docs/directives/subscribers.textile#push_stream_last_received_message_tag
[push_stream_last_event_id]docs/directives/subscribers.textile#push_stream_last_event_id
[push_stream_user_agent]docs/directives/subscribers.textile#push_stream_user_agent
[push_stream_padding_by_user_agent]docs/directives/subscribers.textile#push_stream_padding_by_user_agent
[push_stream_store_messages]docs/directives/publishers.textile#push_stream_store_messages
[push_stream_channel_info_on_publish]docs/directives/publishers.textile#push_stream_channel_info_on_publish
[push_stream_allowed_origins]docs/directives/subscribers.textile#push_stream_allowed_origins
[push_stream_websocket_allow_publish]docs/directives/subscribers.textile#push_stream_websocket_allow_publish
[push_stream_allow_connections_to_events_channel]docs/directives/subscribers.textile#push_stream_allow_connections_to_events_channel
[wiki]https://github.com/wandenberg/nginx-push-stream-module/wiki/_pages
[nginx_debugging]http://wiki.nginx.org/Debugging
