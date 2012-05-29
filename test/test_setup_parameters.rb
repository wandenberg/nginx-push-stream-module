require File.expand_path('base_test_case', File.dirname(__FILE__))

class TestSetuParameters < Test::Unit::TestCase
  include BaseTestCase

  def global_configuration
    @disable_start_stop_server = true
  end

  def test_ping_message_interval_cannot_be_zero
    expected_error_message = "push_stream_ping_message_interval cannot be zero"
    @ping_message_interval = 0

    self.create_config_file
    stderr_msg = self.start_server
    assert(stderr_msg.include?(expected_error_message), "Message error not founded: '#{ expected_error_message }' recieved '#{ stderr_msg }'")
  end

  def test_message_template_cannot_be_blank
    expected_error_message = "push_stream_message_template cannot be blank"
    @message_template = ""

    self.create_config_file
    stderr_msg = self.start_server
    assert(stderr_msg.include?(expected_error_message), "Message error not founded: '#{ expected_error_message }' recieved '#{ stderr_msg }'")
  end

  def test_subscriber_connection_timeout_cannot_be_zero
    expected_error_message = "push_stream_subscriber_connection_ttl cannot be zero"
    @subscriber_connection_timeout = 0

    self.create_config_file
    stderr_msg = self.start_server
    assert(stderr_msg.include?(expected_error_message), "Message error not founded: '#{ expected_error_message }' recieved '#{ stderr_msg }'")
  end

  def test_longpolling_connection_ttl_cannot_be_zero
    expected_error_message = "push_stream_longpolling_connection_ttl cannot be zero"
    @longpolling_connection_ttl = 0

    self.create_config_file
    stderr_msg = self.start_server
    assert(stderr_msg.include?(expected_error_message), "Message error not founded: '#{ expected_error_message }' recieved '#{ stderr_msg }'")
  end

  def test_max_channel_id_length_cannot_be_zero
    expected_error_message = "push_stream_max_channel_id_length cannot be zero"
    @max_channel_id_length = 0

    self.create_config_file
    stderr_msg = self.start_server
    assert(stderr_msg.include?(expected_error_message), "Message error not founded: '#{ expected_error_message }' recieved '#{ stderr_msg }'")
  end

  def test_min_message_buffer_timeout_cannot_be_zero
    expected_error_message = "push_stream_message_ttl cannot be zero"
    @min_message_buffer_timeout = 0

    self.create_config_file
    stderr_msg = self.start_server
    assert(stderr_msg.include?(expected_error_message), "Message error not founded: '#{ expected_error_message }' recieved '#{ stderr_msg }'")
  end


  def test_max_subscribers_per_channel_cannot_be_zero
    expected_error_message = "push_stream_max_subscribers_per_channel cannot be zero"
    @max_subscribers_per_channel = 0

    self.create_config_file
    stderr_msg = self.start_server
    assert(stderr_msg.include?(expected_error_message), "Message error not founded: '#{ expected_error_message }' recieved '#{ stderr_msg }'")
  end

  def test_max_messages_stored_per_channel_cannot_be_zero
    expected_error_message = "push_stream_max_messages_stored_per_channel cannot be zero"
    @max_message_buffer_length = 0

    self.create_config_file
    stderr_msg = self.start_server
    assert(stderr_msg.include?(expected_error_message), "Message error not founded: '#{ expected_error_message }' recieved '#{ stderr_msg }'")
  end

  def test_broadcast_channel_max_qtd_cannot_be_zero
    expected_error_message = "push_stream_broadcast_channel_max_qtd cannot be zero"
    @broadcast_channel_max_qtd = 0

    self.create_config_file
    stderr_msg = self.start_server
    assert(stderr_msg.include?(expected_error_message), "Message error not founded: '#{ expected_error_message }' recieved '#{ stderr_msg }'")
  end

  def test_broadcast_channel_max_qtd_cannot_be_set_without_broadcast_channel_prefix
    expected_error_message = "cannot set broadcast channel max qtd if push_stream_broadcast_channel_prefix is not set or blank"
    @broadcast_channel_max_qtd = 1

    self.create_config_file
    stderr_msg = self.start_server
    assert(stderr_msg.include?(expected_error_message), "Message error not founded: '#{ expected_error_message }' recieved '#{ stderr_msg }'")
  end

  def test_broadcast_channel_max_qtd_cannot_be_set_without_broadcast_channel_prefix
    expected_error_message = "cannot set broadcast channel max qtd if push_stream_broadcast_channel_prefix is not set or blank"
    @broadcast_channel_max_qtd = 1
    @broadcast_channel_prefix = ""

    self.create_config_file
    stderr_msg = self.start_server
    assert(stderr_msg.include?(expected_error_message), "Message error not founded: '#{ expected_error_message }' recieved '#{ stderr_msg }'")
  end

  def test_max_number_of_channels_cannot_be_zero
    expected_error_message = "push_stream_max_number_of_channels cannot be zero"
    @max_number_of_channels = 0

    self.create_config_file
    stderr_msg = self.start_server
    assert(stderr_msg.include?(expected_error_message), "Message error not founded: '#{ expected_error_message }' recieved '#{ stderr_msg }'")
  end

  def test_max_number_of_broadcast_channels_cannot_be_zero
    expected_error_message = "push_stream_max_number_of_broadcast_channels cannot be zero"
    @max_number_of_broadcast_channels = 0

    self.create_config_file
    stderr_msg = self.start_server
    assert(stderr_msg.include?(expected_error_message), "Message error not founded: '#{ expected_error_message }' recieved '#{ stderr_msg }'")
  end

  def test_max_number_of_broadcast_channels_cannot_be_smaller_than_broadcast_channel_max_qtd
    expected_error_message = "max number of broadcast channels cannot be smaller than value in push_stream_broadcast_channel_max_qtd"
    @max_number_of_broadcast_channels = 3
    @broadcast_channel_max_qtd = 4
    @broadcast_channel_prefix = "broad_"

    self.create_config_file
    stderr_msg = self.start_server
    assert(stderr_msg.include?(expected_error_message), "Message error not founded: '#{ expected_error_message }' recieved '#{ stderr_msg }'")
  end

  def test_memory_cleanup_timeout
    expected_error_message = "memory cleanup objects ttl cannot't be less than 30."
    @memory_cleanup_timeout = '15s'

    self.create_config_file
    stderr_msg = self.start_server
    assert(stderr_msg.include?(expected_error_message), "Message error not founded: '#{ expected_error_message }' recieved '#{ stderr_msg }'")
  end

  def config_test_http_not_configured
    @test_config_file = "test_http_not_configured.conf"
    @config_template = %q{
      pid                     <%= @pid_file %>;
      error_log               <%= @main_error_log %> debug;
      # Development Mode
      master_process  off;
      daemon          off;
      worker_processes        <%=nginx_workers%>;

      events {
          worker_connections  1024;
          use                 <%= (RUBY_PLATFORM =~ /darwin/) ? 'kqueue' : 'epoll' %>;
      }
    }
  end

  def test_http_not_configured
    expected_error_message = "ngx_http_push_stream_module will not be used with this configuration."

    self.create_config_file
    self.start_server
    log_file = File.read(@main_error_log)
    assert(log_file.include?(expected_error_message), "Message error not founded: '#{ expected_error_message }' recieved '#{ log_file }'")
  ensure
    self.stop_server
  end

  def test_invalid_push_mode
    expected_error_message = "invalid push_stream_subscriber mode value: unknown, accepted values (streaming, polling, long-polling)"
    @subscriber_mode = "unknown"

    self.create_config_file
    stderr_msg = self.start_server
    assert(stderr_msg.include?(expected_error_message), "Message error not founded: '#{ expected_error_message }' recieved '#{ stderr_msg }'")
  end

  def test_valid_push_mode
    expected_error_message = "invalid push_stream_subscriber mode value"

    @subscriber_mode = ""

    self.create_config_file
    stderr_msg = self.start_server
    assert(!stderr_msg.include?(expected_error_message), "Message error founded: '#{ stderr_msg }'")

    self.stop_server

    @subscriber_mode = "streaming"

    self.create_config_file
    stderr_msg = self.start_server
    assert(!stderr_msg.include?(expected_error_message), "Message error founded: '#{ stderr_msg }'")

    self.stop_server

    @subscriber_mode = "polling"

    self.create_config_file
    stderr_msg = self.start_server
    assert(!stderr_msg.include?(expected_error_message), "Message error founded: '#{ stderr_msg }'")

    self.stop_server

    @subscriber_mode = "long-polling"

    self.create_config_file
    stderr_msg = self.start_server
    assert(!stderr_msg.include?(expected_error_message), "Message error founded: '#{ stderr_msg }'")

    self.stop_server
  end

  def test_invalid_publisher_mode
    expected_error_message = "invalid push_stream_publisher mode value: unknown, accepted values (normal, admin)"
    @publisher_mode = "unknown"

    self.create_config_file
    stderr_msg = self.start_server
    assert(stderr_msg.include?(expected_error_message), "Message error not founded: '#{ expected_error_message }' recieved '#{ stderr_msg }'")
  end

  def test_valid_publisher_mode
    expected_error_message = "invalid push_stream_publisher mode value"

    @publisher_mode = ""

    self.create_config_file
    stderr_msg = self.start_server
    assert(!stderr_msg.include?(expected_error_message), "Message error founded: '#{ stderr_msg }'")

    self.stop_server

    @publisher_mode = "normal"

    self.create_config_file
    stderr_msg = self.start_server
    assert(!stderr_msg.include?(expected_error_message), "Message error founded: '#{ stderr_msg }'")

    self.stop_server

    @publisher_mode = "admin"

    self.create_config_file
    stderr_msg = self.start_server
    assert(!stderr_msg.include?(expected_error_message), "Message error founded: '#{ stderr_msg }'")

    self.stop_server
  end


  def test_event_source_not_available_on_publisher_statistics_and_websocket_locations
    expected_error_message = "push stream module: event source support is only available on subscriber location"

    @extra_location = %q{
      location ~ /test/ {
        push_stream_websocket;
        push_stream_eventsource_support on;
      }
    }

    self.create_config_file
    stderr_msg = self.start_server
    assert(stderr_msg.include?(expected_error_message), "Message error not founded: '#{ expected_error_message }' recieved '#{ stderr_msg }'")

    @extra_location = %q{
      location ~ /test/ {
        push_stream_publisher;
        push_stream_eventsource_support on;
      }
    }

    self.create_config_file
    stderr_msg = self.start_server
    assert(stderr_msg.include?(expected_error_message), "Message error not founded: '#{ expected_error_message }' recieved '#{ stderr_msg }'")

    @extra_location = %q{
      location ~ /test/ {
        push_stream_channels_statistics;
        push_stream_eventsource_support on;
      }
    }

    self.create_config_file
    stderr_msg = self.start_server
    assert(stderr_msg.include?(expected_error_message), "Message error not founded: '#{ expected_error_message }' recieved '#{ stderr_msg }'")
  end

  def test_padding_by_user_agent_parser
    expected_error_message = "push stream module: padding pattern not match the value "

    @padding_by_user_agent = "user_agent,as,df"

    self.create_config_file
    stderr_msg = self.start_server
    assert(stderr_msg.include?(expected_error_message + "user_agent,as,df"), "Message error not founded: '#{ expected_error_message + "user_agent,as,df" }' recieved '#{ stderr_msg }'")


    @padding_by_user_agent = "user_agent;10;0"

    self.create_config_file
    stderr_msg = self.start_server
    assert(stderr_msg.include?(expected_error_message + "user_agent;10;0"), "Message error not founded: '#{ expected_error_message + "user_agent;10;0" }' recieved '#{ stderr_msg }'")


    expected_error_message = "error applying padding pattern to "

    @padding_by_user_agent = "user_agent,10,0:other_user_agent;20;0:another_user_agent,30,0"

    self.create_config_file
    stderr_msg = self.start_server
    assert(stderr_msg.include?(expected_error_message + "other_user_agent;20;0:another_user_agent,30,0"), "Message error not founded: '#{ expected_error_message + "other_user_agent;20;0:another_user_agent,30,0" }' recieved '#{ stderr_msg }'")
  end

end
