require 'rubygems'
require 'popen4'
require 'erb'
require 'fileutils'
require 'ruby-debug'
require 'test/unit'
require 'eventmachine'
require 'em-http'
require 'json'
require 'socket'

module BaseTestCase
  def setup
    create_dirs
    config_log_and_pid_file
    default_configuration
    @test_config_file = "#{method_name_for_test}.conf"
    config_test_name = "config_#{method_name_for_test}"
    self.send(config_test_name) if self.respond_to?(config_test_name)

    self.create_config_file
    unless @disable_start_stop_server
      self.start_server
    end
  end

  def teardown
    old_cld_trap = Signal.trap("CLD", "IGNORE") unless @disable_ignore_childs
    unless @disable_start_stop_server
      self.stop_server
    end
    self.delete_config_and_log_files
    Signal.trap("CLD", old_cld_trap) unless @disable_ignore_childs
  end

  def nginx_executable
    if is_to_use_memory_check
      return "valgrind --show-reachable=yes --trace-children=yes --time-stamp=yes --leak-check=full --log-file=mld_#{method_name_for_test}.log #{ENV['NGINX_EXEC'].nil? ? "/usr/local/nginx/sbin/nginx" : ENV['NGINX_EXEC']}"
    else
      return ENV['NGINX_EXEC'].nil? ? "/usr/local/nginx/sbin/nginx" : ENV['NGINX_EXEC']
    end
  end

  def is_to_use_memory_check
    !ENV['CHECK_MEMORY'].nil? and !`which valgrind`.empty?
  end

  def nginx_address
    return "http://#{nginx_host}:#{nginx_port}"
  end

  def nginx_host
    return ENV['NGINX_HOST'].nil? ? "127.0.0.1" : ENV['NGINX_HOST']
  end

  def nginx_port
    return ENV['NGINX_PORT'].nil? ? "9990" : ENV['NGINX_PORT']
  end

  def nginx_workers
    return ENV['NGINX_WORKERS'].nil? ? "1" : ENV['NGINX_WORKERS']
  end

  def nginx_tests_tmp_dir
    return ENV['NGINX_TESTS_TMP_DIR'].nil? ? "tmp" : ENV['NGINX_TESTS_TMP_DIR']
  end

  def start_server
    error_message = ""
    status = POpen4::popen4("#{ nginx_executable } -c #{ config_filename }") do |stdout, stderr, stdin, pid|
      error_message = stderr.read.strip unless stderr.eof
      return error_message unless error_message.nil?
    end
    assert_equal(0, status.exitstatus, "Server doesn't started - #{error_message}")
  end

  def stop_server
    error_message = ""
    status = POpen4::popen4("#{ nginx_executable } -c #{ config_filename } -s stop") do |stdout, stderr, stdin, pid|
      error_message = stderr.read.strip unless stderr.eof
      return error_message unless error_message.nil?
    end
    assert_equal(0, status.exitstatus, "Server doesn't stop - #{error_message}")
  end

  def create_config_file
    template = ERB.new @config_template || @@config_template
    config_content = template.result(binding)
    File.open(config_filename, 'w') {|f| f.write(config_content) }
    File.open(mime_types_filename, 'w') {|f| f.write(@@mime_tipes_template) }
  end

  def delete_config_and_log_files
    if has_passed?
      File.delete(config_filename) if File.exist?(config_filename)
      File.delete(mime_types_filename) if File.exist?(mime_types_filename)
      File.delete(@main_error_log) if File.exist?(@main_error_log)
      File.delete(@access_log) if File.exist?(@access_log)
      File.delete(@error_log) if File.exist?(@error_log)
      FileUtils.rm_rf(@client_body_temp) if File.exist?(@client_body_temp)
    end
  end

  def create_dirs
    FileUtils.mkdir(nginx_tests_tmp_dir) unless File.exist?(nginx_tests_tmp_dir) and File.directory?(nginx_tests_tmp_dir)
    FileUtils.mkdir("#{nginx_tests_tmp_dir}/client_body_temp") unless File.exist?("#{nginx_tests_tmp_dir}/client_body_temp") and File.directory?("#{nginx_tests_tmp_dir}/client_body_temp")
    FileUtils.mkdir("#{nginx_tests_tmp_dir}/logs") unless File.exist?("#{nginx_tests_tmp_dir}/logs") and File.directory?("#{nginx_tests_tmp_dir}/logs")
  end

  def has_passed?
    @test_passed.nil? ? @passed : @test_passed
  end

  def config_log_and_pid_file
    @client_body_temp = File.expand_path("#{nginx_tests_tmp_dir}/client_body_temp")
    @pid_file = File.expand_path("#{nginx_tests_tmp_dir}/logs/nginx.pid")
    @main_error_log = File.expand_path("#{nginx_tests_tmp_dir}/logs/nginx-main_error-#{method_name_for_test}.log")
    @access_log = File.expand_path("#{nginx_tests_tmp_dir}/logs/nginx-http_access-#{method_name_for_test}.log")
    @error_log = File.expand_path("#{nginx_tests_tmp_dir}/logs/nginx-http_error-#{method_name_for_test}.log")
  end

  def config_filename
    File.expand_path("#{nginx_tests_tmp_dir}/#{ @test_config_file }")
  end

  def mime_types_filename
    File.expand_path("#{nginx_tests_tmp_dir}/mime.types")
  end

  def method_name_for_test
    self.respond_to?('method_name') ? self.method_name : self.__name__
  end

  def time_diff_milli(start, finish)
     (finish - start) * 1000.0
  end

  def time_diff_sec(start, finish)
     (finish - start)
  end

  def default_configuration
    @master_process = 'off'
    @daemon = 'off'
    @max_reserved_memory = '10m'
    @authorized_channels_only = 'off'
    @broadcast_channel_max_qtd = 3
    @broadcast_channel_prefix = 'broad_'
    @content_type = 'text/html; charset=utf-8'
    @header_template = %{<html><head><meta http-equiv=\\"Content-Type\\" content=\\"text/html; charset=utf-8\\">\\r\\n<meta http-equiv=\\"Cache-Control\\" content=\\"no-store\\">\\r\\n<meta http-equiv=\\"Cache-Control\\" content=\\"no-cache\\">\\r\\n<meta http-equiv=\\"Expires\\" content=\\"Thu, 1 Jan 1970 00:00:00 GMT\\">\\r\\n<script type=\\"text/javascript\\">\\r\\nwindow.onError = null;\\r\\ndocument.domain = \\'#{nginx_host}\\';\\r\\nparent.PushStream.register(this);\\r\\n</script>\\r\\n</head>\\r\\n<body onload=\\"try { parent.PushStream.reset(this) } catch (e) {}\\">}
    @footer_template = %{</body></html>}
    @max_channel_id_length = 200
    @max_message_buffer_length = 20
    @max_subscribers_per_channel = nil
    @max_number_of_broadcast_channels = nil
    @max_number_of_channels = nil
    @message_template = %{<script>p(~id~,\\'~channel~\\',\\'~text~\\');</script>}
    @min_message_buffer_timeout = '50m'
    @ping_message_interval = '10s'
    @store_messages = 'on'
    @subscriber_connection_timeout = nil
    @longpolling_connection_timeout = nil
    @memory_cleanup_timeout = '5m'
    @config_template = nil
    @keepalive = 'off'
    @channel_deleted_message_text = nil
    @ping_message_text = nil
    @subscriber_eventsource = 'off'
    @subscriber_mode = nil
    @publisher_mode = nil
    @last_received_message_time = nil
    @last_received_message_tag = nil
    @user_agent = nil
    @padding_by_user_agent = nil

    self.send(:global_configuration) if self.respond_to?(:global_configuration)
  end

  def publish_message_inline_with_callbacks(channel, headers, body, callbacks = {})
    pub = EventMachine::HttpRequest.new(nginx_address + '/pub?id=' + channel.to_s).post :head => headers, :body => body, :timeout => 30
    pub.callback do
      if pub.response_header.status == 200
        callbacks[:success].call(pub.response_header.status, pub.response) unless callbacks[:success].nil?
      else
        callbacks[:error].call(pub.response_header.status, pub.response) unless callbacks[:error].nil?
      end
    end
    pub
  end

  def publish_message(channel, headers, body)
    EventMachine.run {
      pub = publish_message_inline(channel, headers, body) do
        assert_not_equal(0, pub.response_header.content_length, "Empty response was received")
        response = JSON.parse(pub.response)
        assert_equal(channel, response["channel"].to_s, "Channel was not recognized")
        EventMachine.stop
      end
      add_test_timeout
    }
  end

  def publish_message_inline(channel, headers, body, &block)
    pub = EventMachine::HttpRequest.new(nginx_address + '/pub?id=' + channel.to_s).post :head => headers, :body => body, :timeout => 30
    pub.callback {
      fail("Request was not accepted") if pub.response_header.status != 200
      block.call unless block.nil?
    }
    pub
  end

  def publish_message_in_socket(channel, body, socket)
    post_channel_message = "POST /pub?id=#{channel} HTTP/1.0\r\nContent-Length: #{body.size}\r\n\r\n#{body}"
    socket.print(post_channel_message)
    read_response(socket)
  end

  def create_channel_by_subscribe(channel, headers, timeout=60, &block)
    EventMachine.run {
      sub_1 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => headers, :timeout => timeout
      sub_1.stream { |chunk|
        block.call
      }
      sub_1.callback {
        EventMachine.stop
      }
    }
  end

  def add_test_timeout(timeout=5)
    EM.add_timer(timeout) do
      fail("Test timeout reached")
      EventMachine.stop
    end
  end

  def open_socket
    TCPSocket.open(nginx_host, nginx_port)
  end

  def read_response(socket, wait_for=nil)
    response ||= socket.readpartial(1)
    while (tmp = socket.read_nonblock(256))
      response += tmp
    end
  rescue Errno::EAGAIN => e
    headers, body = (response || "").split("\r\n\r\n", 2)
    if !wait_for.nil? && (body.nil? || body.empty? || !body.include?(wait_for))
      IO.select([socket])
      retry
    end
  ensure
    fail("Any response") if response.nil?
    headers, body = response.split("\r\n\r\n", 2)
    return headers, body
  end

  @@config_template = %q{
pid                     <%= @pid_file %>;
error_log               <%= @main_error_log %> debug;
# Development Mode
master_process  <%=@master_process%>;
daemon          <%=@daemon%>;
worker_processes        <%=nginx_workers%>;

events {
    worker_connections  1024;
    use                 <%= (RUBY_PLATFORM =~ /darwin/) ? 'kqueue' : 'epoll' %>;
}

http {
    include         mime.types;
    default_type    application/octet-stream;

    access_log      <%= @access_log %>;
    error_log       <%= @error_log %> debug;

    tcp_nopush                      on;
    tcp_nodelay                     on;
    keepalive_timeout               10;
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
    client_body_temp_path           <%= @client_body_temp %>;
    <%= "push_stream_shared_memory_size #{@max_reserved_memory};" unless @max_reserved_memory.nil? %>
    <%= "push_stream_shared_memory_cleanup_objects_ttl #{@memory_cleanup_timeout};" unless @memory_cleanup_timeout.nil? %>
    <%= %{push_stream_channel_deleted_message_text "#{@channel_deleted_message_text}";} unless @channel_deleted_message_text.nil? %>
    <%= %{push_stream_ping_message_text "#{@ping_message_text}";} unless @ping_message_text.nil? %>
    <%= %{push_stream_broadcast_channel_prefix "#{@broadcast_channel_prefix}";} unless @broadcast_channel_prefix.nil? %>
    <%= "push_stream_max_number_of_channels #{@max_number_of_channels};" unless @max_number_of_channels.nil? %>
    <%= "push_stream_max_number_of_broadcast_channels #{@max_number_of_broadcast_channels};" unless @max_number_of_broadcast_channels.nil? %>

    <%= "push_stream_last_received_message_time #{@last_received_message_time};" unless @last_received_message_time.nil? %>
    <%= "push_stream_last_received_message_tag #{@last_received_message_tag};" unless @last_received_message_tag.nil? %>

    <%= "push_stream_user_agent #{@user_agent};" unless @user_agent.nil? %>
    <%= "push_stream_padding_by_user_agent '#{@padding_by_user_agent}';" unless @padding_by_user_agent.nil? %>

    # max subscribers per channel
    <%= "push_stream_max_subscribers_per_channel #{@max_subscribers_per_channel};" unless @max_subscribers_per_channel.nil? %>
    # max messages to store in memory
    <%= "push_stream_max_messages_stored_per_channel #{@max_message_buffer_length};" unless @max_message_buffer_length.nil? %>
    # message ttl
    <%= "push_stream_message_ttl #{@min_message_buffer_timeout};" unless @min_message_buffer_timeout.nil? %>
    <%= "push_stream_max_channel_id_length #{@max_channel_id_length};" unless @max_channel_id_length.nil? %>
    # ping frequency
    <%= "push_stream_ping_message_interval #{@ping_message_interval};" unless @ping_message_interval.nil? %>
    # connection ttl to enable recycle
    <%= "push_stream_subscriber_connection_ttl #{@subscriber_connection_timeout};" unless @subscriber_connection_timeout.nil? %>
    # timeout for long polling connections
    <%= "push_stream_longpolling_connection_ttl #{@longpolling_connection_ttl};" unless @longpolling_connection_ttl.nil? %>

    # header to be sent when receiving new subscriber connection
    <%= %{push_stream_header_template "#{@header_template}";} unless @header_template.nil? %>
    # message template
    <%= %{push_stream_message_template "#{@message_template}";} unless @message_template.nil? %>
    # footer to be sent when finishing subscriber connection
    <%= %{push_stream_footer_template "#{@footer_template}";} unless @footer_template.nil? %>
    # subscriber may create channels on demand or only authorized
    # (publisher) may do it?
    <%= "push_stream_authorized_channels_only #{@authorized_channels_only};" unless @authorized_channels_only.nil? %>
    <%= "push_stream_broadcast_channel_max_qtd #{@broadcast_channel_max_qtd};" unless @broadcast_channel_max_qtd.nil? %>

    <%= "push_stream_allowed_origins #{@allowed_origins};" unless @allowed_origins.nil? %>

    server {
        listen          <%=nginx_port%>;
        server_name     <%=nginx_host%>;

        location /channels-stats {
            # activate channels statistics mode for this location
            push_stream_channels_statistics;

            # query string based channel id
            set $push_stream_channel_id             $arg_id;

            # keepalive
            <%= "push_stream_keepalive #{@keepalive};" unless @keepalive.nil? %>
        }

        location /pub {
            # activate publisher mode for this location
            push_stream_publisher <%= @publisher_mode unless @publisher_mode.nil? || @publisher_mode == "normal" %>;

            # query string based channel id
            set $push_stream_channel_id             $arg_id;
            # store messages
            <%= "push_stream_store_messages #{@store_messages};" unless @store_messages.nil? %>
            # keepalive
            <%= "push_stream_keepalive #{@keepalive};" unless @keepalive.nil? %>

            # client_max_body_size MUST be equal to client_body_buffer_size or
            # you will be sorry.
            client_max_body_size                    <%= @client_max_body_size.nil? ? '32k' : @client_max_body_size %>;
            client_body_buffer_size                 <%= @client_body_buffer_size.nil? ? '32k' : @client_body_buffer_size %>;
        }

        location ~ /sub/(.*)? {
            # activate subscriber mode for this location
            push_stream_subscriber <%= @subscriber_mode unless @subscriber_mode.nil? || @subscriber_mode == "streaming" %>;

            # activate event source support for this location
            <%= "push_stream_eventsource_support #{@subscriber_eventsource};" unless @subscriber_eventsource.nil? %>

            # positional channel path
            set $push_stream_channels_path          $1;
            # content-type
            <%= %{push_stream_content_type "#{@content_type}";} unless @content_type.nil? %>
        }

        <%= @extra_location %>
    }
}
  }

  @@mime_tipes_template = %q{
types {
    text/html                             html htm shtml;
    text/css                              css;
    text/xml                              xml;
    image/gif                             gif;
    image/jpeg                            jpeg jpg;
    application/x-javascript              js;
    application/atom+xml                  atom;
    application/rss+xml                   rss;

    text/mathml                           mml;
    text/plain                            txt;
    text/vnd.sun.j2me.app-descriptor      jad;
    text/vnd.wap.wml                      wml;
    text/x-component                      htc;

    image/png                             png;
    image/tiff                            tif tiff;
    image/vnd.wap.wbmp                    wbmp;
    image/x-icon                          ico;
    image/x-jng                           jng;
    image/x-ms-bmp                        bmp;
    image/svg+xml                         svg;

    application/java-archive              jar war ear;
    application/mac-binhex40              hqx;
    application/msword                    doc;
    application/pdf                       pdf;
    application/postscript                ps eps ai;
    application/rtf                       rtf;
    application/vnd.ms-excel              xls;
    application/vnd.ms-powerpoint         ppt;
    application/vnd.wap.wmlc              wmlc;
    application/vnd.wap.xhtml+xml         xhtml;
    application/vnd.google-earth.kml+xml  kml;
    application/vnd.google-earth.kmz      kmz;
    application/x-cocoa                   cco;
    application/x-java-archive-diff       jardiff;
    application/x-java-jnlp-file          jnlp;
    application/x-makeself                run;
    application/x-perl                    pl pm;
    application/x-pilot                   prc pdb;
    application/x-rar-compressed          rar;
    application/x-redhat-package-manager  rpm;
    application/x-sea                     sea;
    application/x-shockwave-flash         swf;
    application/x-stuffit                 sit;
    application/x-tcl                     tcl tk;
    application/x-x509-ca-cert            der pem crt;
    application/x-xpinstall               xpi;
    application/zip                       zip;

    application/octet-stream              bin exe dll;
    application/octet-stream              deb;
    application/octet-stream              dmg;
    application/octet-stream              eot;
    application/octet-stream              iso img;
    application/octet-stream              msi msp msm;

    audio/midi                            mid midi kar;
    audio/mpeg                            mp3;
    audio/x-realaudio                     ra;

    video/3gpp                            3gpp 3gp;
    video/mpeg                            mpeg mpg;
    video/quicktime                       mov;
    video/x-flv                           flv;
    video/x-mng                           mng;
    video/x-ms-asf                        asx asf;
    video/x-ms-wmv                        wmv;
    video/x-msvideo                       avi;
}
  }

end
