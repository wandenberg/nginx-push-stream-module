require File.expand_path('base_test_case', File.dirname(__FILE__))

class TestSubscriberEventSource < Test::Unit::TestCase
  include BaseTestCase

  def global_configuration
    @subscriber_eventsource = 'on'
    @header_template = nil
    @message_template = nil
    @footer_template = nil
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

  def config_test_each_line_on_header_template_should_be_prefixed_by_a_colon
    @header_template = "header line 1\nheader line 2\rheader line 3\r\nheader line 4"
  end

  def test_each_line_on_header_template_should_be_prefixed_by_a_colon
    channel = 'ch_test_each_line_on_header_template_should_be_prefixed_by_a_colon'
    EventMachine.run {
      sub = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get
      sub.stream { | chunk |
        assert_equal(": header line 1\r\n: header line 2\r\n: header line 3\r\n: header line 4\r\n\r\n", chunk, "Wrong header")
        EventMachine.stop
      }

      add_test_timeout
    }
  end

  def config_test_escaped_new_lines_on_header_template_should_be_treated_as_single_line
    @header_template = "header line 1\\\\nheader line 2"
  end

  def test_escaped_new_lines_on_header_template_should_be_treated_as_single_line
    channel = 'ch_test_escaped_new_lines_on_header_template_should_be_treated_as_single_line'
    EventMachine.run {
      sub = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get
      sub.stream { | chunk |
        assert_equal(": header line 1\\nheader line 2\r\n\r\n", chunk, "Wrong header")
        EventMachine.stop
      }

      add_test_timeout
    }
  end

  def config_test_each_line_on_footer_template_should_be_prefixed_by_a_colon
    @footer_template = "footer line 1\nfooter line 2\rfooter line 3\r\nfooter line 4"
    @subscriber_connection_timeout = '1s'
  end

  def test_each_line_on_footer_template_should_be_prefixed_by_a_colon
    channel = 'ch_test_each_line_on_footer_template_should_be_prefixed_by_a_colon'
    response = ''
    EventMachine.run {
      sub = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get
      sub.stream { | chunk |
        response += chunk
      }
      sub.callback {
        assert_equal(":\r\n: footer line 1\r\n: footer line 2\r\n: footer line 3\r\n: footer line 4\r\n\r\n", response, "Wrong footer")
        EventMachine.stop
      }

      add_test_timeout
    }
  end

  def config_test_escaped_new_lines_on_footer_template_should_be_treated_as_single_line
    @footer_template = "footer line 1\\\\nfooter line 2"
    @subscriber_connection_timeout = '1s'
  end

  def test_escaped_new_lines_on_footer_template_should_be_treated_as_single_line
    channel = 'ch_test_escaped_new_lines_on_footer_template_should_be_treated_as_single_line'
    response = ''
    EventMachine.run {
      sub = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get
      sub.stream { | chunk |
        response += chunk
      }
      sub.callback {
        assert_equal(":\r\n: footer line 1\\nfooter line 2\r\n\r\n", response, "Wrong footer")
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
          assert_equal(":\r\ndata: #{body}\r\n\r\n", response, "The published message was not received correctly")
          EventMachine.stop
        end
      }

      publish_message_inline(channel, headers, body)

      add_test_timeout
    }
  end

  def test_default_message_template_without_event_type
    headers = {'accept' => 'text/html'}
    body = 'test message'
    channel = 'ch_test_default_message_template_without_event_type'
    response = ''

    EventMachine.run {
      sub = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get
      sub.stream { | chunk |
        response += chunk
        if response.include?("\r\n\r\n")
          assert_equal(":\r\ndata: #{body}\r\n\r\n", response, "The published message was not received correctly")
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
          assert_equal(":\r\nid: #{event_id}\r\ndata: #{body}\r\n\r\n", response, "The published message was not received correctly")
          EventMachine.stop
        end
      }

      publish_message_inline(channel, headers, body)

      add_test_timeout
    }
  end

  def test_default_message_template_with_event_type
    event_type = 'event_type_with_generic_text_01'
    headers = {'accept' => 'text/html', 'Event-type' => event_type }
    body = 'test message'
    channel = 'ch_test_default_message_template_with_event_type'
    response = ''

    EventMachine.run {
      sub = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get
      sub.stream { | chunk |
        response += chunk
        if response.include?("\r\n\r\n")
          assert_equal(":\r\nevent: #{event_type}\r\ndata: #{body}\r\n\r\n", response, "The published message was not received correctly")
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
          assert_equal(%(:\r\ndata: {"id":"1", "message":"#{body}"}\r\n\r\n), response, "The published message was not received correctly")
          EventMachine.stop
        end
      }

      publish_message_inline(channel, headers, body)

      add_test_timeout
    }
  end

  def config_test_custom_message_template_without_event_type
    @message_template = '{\"id\":\"~id~\", \"message\":\"~text~\"}'
  end

  def test_custom_message_template_without_event_type
    headers = {'accept' => 'text/html'}
    body = 'test message'
    channel = 'ch_test_custom_message_template_without_event_type'
    response = ''

    EventMachine.run {
      sub = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get
      sub.stream { | chunk |
        response += chunk
        if response.include?("\r\n\r\n")
          assert_equal(%(:\r\ndata: {"id":"1", "message":"#{body}"}\r\n\r\n), response, "The published message was not received correctly")
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
          assert_equal(%(:\r\nid: #{event_id}\r\ndata: {"id":"1", "message":"#{body}"}\r\n\r\n), response, "The published message was not received correctly")
          EventMachine.stop
        end
      }

      publish_message_inline(channel, headers, body)

      add_test_timeout
    }
  end

  def config_test_custom_message_template_with_event_type
    @message_template = '{\"id\":\"~id~\", \"message\":\"~text~\"}'
  end

  def test_custom_message_template_with_event_type
    event_type = 'event_type_with_generic_text_01'
    headers = {'accept' => 'text/html', 'Event-type' => event_type }
    body = 'test message'
    channel = 'ch_test_custom_message_template_with_event_type'
    response = ''

    EventMachine.run {
      sub = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get
      sub.stream { | chunk |
        response += chunk
        if response.include?("\r\n\r\n")
          assert_equal(%(:\r\nevent: #{event_type}\r\ndata: {"id":"1", "message":"#{body}"}\r\n\r\n), response, "The published message was not received correctly")
          EventMachine.stop
        end
      }

      publish_message_inline(channel, headers, body)

      add_test_timeout
    }
  end

  def test_each_line_on_posted_message_should_be_applied_to_template
    headers = {'accept' => 'text/html'}
    body = "line 1\nline 2\rline 3\r\nline 4"
    channel = 'ch_test_each_line_on_posted_message_should_be_applied_to_template'

    EventMachine.run {
      sub = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get
      sub.stream { | chunk |
        if chunk.include?("line 4")
          assert_equal("data: line 1\r\ndata: line 2\r\ndata: line 3\r\ndata: line 4\r\n\r\n", chunk, "Wrong data message")
          EventMachine.stop
        end
      }


      publish_message_inline(channel, headers, body)

      add_test_timeout
    }
  end

  def test_escaped_new_lines_on_posted_message_should_be_treated_as_single_line
    headers = {'accept' => 'text/html'}
    body = "line 1\\nline 2"
    channel = 'ch_test_escaped_new_lines_on_posted_message_should_be_treated_as_single_line'

    EventMachine.run {
      sub = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get
      sub.stream { | chunk |
        if chunk.include?("line 2")
          assert_equal("data: line 1\\nline 2\r\n\r\n", chunk, "Wrong data message")
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
        if chunk.include?("-1")
          assert_equal(": -1\r\n", chunk, "Wrong ping message")
          EventMachine.stop
        end
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
          assert_equal(":\r\ndata: msg 3\r\n\r\nid: event 3\r\ndata: msg 4\r\n\r\n", response, "The published message was not received correctly")
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
        if chunk.include?("-1")
          assert_equal(": -1\r\n", chunk, "Received any other message instead of ping")
          EventMachine.stop
        end
      }

      add_test_timeout
    }
  end

end
