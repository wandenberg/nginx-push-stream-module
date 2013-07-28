require 'spec_helper'

describe "Subscriber Event Source" do
  let(:config) do
    {
      :subscriber_mode => 'eventsource',
      :header_template => nil,
      :message_template => nil,
      :footer_template => nil,
      :ping_message_interval => nil
    }
  end

  it "should use content type as 'event stream'" do
    channel = 'ch_test_content_type_should_be_event_stream'

    nginx_run_server(config.merge(:header_template => "header")) do |conf|
      EventMachine.run do
        sub = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get
        sub.stream do |chunk|
          sub.response_header["CONTENT_TYPE"].should eql("text/event-stream; charset=utf-8")
          EventMachine.stop
        end
      end
    end
  end

  it "should split header lines and prefix them by a colon" do
    channel = 'ch_test_each_line_on_header_template_should_be_prefixed_by_a_colon'

    nginx_run_server(config.merge(:header_template => "header line 1\nheader line 2\rheader line 3\r\nheader line 4")) do |conf|
      EventMachine.run do
        sub = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get
        sub.stream do |chunk|
          chunk.should eql(": header line 1\r\n: header line 2\r\n: header line 3\r\n: header line 4\r\n\r\n")
          EventMachine.stop
        end
      end
    end
  end

  it "should treat escaped new lines on header as single lines" do
    channel = 'ch_test_escaped_new_lines_on_header_template_should_be_treated_as_single_line'

    nginx_run_server(config.merge(:header_template => "header line 1\\\\nheader line 2")) do |conf|
      EventMachine.run do
        sub = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get
        sub.stream do |chunk|
          chunk.should eql(": header line 1\\nheader line 2\r\n\r\n")
          EventMachine.stop
        end
      end
    end
  end

  it "should split footer lines and prefix them by a colon" do
    channel = 'ch_test_each_line_on_footer_template_should_be_prefixed_by_a_colon'
    response = ''

    nginx_run_server(config.merge(:subscriber_connection_ttl => '1s', :footer_template => "footer line 1\nfooter line 2\rfooter line 3\r\nfooter line 4")) do |conf|
      EventMachine.run do
        sub = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get
        sub.stream do |chunk|
          response += chunk
        end
        sub.callback do
          response.should eql(": footer line 1\r\n: footer line 2\r\n: footer line 3\r\n: footer line 4\r\n\r\n")
          EventMachine.stop
        end
      end
    end
  end

  it "should treat escaped new lines on footer as single lines" do
    channel = 'ch_test_escaped_new_lines_on_footer_template_should_be_treated_as_single_line'
    response = ''

    nginx_run_server(config.merge(:subscriber_connection_ttl => '1s', :footer_template => "footer line 1\\\\nfooter line 2")) do |conf|
      EventMachine.run do
        sub = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get
        sub.stream do |chunk|
          response += chunk
        end
        sub.callback do
          response.should eql(": footer line 1\\nfooter line 2\r\n\r\n")
          EventMachine.stop
        end
      end
    end
  end

  it "should use default message template without event id" do
    body = 'test message'
    channel = 'ch_test_default_message_template_without_event_id'
    response = ''

    nginx_run_server(config) do |conf|
      EventMachine.run do
        sub = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get
        sub.stream do |chunk|
          response += chunk
          if response.include?("\r\n\r\n")
            response.should eql("data: #{body}\r\n\r\n")
            EventMachine.stop
          end
        end

        publish_message_inline(channel, headers, body)
      end
    end
  end

  it "should use default message template without event type" do
    body = 'test message'
    channel = 'ch_test_default_message_template_without_event_type'
    response = ''

    nginx_run_server(config) do |conf|
      EventMachine.run do
        sub = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get
        sub.stream do |chunk|
          response += chunk
          if response.include?("\r\n\r\n")
            response.should eql("data: #{body}\r\n\r\n")
            EventMachine.stop
          end
        end

        publish_message_inline(channel, headers, body)
      end
    end
  end

  it "should use default message template with event id" do
    event_id = 'event_id_with_generic_text_01'
    body = 'test message'
    channel = 'ch_test_default_message_template_with_event_id'
    response = ''

    nginx_run_server(config) do |conf|
      EventMachine.run do
        sub = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get
        sub.stream do |chunk|
          response += chunk
          if response.include?("\r\n\r\n")
            response.should eql("id: #{event_id}\r\ndata: #{body}\r\n\r\n")
            EventMachine.stop
          end
        end

        publish_message_inline(channel, headers.merge('Event-Id' => event_id), body)
      end
    end
  end

  it "should use default message template with event type" do
    event_type = 'event_type_with_generic_text_01'
    body = 'test message'
    channel = 'ch_test_default_message_template_with_event_type'
    response = ''

    nginx_run_server(config) do |conf|
      EventMachine.run do
        sub = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get
        sub.stream do |chunk|
          response += chunk
          if response.include?("\r\n\r\n")
            response.should eql("event: #{event_type}\r\ndata: #{body}\r\n\r\n")
            EventMachine.stop
          end
        end

        publish_message_inline(channel, headers.merge('Event-type' => event_type), body)
      end
    end
  end

  it "should use custom message template without event id" do
    body = 'test message'
    channel = 'ch_test_custom_message_template_without_event_id'
    response = ''

    nginx_run_server(config.merge(:message_template => '{\"id\":\"~id~\", \"message\":\"~text~\"}')) do |conf|
      EventMachine.run do
        sub = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get
        sub.stream do |chunk|
          response += chunk
          if response.include?("\r\n\r\n")
            response.should eql(%(data: {"id":"1", "message":"#{body}"}\r\n\r\n))
            EventMachine.stop
          end
        end

        publish_message_inline(channel, headers, body)
      end
    end
  end

  it "should use custom message template without event type" do
    body = 'test message'
    channel = 'ch_test_custom_message_template_without_event_type'
    response = ''

    nginx_run_server(config.merge(:message_template => '{\"id\":\"~id~\", \"message\":\"~text~\"}')) do |conf|
      EventMachine.run do
        sub = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get
        sub.stream do |chunk|
          response += chunk
          if response.include?("\r\n\r\n")
            response.should eql(%(data: {"id":"1", "message":"#{body}"}\r\n\r\n))
            EventMachine.stop
          end
        end

        publish_message_inline(channel, headers, body)
      end
    end
  end

  it "should use custom message template with event id" do
    event_id = 'event_id_with_generic_text_01'
    body = 'test message'
    channel = 'ch_test_custom_message_template_with_event_id'
    response = ''

    nginx_run_server(config.merge(:message_template => '{\"id\":\"~id~\", \"message\":\"~text~\"}')) do |conf|
      EventMachine.run do
        sub = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get
        sub.stream do |chunk|
          response += chunk
          if response.include?("\r\n\r\n")
            response.should eql(%(id: #{event_id}\r\ndata: {"id":"1", "message":"#{body}"}\r\n\r\n))
            EventMachine.stop
          end
        end

        publish_message_inline(channel, headers.merge('Event-Id' => event_id), body)
      end
    end
  end

  it "should use custom message template with event type" do
    event_type = 'event_type_with_generic_text_01'
    body = 'test message'
    channel = 'ch_test_custom_message_template_with_event_type'
    response = ''

    nginx_run_server(config.merge(:message_template => '{\"id\":\"~id~\", \"message\":\"~text~\"}')) do |conf|
      EventMachine.run do
        sub = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get
        sub.stream do |chunk|
          response += chunk
          if response.include?("\r\n\r\n")
            response.should eql(%(event: #{event_type}\r\ndata: {"id":"1", "message":"#{body}"}\r\n\r\n))
            EventMachine.stop
          end
        end

        publish_message_inline(channel, headers.merge('Event-type' => event_type), body)
      end
    end
  end

  it "should apply the message template to each line on posted message" do
    body = "line 1\nline 2\rline 3\r\nline 4"
    channel = 'ch_test_each_line_on_posted_message_should_be_applied_to_template'

    nginx_run_server(config) do |conf|
      EventMachine.run do
        sub = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get
        sub.stream do |chunk|
          if chunk.include?("line 4")
            chunk.should eql("data: line 1\r\ndata: line 2\r\ndata: line 3\r\ndata: line 4\r\n\r\n")
            EventMachine.stop
          end
        end

        publish_message_inline(channel, headers, body)
      end
    end
  end

  it "should treat escaped new lines on posted message as single lines" do
    body = "line 1\\nline 2"
    channel = 'ch_test_escaped_new_lines_on_posted_message_should_be_treated_as_single_line'

    nginx_run_server(config) do |conf|
      EventMachine.run do
        sub = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get
        sub.stream do |chunk|
          if chunk.include?("line 2")
            chunk.should eql("data: line 1\\nline 2\r\n\r\n")
            EventMachine.stop
          end
        end

        publish_message_inline(channel, headers, body)
      end
    end
  end

  it "should receive ping message" do
    channel = 'ch_test_ping_message_on_event_source'

    nginx_run_server(config.merge(:ping_message_interval => '1s', :message_template => '{\"id\":\"~id~\", \"message\":\"~text~\"}')) do |conf|
      EventMachine.run do
        sub = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get
        sub.stream do |chunk|
          if chunk.include?("-1")
            chunk.should eql(": -1\r\n")
            EventMachine.stop
          end
        end
      end
    end
  end

  it "should get old messages by last event id" do
    channel = 'ch_test_get_old_messages_by_last_event_id'
    response = ''

    nginx_run_server(config) do |conf|
      EventMachine.run do
        publish_message_inline(channel, headers.merge({'Event-Id' => 'event 1'}), 'msg 1')
        publish_message_inline(channel, headers.merge({'Event-Id' => 'event 2'}), 'msg 2')
        publish_message_inline(channel, headers, 'msg 3')
        publish_message_inline(channel, headers.merge({'Event-Id' => 'event 3'}), 'msg 4')

        sub = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => {'Last-Event-Id' => 'event 2' }
        sub.stream do |chunk|
          response += chunk
          if response.include?("msg 4")
            response.should eql("data: msg 3\r\n\r\nid: event 3\r\ndata: msg 4\r\n\r\n")
            EventMachine.stop
          end
        end
      end
    end
  end

  it "should get old messages by last event id without found an event" do
    channel = 'ch_test_get_old_messages_by_last_event_id_without_found_event'
    response = ''

    nginx_run_server(config.merge(:ping_message_interval => '1s')) do |conf|
      EventMachine.run do
        publish_message_inline(channel, headers.merge({'Event-Id' => 'event 1'}), 'msg 1')
        publish_message_inline(channel, headers.merge({'Event-Id' => 'event 2'}), 'msg 2')
        publish_message_inline(channel, headers, 'msg 3')
        publish_message_inline(channel, headers.merge({'Event-Id' => 'event 3'}), 'msg 4')

        sub = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => {'Last-Event-Id' => 'event_not_found' }
        sub.stream do |chunk|
          if chunk.include?("-1")
            chunk.should eql(": -1\r\n")
            EventMachine.stop
          end
        end
      end
    end
  end
end
