require File.expand_path('base_test_case', File.dirname(__FILE__))

class TestEventSource < Test::Unit::TestCase
  include BaseTestCase

  def global_configuration
    @subscriber_eventsource = 'on'
    @header_template = nil
    @message_template = nil
  end

  def config_test_content_type_should_be_event_stream
    @header_template = "header"
  end

  def test_content_type_should_be_event_stream
    channel = 'ch_test_content_type_should_be_event_stream'
    EventMachine.run {
      sub = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get
      sub.stream { | chunk |
        assert_equal("text/event-stream; charset=utf-8", sub.response_header["CONTENT_TYPE"], "wrong content-type")
        EventMachine.stop
      }

      add_test_timeout
    }
  end

  def test_default_message_template_without_event_id
    headers = {'accept' => 'text/html'}
    body = 'test message'
    channel = 'ch_test_default_message_template_without_event_id'
    response = ''

    EventMachine.run {
      sub = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get
      sub.stream { | chunk |
        response += chunk
        if response.include?("\r\n\r\n")
          assert_equal("data: #{body}\r\n\r\n", response, "The published message was not received correctly")
          EventMachine.stop
        end
      }

      pub = EventMachine::HttpRequest.new(nginx_address + '/pub?id=' + channel.to_s ).post :head => headers, :body => body, :timeout => 30

      add_test_timeout
    }
  end

  def test_default_message_template_with_event_id
    event_id = 'event_id_with_generic_text_01'
    headers = {'accept' => 'text/html', 'Event-iD' => event_id }
    body = 'test message'
    channel = 'ch_test_default_message_template_with_event_id'
    response = ''

    EventMachine.run {
      sub = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get
      sub.stream { | chunk |
        response += chunk
        if response.include?("\r\n\r\n")
          assert_equal("id: #{event_id}\r\ndata: #{body}\r\n\r\n", response, "The published message was not received correctly")
          EventMachine.stop
        end
      }

      pub = EventMachine::HttpRequest.new(nginx_address + '/pub?id=' + channel.to_s ).post :head => headers, :body => body, :timeout => 30

      add_test_timeout
    }
  end

  def config_test_custom_message_template_without_event_id
    @message_template = '{\"id\":\"~id~\", \"message\":\"~text~\"}'
  end

  def test_custom_message_template_without_event_id
    headers = {'accept' => 'text/html'}
    body = 'test message'
    channel = 'ch_test_custom_message_template_without_event_id'
    response = ''

    EventMachine.run {
      sub = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get
      sub.stream { | chunk |
        response += chunk
        if response.include?("\r\n\r\n")
          assert_equal(%(data: {"id":"1", "message":"#{body}"}\r\n\r\n), response, "The published message was not received correctly")
          EventMachine.stop
        end
      }

      pub = EventMachine::HttpRequest.new(nginx_address + '/pub?id=' + channel.to_s ).post :head => headers, :body => body, :timeout => 30

      add_test_timeout
    }
  end

  def config_test_custom_message_template_with_event_id
    @message_template = '{\"id\":\"~id~\", \"message\":\"~text~\"}'
  end

  def test_custom_message_template_with_event_id
    event_id = 'event_id_with_generic_text_01'
    headers = {'accept' => 'text/html', 'Event-iD' => event_id }
    body = 'test message'
    channel = 'ch_test_custom_message_template_with_event_id'
    response = ''

    EventMachine.run {
      sub = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get
      sub.stream { | chunk |
        response += chunk
        if response.include?("\r\n\r\n")
          assert_equal(%(id: #{event_id}\r\ndata: {"id":"1", "message":"#{body}"}\r\n\r\n), response, "The published message was not received correctly")
          EventMachine.stop
        end
      }

      pub = EventMachine::HttpRequest.new(nginx_address + '/pub?id=' + channel.to_s ).post :head => headers, :body => body, :timeout => 30

      add_test_timeout
    }
  end

end
