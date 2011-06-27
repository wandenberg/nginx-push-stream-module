require File.expand_path('base_test_case', File.dirname(__FILE__))

class TestCreateManyChannels < Test::Unit::TestCase
  include BaseTestCase

  def config_test_create_many_channels
    @max_reserved_memory = "256m"
  end

  def test_create_many_channels
    headers = {'accept' => 'application/json'}
    body = 'channel started'
    channels_to_be_created = 4000
    channel = 'ch_test_create_many_channels_'

    0.step(channels_to_be_created - 1, 10) do |i|
      EventMachine.run {
        publish_message_inline("#{channel}#{i + 1}", headers, body)
        publish_message_inline("#{channel}#{i + 2}", headers, body)
        publish_message_inline("#{channel}#{i + 3}", headers, body)
        publish_message_inline("#{channel}#{i + 4}", headers, body)
        publish_message_inline("#{channel}#{i + 5}", headers, body)
        publish_message_inline("#{channel}#{i + 6}", headers, body)
        publish_message_inline("#{channel}#{i + 7}", headers, body)
        publish_message_inline("#{channel}#{i + 8}", headers, body)
        publish_message_inline("#{channel}#{i + 9}", headers, body)
        publish_message_inline("#{channel}#{i + 10}", headers, body) { EventMachine.stop }
      }
    end
  end
end
