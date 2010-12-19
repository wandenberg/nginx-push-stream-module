require 'rubygems'
require 'popen4'
require 'test/unit'
require File.expand_path('base_test_case', File.dirname(__FILE__))

class TestSetuParameters < Test::Unit::TestCase
  include BaseTestCase

  def test_min_buffer_messages_greater_them_max_buffer_messages
    expected_error_message = "push_stream_max_message_buffer_length cannot be smaller than push_stream_min_message_buffer_length"
    @test_config_file = "test_min_buffer_messages_greater_them_max_buffer_messages.conf"
    @max_message_buffer_length = 20
    @min_message_buffer_length = 21

    self.create_config_file
    stderr_msg = self.start_server
    assert(stderr_msg.include?(expected_error_message), "Message error not founded: '#{ expected_error_message }' recieved #{ stderr_msg }")
  end

end
