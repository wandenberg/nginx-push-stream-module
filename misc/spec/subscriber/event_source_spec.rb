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
        source = EventMachine::EventSource.new(nginx_address + '/sub/' + channel.to_s)
        source.open do
          EventMachine.stop
        end

        sub = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get
        sub.stream do |chunk|
          expect(sub.response_header["CONTENT_TYPE"]).to eql("text/event-stream; charset=utf-8")
          source.start
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
          expect(chunk).to eql(": header line 1\n: header line 2\n: header line 3\n: header line 4\n")
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
          expect(chunk).to eql(": header line 1\\nheader line 2\n")
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
          expect(response).to eql(": \n: footer line 1\n: footer line 2\n: footer line 3\n: footer line 4\n")
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
          expect(response).to eql(": \n: footer line 1\\nfooter line 2\n")
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
        source = EventMachine::EventSource.new(nginx_address + '/sub/_' + channel.to_s)
        source.message do |message|
          expect(message).to eql(body)
          publish_message_inline(channel, headers, body)
        end
        source.start

        sub = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get
        sub.stream do |chunk|
          response += chunk
          if response.include?("data: ")
            expect(response).to eql(": \ndata: #{body}\n\n")
            EventMachine.stop
          end
        end

        publish_message_inline("_#{channel}", headers, body)
      end
    end
  end

  it "should use default message template without event type" do
    body = 'test message'
    channel = 'ch_test_default_message_template_without_event_type'
    response = ''

    nginx_run_server(config) do |conf|
      EventMachine.run do
        source = EventMachine::EventSource.new(nginx_address + '/sub/_' + channel.to_s)
        source.message do |message|
          expect(message).to eql(body)
          publish_message_inline(channel, headers, body)
        end
        source.start

        sub = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get
        sub.stream do |chunk|
          response += chunk
          if response.include?("data: ")
            expect(response).to eql(": \ndata: #{body}\n\n")
            EventMachine.stop
          end
        end

        publish_message_inline("_#{channel}", headers, body)
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
        source = EventMachine::EventSource.new(nginx_address + '/sub/_' + channel.to_s)
        source.message do |message|
          expect(message).to eql(body)
          expect(source.last_event_id).to eql(event_id)
          publish_message_inline(channel, headers.merge('Event-Id' => event_id), body)
        end
        source.start

        sub = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get
        sub.stream do |chunk|
          response += chunk
          if response.include?("data: ")
            expect(response).to eql(": \nid: #{event_id}\ndata: #{body}\n\n")
            EventMachine.stop
          end
        end

        publish_message_inline("_#{channel}", headers.merge('Event-Id' => event_id), body)
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
        source = EventMachine::EventSource.new(nginx_address + '/sub/_' + channel.to_s)
        source.on event_type do |message|
          expect(message).to eql(body)
          publish_message_inline(channel, headers.merge('Event-type' => event_type), body)
        end
        source.start

        sub = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get
        sub.stream do |chunk|
          response += chunk
          if response.include?("data: ")
            expect(response).to eql(": \nevent: #{event_type}\ndata: #{body}\n\n")
            EventMachine.stop
          end
        end

        publish_message_inline("_#{channel}", headers.merge('Event-type' => event_type), body)
      end
    end
  end

  it "should use custom message template without event id" do
    body = 'test message'
    channel = 'ch_test_custom_message_template_without_event_id'
    response = ''

    nginx_run_server(config.merge(:message_template => '{\"id\":\"~id~\", \"message\":\"~text~\"}')) do |conf|
      EventMachine.run do
        source = EventMachine::EventSource.new(nginx_address + '/sub/_' + channel.to_s)
        source.message do |message|
          expect(message).to eql(%({"id":"1", "message":"#{body}"}))
          publish_message_inline(channel, headers, body)
        end
        source.start

        sub = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get
        sub.stream do |chunk|
          response += chunk
          if response.include?("data: ")
            expect(response).to eql(%(: \ndata: {"id":"1", "message":"#{body}"}\n\n))
            EventMachine.stop
          end
        end

        publish_message_inline("_#{channel}", headers, body)
      end
    end
  end

  it "should use custom message template without event type" do
    body = 'test message'
    channel = 'ch_test_custom_message_template_without_event_type'
    response = ''

    nginx_run_server(config.merge(:message_template => '{\"id\":\"~id~\", \"message\":\"~text~\"}')) do |conf|
      EventMachine.run do
        source = EventMachine::EventSource.new(nginx_address + '/sub/_' + channel.to_s)
        source.message do |message|
          expect(message).to eql(%({"id":"1", "message":"#{body}"}))
          publish_message_inline(channel, headers, body)
        end
        source.start

        sub = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get
        sub.stream do |chunk|
          response += chunk
          if response.include?("data: ")
            expect(response).to eql(%(: \ndata: {"id":"1", "message":"#{body}"}\n\n))
            EventMachine.stop
          end
        end

        publish_message_inline("_#{channel}", headers, body)
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
        source = EventMachine::EventSource.new(nginx_address + '/sub/_' + channel.to_s)
        source.message do |message|
          expect(message).to eql(%({"id":"1", "message":"#{body}"}))
          expect(source.last_event_id).to eql(event_id)
          publish_message_inline(channel, headers.merge('Event-Id' => event_id), body)
        end
        source.start

        sub = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get
        sub.stream do |chunk|
          response += chunk
          if response.include?("data: ")
            expect(response).to eql(%(: \nid: #{event_id}\ndata: {"id":"1", "message":"#{body}"}\n\n))
            EventMachine.stop
          end
        end

        publish_message_inline("_#{channel}", headers.merge('Event-Id' => event_id), body)
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
        source = EventMachine::EventSource.new(nginx_address + '/sub/_' + channel.to_s)
        source.on event_type do |message|
          expect(message).to eql(%({"id":"1", "message":"#{body}"}))
          publish_message_inline(channel, headers.merge('Event-type' => event_type), body)
        end
        source.start

        sub = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get
        sub.stream do |chunk|
          response += chunk
          if response.include?("data: ")
            expect(response).to eql(%(: \nevent: #{event_type}\ndata: {"id":"1", "message":"#{body}"}\n\n))
            EventMachine.stop
          end
        end

        publish_message_inline("_#{channel}", headers.merge('Event-type' => event_type), body)
      end
    end
  end

  it "should apply the message template to each line on posted message" do
    body = "line 1\nline 2\rline 3\r\nline 4"
    channel = 'ch_test_each_line_on_posted_message_should_be_applied_to_template'

    nginx_run_server(config) do |conf|
      EventMachine.run do
        source = EventMachine::EventSource.new(nginx_address + '/sub/_' + channel.to_s)
        source.message do |message|
          expect(message).to eql("line 1\nline 2\nline 3\nline 4")
          publish_message_inline(channel, headers, body)
        end
        source.start

        sub = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get
        sub.stream do |chunk|
          if chunk.include?("line 4")
            expect(chunk).to eql("data: line 1\ndata: line 2\ndata: line 3\ndata: line 4\n\n")
            EventMachine.stop
          end
        end

        publish_message_inline("_#{channel}", headers, body)
      end
    end
  end

  it "should treat escaped new lines on posted message as single lines" do
    body = "line 1\\nline 2"
    channel = 'ch_test_escaped_new_lines_on_posted_message_should_be_treated_as_single_line'

    nginx_run_server(config) do |conf|
      EventMachine.run do
        source = EventMachine::EventSource.new(nginx_address + '/sub/_' + channel.to_s)
        source.message do |message|
          expect(message).to eql(body)
          publish_message_inline(channel, headers, body)
        end
        source.start

        sub = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get
        sub.stream do |chunk|
          if chunk.include?("line 2")
            expect(chunk).to eql("data: line 1\\nline 2\n\n")
            EventMachine.stop
          end
        end

        publish_message_inline("_#{channel}", headers, body)
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
            expect(chunk).to eql(": -1\n")
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
            expect(response).to eql(": header\ndata: msg #{body}\n\n: footer\n")

            response = ''
            sub_1 = EventMachine::HttpRequest.new(nginx_address + '/ev/' + channel.to_s + '?tests=on').get
            sub_1.stream do |chunk_1|
              response += chunk_1
              if response.include?("footer")
                expect(response).to eql(": header\ndata: msg #{body}\n\n: footer\n")
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
