require 'spec_helper'

describe "Subscriber Properties" do

  shared_examples_for "long polling location" do

    it "should disconnect after receive a message" do
      channel = 'ch_test_disconnect_after_receive_a_message_when_longpolling_is_on'
      body = 'body'
      response = ""

      nginx_run_server(config) do |conf|
        EventMachine.run do
          sub_1 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => headers
          sub_1.stream do |chunk|
            response += chunk
          end
          sub_1.callback do |chunk|
            response.should eql("#{body}\r\n")

            sent_headers = headers.merge({'If-Modified-Since' => sub_1.response_header['LAST_MODIFIED'], 'If-None-Match' => sub_1.response_header['ETAG']})
            response = ""
            sub_2 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => sent_headers
            sub_2.stream do |chunk2|
              response += chunk2
            end
            sub_2.callback do
              response.should eql("#{body} 1\r\n")
              EventMachine.stop
            end

            publish_message_inline(channel, {}, body + " 1")
          end

          publish_message_inline(channel, {}, body)
        end
      end
    end

    it "should disconnect after receive old messages by backtrack" do
      channel = 'ch_test_disconnect_after_receive_old_messages_by_backtrack_when_longpolling_is_on'
      response = ""

      nginx_run_server(config) do |conf|
        EventMachine.run do
          publish_message_inline(channel, {}, 'msg 1')
          publish_message_inline(channel, {}, 'msg 2')
          publish_message_inline(channel, {}, 'msg 3')
          publish_message_inline(channel, {}, 'msg 4')

          sub = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s + '.b2').get :head => headers
          sub.stream do |chunk|
            response += chunk
          end
          sub.callback do |chunk|
            response.should eql("msg 3\r\nmsg 4\r\n")

            response = ''
            sub_1 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => headers.merge({'If-Modified-Since' => sub.response_header['LAST_MODIFIED'], 'If-None-Match' => sub.response_header['ETAG']})
            sub_1.stream do |chunk2|
              response += chunk2
            end
            sub_1.callback do
              response.should eql("msg 5\r\n")

              EventMachine.stop
            end

            publish_message_inline(channel, {}, 'msg 5')
          end
        end
      end
    end

    it "should disconnect after receive old messages by 'last_event_id'" do
      channel = 'ch_test_disconnect_after_receive_old_messages_by_last_event_id_when_longpolling_is_on'
      response = ""

      nginx_run_server(config) do |conf|
        EventMachine.run do
          publish_message_inline(channel, {'Event-Id' => 'event 1'}, 'msg 1')
          publish_message_inline(channel, {'Event-Id' => 'event 2'}, 'msg 2')
          publish_message_inline(channel, {}, 'msg 3')
          publish_message_inline(channel, {'Event-Id' => 'event 3'}, 'msg 4')

          sub = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => headers.merge({'Last-Event-Id' => 'event 2'})
          sub.stream do |chunk|
            response += chunk
          end
          sub.callback do |chunk|
            response.should eql("msg 3\r\nmsg 4\r\n")
            EventMachine.stop
          end
        end
      end
    end

    it "should disconnect after receive old messages from different channels" do
      channel_1 = 'ch_test_receive_old_messages_from_different_channels_1'
      channel_2 = 'ch_test_receive_old_messages_from_different_channels_2'
      body = 'body'
      response = ""

      nginx_run_server(config) do |conf|
        EventMachine.run do
          publish_message_inline(channel_1, {}, body + "_1")
          publish_message_inline(channel_2, {}, body + "_2")

          sub_1 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel_2.to_s + '/' + channel_1.to_s).get :head => headers
          sub_1.callback do
            sub_1.should be_http_status(200)
            sub_1.response_header['LAST_MODIFIED'].to_s.should_not eql("")
            sub_1.response_header['ETAG'].to_s.should_not eql("")
            sub_1.response.should eql("#{body}_2\r\n#{body}_1\r\n")

            sent_headers = headers.merge({'If-Modified-Since' => sub_1.response_header['LAST_MODIFIED'], 'If-None-Match' => sub_1.response_header['ETAG']})
            sub_2 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel_2.to_s + '/' + channel_1.to_s).get :head => sent_headers
            sub_2.callback do
              sub_2.should be_http_status(200)
              sub_2.response_header['LAST_MODIFIED'].to_s.should_not eql(sub_1.response_header['LAST_MODIFIED'])
              sub_2.response_header['ETAG'].to_s.should eql("0")
              sub_2.response.should eql("#{body}1_1\r\n")

              sent_headers = headers.merge({'If-Modified-Since' => sub_2.response_header['LAST_MODIFIED'], 'If-None-Match' => sub_2.response_header['ETAG']})
              sub_3 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel_2.to_s + '/' + channel_1.to_s).get :head => sent_headers
              sub_3.callback do
                sub_3.should be_http_status(200)
                sub_3.response_header['LAST_MODIFIED'].to_s.should_not eql(sub_2.response_header['LAST_MODIFIED'])
                sub_3.response_header['ETAG'].to_s.should eql("0")
                sub_3.response.should eql("#{body}1_2\r\n")

                EventMachine.stop
              end

              sleep(1) # to publish the second message in a different second from the first
              publish_message_inline(channel_2, {}, body + "1_2")
            end

            sleep(1) # to publish the second message in a different second from the first
            publish_message_inline(channel_1, {}, body + "1_1")
          end
        end
      end
    end

    it "should disconnect after timeout is reached" do
      channel = 'ch_test_disconnect_long_polling_subscriber_when_longpolling_timeout_is_set'

      start = Time.now
      nginx_run_server(config.merge(:subscriber_connection_ttl => "10s"), :timeout => 30) do |conf|
        EventMachine.run do
          sub = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => headers
          sub.callback do
            stop = Time.now
            time_diff_sec(start, stop).should be_in_the_interval(10, 10.5)
            sub.should be_http_status(304).without_body
            Time.parse(sub.response_header['LAST_MODIFIED'].to_s).utc.to_i.should be_in_the_interval(Time.now.utc.to_i-1, Time.now.utc.to_i)
            sub.response_header['ETAG'].to_s.should eql("0")
            EventMachine.stop
          end
        end
      end
    end

    it "should overwrite subscriber timeout with long polling timeout" do
      channel = 'ch_test_disconnect_long_polling_subscriber_when_longpolling_timeout_is_set'

      start = Time.now
      nginx_run_server(config.merge(:subscriber_connection_ttl => "10s", :longpolling_connection_ttl => "5s"), :timeout => 10) do |conf|
        EventMachine.run do
          sub = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => headers
          sub.callback do
            stop = Time.now
            time_diff_sec(start, stop).should be_in_the_interval(5, 5.5)
            sub.should be_http_status(304).without_body
            Time.parse(sub.response_header['LAST_MODIFIED'].to_s).utc.to_i.should be_in_the_interval(Time.now.utc.to_i-1, Time.now.utc.to_i)
            sub.response_header['ETAG'].to_s.should eql("0")
            EventMachine.stop
          end
        end
      end
    end

    it "should disconnet after timeout be reached when only long polling timeout is set" do
      channel = 'ch_test_disconnect_long_polling_subscriber_when_only_longpolling_timeout_is_set'

      start = Time.now
      nginx_run_server(config.merge(:subscriber_connection_ttl => nil, :longpolling_connection_ttl => "3s")) do |conf|
        EventMachine.run do
          sub = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => headers
          sub.callback do
            stop = Time.now
            time_diff_sec(start, stop).should be_in_the_interval(3, 3.5)
            sub.should be_http_status(304).without_body
            Time.parse(sub.response_header['LAST_MODIFIED'].to_s).utc.to_i.should be_in_the_interval(Time.now.utc.to_i-1, Time.now.utc.to_i)
            sub.response_header['ETAG'].to_s.should eql("0")
            EventMachine.stop
          end
        end
      end
    end

    it "should not receive ping message" do
      channel = 'ch_test_not_receive_ping_message'

      start = Time.now
      nginx_run_server(config.merge(:subscriber_connection_ttl => "5s", :ping_message_interval => "1s"), :timeout => 10) do |conf|
        EventMachine.run do
          sub = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => headers
          sub.callback do
            stop = Time.now
            time_diff_sec(start, stop).should be_in_the_interval(5, 5.5)
            sub.should be_http_status(304).without_body
            EventMachine.stop
          end
        end
      end
    end

    it "should receive messages with etag greather than recent message" do
      channel = 'ch_test_receiving_messages_with_etag_greather_than_recent_message'
      body_prefix = 'published message '
      messagens_to_publish = 10

      nginx_run_server(config.merge(:store_messages => "on", :message_template => '{\"id\":\"~id~\", \"message\":\"~text~\"}')) do |conf|
        EventMachine.run do
          i = 0
          stored_messages = 0
          EM.add_periodic_timer(0.001) do
            if i < messagens_to_publish
              i += 1
              publish_message_inline(channel.to_s, headers, body_prefix + i.to_s)
            else
            end
          end

          EM.add_timer(1) do
            pub = EventMachine::HttpRequest.new(nginx_address + '/pub?id=' + channel.to_s ).post :head => headers, :body => body_prefix + i.to_s
            pub.callback do
              response = JSON.parse(pub.response)
              stored_messages = response["stored_messages"].to_i
            end
          end

          EM.add_timer(2) do
            sub = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => headers.merge({'If-Modified-Since' => 'Thu, 1 Jan 1970 00:00:00 GMT', 'If-None-Match' => 0})
            sub.callback do
              sub.should be_http_status(200)
              stored_messages.should eql(messagens_to_publish + 1)
              messages = sub.response.split("\r\n")
              messages.count.should eql(messagens_to_publish + 1)
              messages.each_with_index do |content, index|
                message = JSON.parse(content)
                message["id"].to_i.should eql(index + 1)
              end
              EventMachine.stop
            end
          end
        end
      end
    end

    it "should receive messages when connected in more than one channel" do
      channel_1 = 'ch_test_receiving_messages_when_connected_in_more_then_one_channel_1'
      channel_2 = 'ch_test_receiving_messages_when_connected_in_more_then_one_channel_2'
      body = 'published message'

      nginx_run_server(config.merge(:store_messages => "on", :message_template => '{\"id\":\"~id~\", \"message\":\"~text~\", \"channel\":\"~channel~\"}')) do |conf|
        EventMachine.run do
          sub_1 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel_1.to_s + '/' + channel_2.to_s).get :head => headers.merge({'If-Modified-Since' => 'Thu, 1 Jan 1970 00:00:00 GMT', 'If-None-Match' => 0})
          sub_1.callback do
            sub_1.should be_http_status(200)
            response = JSON.parse(sub_1.response)
            response["channel"].should eql(channel_1)

            sub_2 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel_1.to_s + '/' + channel_2.to_s).get :head => headers.merge({'If-Modified-Since' => sub_1.response_header['LAST_MODIFIED'], 'If-None-Match' => sub_1.response_header['ETAG']})
            sub_2.callback do
              sub_2.should be_http_status(200)
              response = JSON.parse(sub_2.response)
              response["channel"].should eql(channel_2)
              sub_2.response_header['ETAG'].to_i.should eql(sub_1.response_header['ETAG'].to_i + 1)

              EventMachine.stop
            end
          end

          publish_message_inline(channel_1.to_s, headers, body)
          publish_message_inline(channel_2.to_s, headers, body)
        end
      end
    end

    it "should accept delete a channel with a long polling subscriber" do
      channel = 'ch_test_delete_channel_with_long_polling_subscriber'
      body = 'published message'

      resp = ""
      nginx_run_server(config.merge(:publisher_mode => 'admin', :message_template => '{\"id\":\"~id~\", \"message\":\"~text~\", \"channel\":\"~channel~\"}')) do |conf|
        EventMachine.run do
          sub_1 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => headers
          sub_1.callback do
            sub_1.should be_http_status(200)
            response = JSON.parse(sub_1.response)
            response["channel"].should eql(channel)
            response["id"].to_i.should eql(-2)
            EventMachine.stop
          end

          pub = EventMachine::HttpRequest.new(nginx_address + '/pub?id=' + channel.to_s).delete :head => headers
          pub.callback do
            pub.should be_http_status(200).without_body
            pub.response_header['X_NGINX_PUSHSTREAM_EXPLAIN'].should eql("Channel deleted.")
          end
        end
      end
    end

    it "should accept send modified since and none match values without using header" do
      channel = 'ch_test_send_modified_since_and_none_match_values_not_using_headers'
      body = 'body'

      response = ""
      nginx_run_server(config.merge(:last_received_message_time => "$arg_time", :last_received_message_tag => "$arg_tag")) do |conf|
        EventMachine.run do
          sub_1 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => headers
          sub_1.stream do |chunk|
            response += chunk
          end
          sub_1.callback do |chunk|
            response.should eql("#{body}\r\n")

            time = sub_1.response_header['LAST_MODIFIED']
            tag = sub_1.response_header['ETAG']

            response = ""
            sub_2 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s + '?time=' + time + '&tag=' + tag).get :head => headers
            sub_2.stream do |chunk2|
              response += chunk2
            end
            sub_2.callback do
              response.should eql("#{body} 1\r\n")
              EventMachine.stop
            end

            publish_message_inline(channel, {}, body + " 1")
          end

          publish_message_inline(channel, {}, body)
        end
      end
    end

    it "should accept a callback parameter to be used with JSONP" do
      channel = 'ch_test_return_message_using_function_name_specified_in_callback_parameter'
      body = 'body'
      response = ""
      callback_function_name = "callback_function"

      nginx_run_server(config) do |conf|
        EventMachine.run do
          sub_1 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s + '?callback=' + callback_function_name).get :head => headers
          sub_1.callback do
            sub_1.response.should eql("#{callback_function_name}\r\n([#{body}\r\n]);\r\n")
            EventMachine.stop
          end

          publish_message_inline(channel, {}, body)
        end
      end
    end

    it "should return old messages using function name specified in callback parameter grouping in one answer" do
      channel = 'ch_test_return_old_messages_using_function_name_specified_in_callback_parameter_grouping_in_one_answer'
      body = 'body'
      response = ""
      callback_function_name = "callback_function"

      nginx_run_server(config) do |conf|
        EventMachine.run do
          publish_message_inline(channel, {}, body)
          publish_message_inline(channel, {}, body + "1")

          sub_1 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s + '.b2' + '?callback=' + callback_function_name).get :head => headers
          sub_1.callback do
            sub_1.response.should eql("#{callback_function_name}\r\n([#{body}\r\n,#{body + "1"}\r\n,]);\r\n")
            EventMachine.stop
          end
        end
      end
    end

    it "should force content_type to be application/javascript when using function name specified in callback parameter" do
      channel = 'test_force_content_type_to_be_application_javascript_when_using_function_name_specified_in_callback_parameter_when_polling'
      body = 'body'
      response = ""
      callback_function_name = "callback_function"

      nginx_run_server(config.merge({:content_type => "anything/value"})) do |conf|
        EventMachine.run do
          sent_headers = headers.merge({'accept' => 'otherknown/value'})
          sub_1 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s + '?callback=' + callback_function_name).get :head => sent_headers
          sub_1.callback do
            sub_1.response_header['CONTENT_TYPE'].should eql('application/javascript')
            EventMachine.stop
          end
          publish_message_inline(channel, {}, body)
        end
      end
    end

    it "should not cache the response" do
      channel = 'ch_test_not_cache_the_response'

      nginx_run_server(config.merge(:longpolling_connection_ttl => '1s')) do |conf|
        EventMachine.run do
          sub_1 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => headers
          sub_1.callback do
            sub_1.response_header["EXPIRES"].should eql("Thu, 01 Jan 1970 00:00:01 GMT")
            sub_1.response_header["CACHE_CONTROL"].should eql("no-cache, no-store, must-revalidate")
            EventMachine.stop
          end
        end
      end
    end

    it "should accpet return content gzipped" do
      channel = 'ch_test_get_content_gzipped'
      body = 'body'
      actual_response = ''

      nginx_run_server(config.merge({:gzip => "on"})) do |conf|
        EventMachine.run do
          sent_headers = headers.merge({'accept-encoding' => 'gzip, compressed'})
          sub_1 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => sent_headers, :decoding => false
          sub_1.stream do |chunk|
            actual_response << chunk
          end
          sub_1.callback do
            sub_1.should be_http_status(200)

            sub_1.response_header["CONTENT_ENCODING"].should eql("gzip")
            actual_response = Zlib::GzipReader.new(StringIO.new(actual_response)).read

            actual_response.should eql("#{body}\r\n")
            EventMachine.stop
          end
          publish_message_inline(channel, {}, body)
        end
      end
    end
  end

  context "when using subscriber push mode config" do
    let(:config) do
      {
        :ping_message_interval => nil,
        :header_template => nil,
        :footer_template => nil,
        :message_template => nil,
        :subscriber_mode => 'long-polling'
      }
    end

    let(:headers) do
      {'accept' => 'text/html'}
    end

    it_should_behave_like "long polling location"
  end

  context "when using push mode header" do
    let(:config) do
      {
        :ping_message_interval => nil,
        :header_template => nil,
        :footer_template => nil,
        :message_template => nil,
        :subscriber_mode => nil
      }
    end

    let(:headers) do
      {'accept' => 'text/html', 'X-Nginx-PushStream-Mode' => 'long-polling'}
    end

    it_should_behave_like "long polling location"
  end
end
