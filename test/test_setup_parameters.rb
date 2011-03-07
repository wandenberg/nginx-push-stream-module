require File.expand_path('base_test_case', File.dirname(__FILE__))

class TestSetuParameters < Test::Unit::TestCase
  include BaseTestCase

  def initialize(opts)
    super(opts)
    @disable_start_stop_server = true
  end

  def test_ping_message_interval_cannot_be_zero
    expected_error_message = "push_stream_ping_message_interval cannot be zero"
    @ping_message_interval = 0

    self.create_config_file
    stderr_msg = self.start_server
    assert(stderr_msg.include?(expected_error_message), "Message error not founded: '#{ expected_error_message }' recieved '#{ stderr_msg }'")
  end

  def test_ping_message_interval_cannot_be_set_without_a_message_template
    expected_error_message = "cannot have ping message if push_stream_message_template is not set or blank"
    @ping_message_interval = "1s"
    @message_template = nil

    self.create_config_file
    stderr_msg = self.start_server
    assert(stderr_msg.include?(expected_error_message), "Message error not founded: '#{ expected_error_message }' recieved '#{ stderr_msg }'")
  end

  def test_ping_message_interval_cannot_be_set_if_message_template_is_blank
    expected_error_message = "cannot have ping message if push_stream_message_template is not set or blank"
    @ping_message_interval = "1s"
    @message_template = ""

    self.create_config_file
    stderr_msg = self.start_server
    assert(stderr_msg.include?(expected_error_message), "Message error not founded: '#{ expected_error_message }' recieved '#{ stderr_msg }'")
  end

  def test_subscriber_disconnect_interval_cannot_be_zero
    expected_error_message = "push_stream_subscriber_disconnect_interval cannot be zero"
    @subscriber_disconnect_interval = 0

    self.create_config_file
    stderr_msg = self.start_server
    assert(stderr_msg.include?(expected_error_message), "Message error not founded: '#{ expected_error_message }' recieved '#{ stderr_msg }'")
  end

  def test_subscriber_connection_timeout_cannot_be_zero
    expected_error_message = "push_stream_subscriber_connection_timeout cannot be zero"
    @subscriber_connection_timeout = 0

    self.create_config_file
    stderr_msg = self.start_server
    assert(stderr_msg.include?(expected_error_message), "Message error not founded: '#{ expected_error_message }' recieved '#{ stderr_msg }'")
  end

  def test_subscriber_disconnect_interval_cannot_be_set_without_a_connection_timeout
    expected_error_message = "cannot set subscriber disconnect interval if push_stream_subscriber_connection_timeout is not set or zero"
    @subscriber_disconnect_interval = "1s"

    self.create_config_file
    stderr_msg = self.start_server
    assert(stderr_msg.include?(expected_error_message), "Message error not founded: '#{ expected_error_message }' recieved '#{ stderr_msg }'")
  end

  def test_subscriber_connection_timeout_cannot_be_set_without_a_disconnect_interval
    expected_error_message = "cannot set subscriber connection timeout if push_stream_subscriber_disconnect_interval is not set or zero"
    @subscriber_connection_timeout = "1s"

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
    expected_error_message = "push_stream_min_message_buffer_timeout cannot be zero"
    @min_message_buffer_timeout = 0

    self.create_config_file
    stderr_msg = self.start_server
    assert(stderr_msg.include?(expected_error_message), "Message error not founded: '#{ expected_error_message }' recieved '#{ stderr_msg }'")
  end

  def test_max_message_buffer_length_cannot_be_zero
    expected_error_message = "push_stream_max_message_buffer_length cannot be zero"
    @max_message_buffer_length = 0

    self.create_config_file
    stderr_msg = self.start_server
    assert(stderr_msg.include?(expected_error_message), "Message error not founded: '#{ expected_error_message }' recieved '#{ stderr_msg }'")
  end

  def test_store_messages_cannot_be_set_without_set_max_message_buffer_length_or_min_message_buffer_timeout
    expected_error_message = "push_stream_store_messages cannot be set without set max message buffer length or min message buffer timeout"
    @store_messages = 'on'
    @min_message_buffer_timeout = nil
    @max_message_buffer_length = nil

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

  def test_broadcast_channel_prefix_cannot_be_set_without_broadcast_channel_max_qtd
    expected_error_message = "cannot set broadcast channel prefix if push_stream_broadcast_channel_max_qtd is not set"
    @broadcast_channel_prefix = "broad_"
    @broadcast_channel_max_qtd = nil

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
    expected_error_message = "memory cleanup timeout cannot't be less than 30."
    @memory_cleanup_timeout = '15s'

    self.create_config_file
    stderr_msg = self.start_server
    assert(stderr_msg.include?(expected_error_message), "Message error not founded: '#{ expected_error_message }' recieved '#{ stderr_msg }'")
  end
end
