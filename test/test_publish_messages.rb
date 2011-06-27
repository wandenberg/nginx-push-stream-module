require File.expand_path('base_test_case', File.dirname(__FILE__))

class TestPublishMessages < Test::Unit::TestCase
  include BaseTestCase

  def config_test_publish_messages
    @header_template = nil
    @message_template = "~text~"
  end

  def test_publish_messages
    headers = {'accept' => 'text/html'}
    body = 'published unique message'
    channel = 'ch_test_publish_messages'

    EventMachine.run {
      sub = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => headers
      sub.stream { | chunk |
        assert_equal(body + "\r\n", chunk, "The published message was not received correctly")
        EventMachine.stop
      }

      pub = EventMachine::HttpRequest.new(nginx_address + '/pub?id=' + channel.to_s ).post :head => headers, :body => body, :timeout => 30
    }
  end

  def config_test_publish_many_messages_in_the_same_channel
    @header_template = nil
    @message_template = "~text~"
    @max_reserved_memory = "256m"
    @ping_message_interval = nil
  end

  def test_publish_many_messages_in_the_same_channel
    headers = {'accept' => 'text/html'}
    body_prefix = 'published message '
    channel = 'ch_test_publish_many_messages_in_the_same_channel'
    messagens_to_publish = 1500

    response = ""
    EventMachine.run {
      sub = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => headers
      sub.stream { | chunk |
        response += chunk
        recieved_messages = response.split("\r\n")

        if recieved_messages.length >= messagens_to_publish
          assert_equal(body_prefix + messagens_to_publish.to_s, recieved_messages.last, "Didn't receive all messages")
          EventMachine.stop
        end
      }

      req = EventMachine::HttpRequest.new(nginx_address + '/pub?id=' + channel.to_s )

      i = 0
      EM.add_periodic_timer(0.001) do
        i += 1
        if i <= messagens_to_publish
          pub = req.post :head => headers, :body => body_prefix + i.to_s, :timeout => 30
          pub.callback { fail("Massage was not published: " + body_prefix + i.to_s) if pub.response_header.status != 200 }
        end
      end
    }
  end
end
