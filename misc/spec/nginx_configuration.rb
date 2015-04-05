module NginxConfiguration
  def self.default_configuration
    {
      :disable_start_stop_server => false,
      :master_process => 'on',
      :daemon => 'on',
      :workers => 2,

      :gzip => 'off',

      :content_type => 'text/html',

      :keepalive_requests => nil,
      :ping_message_interval => '10s',
      :header_template_file => nil,
      :header_template => %{<html><head><meta http-equiv=\\"Content-Type\\" content=\\"text/html; charset=utf-8\\">\\r\\n<meta http-equiv=\\"Cache-Control\\" content=\\"no-store\\">\\r\\n<meta http-equiv=\\"Cache-Control\\" content=\\"no-cache\\">\\r\\n<meta http-equiv=\\"Expires\\" content=\\"Thu, 1 Jan 1970 00:00:00 GMT\\">\\r\\n<script type=\\"text/javascript\\">\\r\\nwindow.onError = null;\\r\\ndocument.domain = \\'<%= nginx_host %>\\';\\r\\nparent.PushStream.register(this);\\r\\n</script>\\r\\n</head>\\r\\n<body onload=\\"try { parent.PushStream.reset(this) } catch (e) {}\\">},
      :message_template => "<script>p(~id~,'~channel~','~text~');</script>",
      :footer_template => "</body></html>",

      :store_messages => 'on',

      :subscriber_connection_ttl => nil,
      :longpolling_connection_ttl => nil,
      :timeout_with_body => 'off',
      :message_ttl => '50m',

      :max_channel_id_length => 200,
      :max_subscribers_per_channel => nil,
      :max_messages_stored_per_channel => 20,
      :max_number_of_channels => nil,
      :max_number_of_wildcard_channels => nil,

      :wildcard_channel_max_qtd => 3,
      :wildcard_channel_prefix => 'broad_',

      :subscriber_mode => nil,
      :publisher_mode => nil,
      :padding_by_user_agent => nil,

      :shared_memory_size => '10m',

      :channel_deleted_message_text => nil,
      :ping_message_text => nil,
      :last_received_message_time => nil,
      :last_received_message_tag => nil,
      :last_event_id => nil,
      :user_agent => nil,

      :authorized_channels_only => 'off',
      :allowed_origins => nil,

      :client_max_body_size => '32k',
      :client_body_buffer_size => '32k',

      :channel_info_on_publish => "on",
      :channel_inactivity_time => nil,

      :channel_id => '$arg_id',
      :channels_path_for_pub => '$arg_id',
      :channels_path => '$1',

      :events_channel_id => nil,
      :allow_connections_to_events_channel => nil,

      :extra_location => '',
      :extra_configuration => ''
    }
  end


  def self.template_configuration
  %(
pid               <%= pid_file %>;
error_log         <%= error_log %> info;

# Development Mode
master_process    <%= master_process %>;
daemon            <%= daemon %>;
worker_processes  <%= workers %>;
worker_rlimit_core  2500M;
working_directory <%= File.join(nginx_tests_tmp_dir, "cores", config_id) %>;
debug_points abort;

events {
  worker_connections  256;
  use                 <%= (RUBY_PLATFORM =~ /darwin/) ? 'kqueue' : 'epoll' %>;
}

http {
  default_type    application/octet-stream;

  access_log      <%= access_log %>;


  gzip             <%= gzip %>;
  gzip_buffers     16 4k;
  gzip_proxied     any;
  gzip_types       text/plain text/css application/x-javascript text/xml application/xml application/xml+rss text/javascript application/json;
  gzip_comp_level  9;
  gzip_http_version   1.0;

  tcp_nopush                      on;
  tcp_nodelay                     on;
  keepalive_timeout               100;
  <%= write_directive("keepalive_requests", keepalive_requests) %>
  send_timeout                    10;
  client_body_timeout             10;
  client_header_timeout           10;
  sendfile                        on;
  client_header_buffer_size       1k;
  large_client_header_buffers     2 4k;
  client_max_body_size            1k;
  client_body_buffer_size         1k;
  ignore_invalid_headers          on;
  client_body_in_single_buffer    on;
  client_body_temp_path           <%= client_body_temp %>;

  <%= write_directive("push_stream_ping_message_interval", ping_message_interval, "ping frequency") %>

  <%= write_directive("push_stream_message_template", message_template, "message template") %>

  <%= write_directive("push_stream_subscriber_connection_ttl", subscriber_connection_ttl, "timeout for subscriber connections") %>
  <%= write_directive("push_stream_longpolling_connection_ttl", longpolling_connection_ttl, "timeout for long polling connections") %>
  <%= write_directive("push_stream_timeout_with_body", timeout_with_body) %>
  <%= write_directive("push_stream_header_template", header_template, "header to be sent when receiving new subscriber connection") %>
  <%= write_directive("push_stream_header_template_file", header_template_file, "file with the header to be sent when receiving new subscriber connection") %>
  <%= write_directive("push_stream_message_ttl", message_ttl, "message ttl") %>
  <%= write_directive("push_stream_footer_template", footer_template, "footer to be sent when finishing subscriber connection") %>

  <%= write_directive("push_stream_max_channel_id_length", max_channel_id_length) %>
  <%= write_directive("push_stream_max_subscribers_per_channel", max_subscribers_per_channel, "max subscribers per channel") %>
  <%= write_directive("push_stream_max_messages_stored_per_channel", max_messages_stored_per_channel, "max messages to store in memory") %>
  <%= write_directive("push_stream_max_number_of_channels", max_number_of_channels) %>
  <%= write_directive("push_stream_max_number_of_wildcard_channels", max_number_of_wildcard_channels) %>

  <%= write_directive("push_stream_wildcard_channel_max_qtd", wildcard_channel_max_qtd) %>
  <%= write_directive("push_stream_wildcard_channel_prefix", wildcard_channel_prefix) %>

  <%= write_directive("push_stream_padding_by_user_agent", padding_by_user_agent) %>

  <%= write_directive("push_stream_authorized_channels_only", authorized_channels_only, "subscriber may create channels on demand or only authorized (publisher) may do it?") %>

  <%= write_directive("push_stream_shared_memory_size", shared_memory_size) %>

  <%= write_directive("push_stream_user_agent", user_agent) %>

  <%= write_directive("push_stream_allowed_origins", allowed_origins) %>

  <%= write_directive("push_stream_last_received_message_time", last_received_message_time) %>
  <%= write_directive("push_stream_last_received_message_tag", last_received_message_tag) %>
  <%= write_directive("push_stream_last_event_id", last_event_id) %>

  <%= write_directive("push_stream_channel_deleted_message_text", channel_deleted_message_text) %>

  <%= write_directive("push_stream_ping_message_text", ping_message_text) %>
  <%= write_directive("push_stream_channel_inactivity_time", channel_inactivity_time) %>

  <%= write_directive("push_stream_events_channel_id", events_channel_id) %>
  <%= write_directive("push_stream_allow_connections_to_events_channel", allow_connections_to_events_channel) %>

  server {
    listen        <%= nginx_port %>;
    server_name   <%= nginx_host %>;

    location /channels-stats {
      # activate channels statistics mode for this location
      push_stream_channels_statistics;

      <%= write_directive("push_stream_channels_path", channels_path_for_pub) %>
    }

    location /pub {
      # activate publisher mode for this location
      push_stream_publisher <%= publisher_mode unless publisher_mode.nil? || publisher_mode == "normal" %>;

      <%= write_directive("push_stream_channels_path", channels_path_for_pub) %>
      <%= write_directive("push_stream_store_messages", store_messages, "store messages") %>
      <%= write_directive("push_stream_channel_info_on_publish", channel_info_on_publish, "channel_info_on_publish") %>

      # client_max_body_size MUST be equal to client_body_buffer_size or
      # you will be sorry.
      client_max_body_size                    <%= client_max_body_size %>;
      client_body_buffer_size                 <%= client_body_buffer_size %>;
    }

    location ~ /sub/(.*)? {
      # activate subscriber mode for this location
      push_stream_subscriber <%= subscriber_mode unless subscriber_mode.nil? || subscriber_mode == "streaming" %>;

      # positional channel path
      <%= write_directive("push_stream_channels_path", channels_path) %>
      <%= write_directive("default_type", content_type, "content-type") %>
    }

    <%= extra_location %>
  }
}

<%= extra_configuration %>
  )
  end
end
