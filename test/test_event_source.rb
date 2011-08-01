require File.expand_path('base_test_case', File.dirname(__FILE__))

class TestEventSource < Test::Unit::TestCase
  include BaseTestCase

  def global_configuration
    @subscriber_eventsource = 'on'
    @header_template = nil
    @message_template = nil
    @ping_message_interval = nil
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

      publish_message_inline(channel, headers, body)

      add_test_timeout
    }
  end

  def test_default_message_template_with_event_id
    event_id = 'event_id_with_generic_text_01'
    headers = {'accept' => 'text/html', 'Event-Id' => event_id }
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

      publish_message_inline(channel, headers, body)

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

      publish_message_inline(channel, headers, body)

      add_test_timeout
    }
  end

  def config_test_custom_message_template_with_event_id
    @message_template = '{\"id\":\"~id~\", \"message\":\"~text~\"}'
  end

  def test_custom_message_template_with_event_id
    event_id = 'event_id_with_generic_text_01'
    headers = {'accept' => 'text/html', 'Event-Id' => event_id }
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

      publish_message_inline(channel, headers, body)

      add_test_timeout
    }
  end

  def config_test_ping_message_on_event_source
    @ping_message_interval = '1s'
    @message_template = '{\"id\":\"~id~\", \"message\":\"~text~\"}'
  end

  def test_ping_message_on_event_source
    headers = {'accept' => 'text/html'}
    channel = 'ch_test_ping_message_on_event_source'

    EventMachine.run {
      sub = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get
      sub.stream { | chunk |
        assert_equal(": -1\r\n", chunk, "Wrong ping message")
        EventMachine.stop
      }

      add_test_timeout
    }
  end

  def test_get_old_messages_by_last_event_id
    channel = 'ch_test_get_old_messages_by_last_event_id'

    response = ''

    EventMachine.run {
      publish_message_inline(channel, {'accept' => 'text/html', 'Event-Id' => 'event 1' }, 'msg 1')
      publish_message_inline(channel, {'accept' => 'text/html', 'Event-Id' => 'event 2' }, 'msg 2')
      publish_message_inline(channel, {'accept' => 'text/html' }, 'msg 3')
      publish_message_inline(channel, {'accept' => 'text/html', 'Event-Id' => 'event 3' }, 'msg 4')

      sub = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => {'Last-Event-Id' => 'event 2' }
      sub.stream { | chunk |
        response += chunk
        if response.include?("msg 4")
          assert_equal("data: msg 3\r\n\r\nid: event 3\r\ndata: msg 4\r\n\r\n", response, "The published message was not received correctly")
          EventMachine.stop
        end
      }

      add_test_timeout
    }
  end

  def config_test_get_old_messages_by_last_event_id_without_found_event
    @ping_message_interval = '1s'
  end

  def test_get_old_messages_by_last_event_id_without_found_event
    channel = 'ch_test_get_old_messages_by_last_event_id_without_found_event'

    response = ''

    EventMachine.run {
      publish_message_inline(channel, {'accept' => 'text/html', 'Event-Id' => 'event 1' }, 'msg 1')
      publish_message_inline(channel, {'accept' => 'text/html', 'Event-Id' => 'event 2' }, 'msg 2')
      publish_message_inline(channel, {'accept' => 'text/html' }, 'msg 3')
      publish_message_inline(channel, {'accept' => 'text/html', 'Event-Id' => 'event 3' }, 'msg 4')

      sub = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => {'Last-Event-Id' => 'event_not_found' }
      sub.stream { | chunk |
        assert_equal(": -1\r\n", chunk, "Received any other message instead of ping")
        EventMachine.stop
      }

      add_test_timeout
    }
  end


end
