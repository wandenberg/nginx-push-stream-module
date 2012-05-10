require File.expand_path('base_test_case', File.dirname(__FILE__))
require 'time'

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

  def config_test_publish_messages_with_different_bytes
    @header_template = nil
    @message_template = "~text~"
    @ping_message_interval = nil
    @client_max_body_size = '65k'
    @client_body_buffer_size = '65k'
  end

  def test_publish_messages_with_different_bytes
    headers = {'accept' => 'text/html'}
    channel = 'ch_test_publish_messages_with_different_bytes'

    ranges = [1..63, 64..127, 128..191, 192..255]
    ranges.each do |range|
      bytes = []
      range.each do |i|
        1.upto(255) do |j|
          bytes << "%s%s" % [i.chr, j.chr]
        end
      end

      body = bytes.join('')
      response = ''

      EventMachine.run {
        sub = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => headers
        sub.stream { | chunk |
          response += chunk
          if response.include?(body)
            assert_equal(body + "\r\n", response, "The published message was not received correctly")
            EventMachine.stop
          end
        }

        pub = EventMachine::HttpRequest.new(nginx_address + '/pub?id=' + channel.to_s ).post :head => headers, :body => body, :timeout => 30
        add_test_timeout
      }
    end
  end

  def config_test_publish_large_messages
    @header_template = nil
    @footer_template = nil
    @message_template = "~text~"
    @ping_message_interval = nil
    @client_max_body_size = '2000k'
    @client_body_buffer_size = '2000k'
    @subscriber_connection_timeout = '5s'
  end

  def test_publish_large_messages
    headers = {'accept' => 'text/html'}
    channel = 'ch_test_publish_large_messages'

    body = "|123456789" * 102400 + "|"
    response = ''

    EventMachine.run {
      sub = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => headers
      sub.stream { | chunk |
        response += chunk
        if response.include?(body)
          assert_equal(body + "\r\n", response, "The published message was not received correctly")
        end
      }
      sub.callback {
        response = ''
        sub_1 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s + ".b1").get :head => headers
        sub_1.stream { | chunk |
          response += chunk
          if response.include?(body)
            assert_equal(body + "\r\n", response, "The published message was not received correctly")
            EventMachine.stop
          end
        }
        sub_1.callback {
          EventMachine.stop
        }
      }

      pub = EventMachine::HttpRequest.new(nginx_address + '/pub?id=' + channel.to_s ).post :head => headers, :body => body, :timeout => 30
      add_test_timeout(15)
    }
  end

  def config_test_publish_many_messages_in_the_same_channel
    @header_template = nil
    @message_template = "~text~"
    @max_reserved_memory = "256m"
    @ping_message_interval = nil
    @keepalive = "on"
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

        if recieved_messages.length == messagens_to_publish
          assert_equal(body_prefix + messagens_to_publish.to_s, recieved_messages.last, "Didn't receive all messages")
          EventMachine.stop
        end
      }

      EM.add_timer(0.05) do
        0.step(messagens_to_publish - 1, 10) do |i|
          socket = open_socket
          1.upto(10) do |j|
            resp_headers, body = publish_message_in_socket(channel, body_prefix + (i+j).to_s, socket)
            fail("Message was not published: " + body_prefix + (i+j).to_s) unless resp_headers.include?("HTTP/1.1 200 OK")
          end
          socket.close
        end
      end

      add_test_timeout
    }
  end

  def config_test_set_an_event_id_to_the_message_through_header_parameter
    @header_template = nil
    @message_template = '{\"id\": \"~id~\", \"channel\": \"~channel~\", \"text\": \"~text~\", \"event_id\": \"~event-id~\"}'
  end

  def test_set_an_event_id_to_the_message_through_header_parameter
    event_id = 'event_id_with_generic_text_01'
    headers = {'accept' => 'text/html', 'Event-Id' => event_id }
    body = 'test message'
    channel = 'ch_test_set_an_event_id_to_the_message_through_header_parameter'
    response = ''

    EventMachine.run {
      sub = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get
      sub.stream { | chunk |
        response = JSON.parse(chunk)
        assert_equal(1, response["id"].to_i, "Wrong data received")
        assert_equal(channel, response["channel"], "Wrong data received")
        assert_equal(body, response["text"], "Wrong data received")
        assert_equal(event_id, response["event_id"], "Wrong data received")
        EventMachine.stop
      }

      pub = EventMachine::HttpRequest.new(nginx_address + '/pub?id=' + channel.to_s ).post :head => headers, :body => body, :timeout => 30

      add_test_timeout
    }
  end

  def config_test_set_an_event_type_to_the_message_through_header_parameter
    @header_template = nil
    @message_template = '{\"id\": \"~id~\", \"channel\": \"~channel~\", \"text\": \"~text~\", \"event_type\": \"~event-type~\"}'
  end

  def test_set_an_event_type_to_the_message_through_header_parameter
    event_type = 'event_type_with_generic_text_01'
    headers = {'accept' => 'text/html', 'Event-type' => event_type }
    body = 'test message'
    channel = 'ch_test_set_an_event_type_to_the_message_through_header_parameter'
    response = ''

    EventMachine.run {
      sub = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get
      sub.stream { | chunk |
        response = JSON.parse(chunk)
        assert_equal(1, response["id"].to_i, "Wrong data received")
        assert_equal(channel, response["channel"], "Wrong data received")
        assert_equal(body, response["text"], "Wrong data received")
        assert_equal(event_type, response["event_type"], "Wrong data received")
        EventMachine.stop
      }

      pub = EventMachine::HttpRequest.new(nginx_address + '/pub?id=' + channel.to_s ).post :head => headers, :body => body, :timeout => 30

      add_test_timeout
    }
  end

  def config_test_ignore_event_id_header_parameter_with_not_match_exactly
    @header_template = nil
    @message_template = '{\"id\": \"~id~\", \"channel\": \"~channel~\", \"text\": \"~text~\", \"event_id\": \"~event-id~\"}'
  end

  def test_ignore_event_id_header_parameter_with_not_match_exactly
    event_id = 'event_id_with_generic_text_01'
    headers = {'accept' => 'text/html'}
    body = 'test message'
    channel = 'ch_test_set_an_event_id_to_the_message_through_header_parameter'
    response = ''

    EventMachine.run {
      sub = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get
      sub.stream { | chunk |
        response = JSON.parse(chunk)
        assert_equal(1, response["id"].to_i, "Wrong data received")
        assert_equal(channel, response["channel"], "Wrong data received")
        assert_equal(body, response["text"], "Wrong data received")
        assert_equal("", response["event_id"], "Wrong data received")
        EventMachine.stop
      }

      pub = EventMachine::HttpRequest.new(nginx_address + '/pub?id=' + channel.to_s ).post :head => headers.merge('Event-Ids' => event_id), :body => body, :timeout => 30

      add_test_timeout
    }

    EventMachine.run {
      sub = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get
      sub.stream { | chunk |
        response = JSON.parse(chunk)
        assert_equal(2, response["id"].to_i, "Wrong data received")
        assert_equal(channel, response["channel"], "Wrong data received")
        assert_equal(body, response["text"], "Wrong data received")
        assert_equal("", response["event_id"], "Wrong data received")
        EventMachine.stop
      }

      pub = EventMachine::HttpRequest.new(nginx_address + '/pub?id=' + channel.to_s ).post :head => headers.merge('Event-I' => event_id), :body => body, :timeout => 30

      add_test_timeout
    }
  end

  def config_test_expose_message_publish_time_through_message_template
    @header_template = nil
    @message_template = '{\"id\": \"~id~\", \"channel\": \"~channel~\", \"text\": \"~text~\", \"publish_time\": \"~time~\"}'
  end

  def test_expose_message_publish_time_through_message_template
    headers = {'accept' => 'text/html'}
    body = 'test message'
    channel = 'ch_test_expose_message_publish_time_through_message_template'
    response = ''

    now = nil

    EventMachine.run {
      sub = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get
      sub.stream { | chunk |
        response = JSON.parse(chunk)
        assert_equal(1, response["id"].to_i, "Wrong data received")
        assert_equal(channel, response["channel"], "Wrong data received")
        assert_equal(body, response["text"], "Wrong data received")
        assert_equal(29, response["publish_time"].size, "Wrong data received")
        publish_time = Time.parse(response["publish_time"])
        assert_equal(now.to_i, publish_time.to_i, "Didn't receive the correct publish time")

        EventMachine.stop
      }

      now = Time.now
      pub = EventMachine::HttpRequest.new(nginx_address + '/pub?id=' + channel.to_s ).post :head => headers, :body => body, :timeout => 30

      add_test_timeout
    }
  end

  def config_test_expose_message_tag_through_message_template
    @header_template = nil
    @message_template = '{\"id\": \"~id~\", \"channel\": \"~channel~\", \"text\": \"~text~\", \"tag\": \"~tag~\"}'
  end

  def test_expose_message_tag_through_message_template
    headers = {'accept' => 'text/html'}
    body = 'test message'
    channel = 'ch_test_expose_message_tag_through_message_template'
    response = ''

    EventMachine.run {
      sub = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get
      sub.stream { | chunk |
        response += chunk
        lines = response.split('\r\n')
        if lines.size > 1
          lines.each_with_index do |line, i|
            resp = JSON.parse(line)
            assert_equal(i + 1, resp["id"].to_i, "Wrong data received")
            assert_equal(channel, resp["channel"], "Wrong data received")
            assert_equal(body, resp["text"], "Wrong data received")
            assert_equal(i, resp["tag"].to_i, "Wrong data received")
          end
        end

        EventMachine.stop
      }

      pub = EventMachine::HttpRequest.new(nginx_address + '/pub?id=' + channel.to_s ).post :head => headers, :body => body, :timeout => 30
      pub = EventMachine::HttpRequest.new(nginx_address + '/pub?id=' + channel.to_s ).post :head => headers, :body => body, :timeout => 30

      add_test_timeout
    }
  end
end
