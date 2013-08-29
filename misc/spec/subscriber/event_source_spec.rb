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
          chunk.should eql(": header line 1\r\n: header line 2\r\n: header line 3\r\n: header line 4\r\n")
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
          chunk.should eql(": header line 1\\nheader line 2\r\n")
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
          response.should eql(": \r\n: footer line 1\r\n: footer line 2\r\n: footer line 3\r\n: footer line 4\r\n")
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
          response.should eql(": \r\n: footer line 1\\nfooter line 2\r\n")
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
          if response.include?("data: ")
            response.should eql(": \r\ndata: #{body}\r\n\r\n")
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
          if response.include?("data: ")
            response.should eql(": \r\ndata: #{body}\r\n\r\n")
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
          if response.include?("data: ")
            response.should eql(": \r\nid: #{event_id}\r\ndata: #{body}\r\n\r\n")
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
          if response.include?("data: ")
            response.should eql(": \r\nevent: #{event_type}\r\ndata: #{body}\r\n\r\n")
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
          if response.include?("data: ")
            response.should eql(%(: \r\ndata: {"id":"1", "message":"#{body}"}\r\n\r\n))
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
          if response.include?("data: ")
            response.should eql(%(: \r\ndata: {"id":"1", "message":"#{body}"}\r\n\r\n))
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
          if response.include?("data: ")
            response.should eql(%(: \r\nid: #{event_id}\r\ndata: {"id":"1", "message":"#{body}"}\r\n\r\n))
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
          if response.include?("data: ")
            response.should eql(%(: \r\nevent: #{event_type}\r\ndata: {"id":"1", "message":"#{body}"}\r\n\r\n))
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

  it "should not reaplly formatter to header, message or footer template when inside an if" do
    channel = 'ch_test_not_reaplly_formatter_on_header_message_footer_template'
    body = 'test message'
    response = ''
    extra_location = %(
      location ~ /ev/(.*) {
        push_stream_subscriber "eventsource";
        push_stream_channels_path "$1";
        if ($arg_tests = "on") {
          push_stream_channels_path "test_$1";
        }
      }
    )

    nginx_run_server(config.merge(:extra_location => extra_location, :header_template => "header", :message_template => "msg ~text~", :footer_template => "footer", :subscriber_connection_ttl => '1s')) do |conf|
      EventMachine.run do
        sub = EventMachine::HttpRequest.new(nginx_address + '/ev/' + channel.to_s).get
        sub.stream do |chunk|
          response += chunk
          if response.include?("footer")
            response.should eql(": header\r\ndata: msg #{body}\r\n\r\n: footer\r\n")

            response = ''
            sub_1 = EventMachine::HttpRequest.new(nginx_address + '/ev/' + channel.to_s + '?tests=on').get
            sub_1.stream do |chunk_1|
              response += chunk_1
              if response.include?("footer")
                response.should eql(": header\r\ndata: msg #{body}\r\n\r\n: footer\r\n")
                EventMachine.stop
              end
            end

            publish_message_inline("test_" + channel, headers, body)
          end
        end

        publish_message_inline(channel, headers, body)
      end
    end
  end
end
