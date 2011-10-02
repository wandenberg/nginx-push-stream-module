require File.expand_path('base_test_case', File.dirname(__FILE__))

class TestCreateManyChannels < Test::Unit::TestCase
  include BaseTestCase

  def config_test_create_many_channels
    @max_reserved_memory = "256m"
    @keepalive = "on"
  end

  def test_create_many_channels
    headers = {'accept' => 'application/json'}
    body = 'channel started'
    channels_to_be_created = 4000
    channel = 'ch_test_create_many_channels_'

    0.step(channels_to_be_created - 1, 10) do |i|
      socket = open_socket
      1.upto(10) do |j|
        channel_name = "#{channel}#{i + j}"
        headers, body = publish_message_in_socket(channel_name, body, socket)
        fail("Don't create the channel") unless headers.include?("HTTP/1.1 200 OK")
      end
      socket.close
    end
  end
end
