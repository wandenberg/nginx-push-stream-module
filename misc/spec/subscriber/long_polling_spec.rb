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
            expect(response).to eql("#{body}")

            sent_headers = headers.merge({'If-Modified-Since' => sub_1.response_header['LAST_MODIFIED'], 'If-None-Match' => sub_1.response_header['ETAG']})
            response = ""
            sub_2 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => sent_headers
            sub_2.stream do |chunk2|
              response += chunk2
            end
            sub_2.callback do
              expect(response).to eql("#{body} 1")
              EventMachine.stop
            end

            publish_message_inline(channel, {}, body + " 1")
          end

          publish_message_inline(channel, {}, body)
        end
      end
    end

    it "should disconnect after receive old messages" do
      channel = 'ch_test_disconnect_after_receive_old_messages_by_last_event_id_when_longpolling_is_on'
      response = ""

      nginx_run_server(config) do |conf|
        EventMachine.run do
          publish_message(channel, {'Event-Id' => 'event 1'}, 'msg 1')
          publish_message(channel, {'Event-Id' => 'event 2'}, 'msg 2')
          publish_message(channel, {}, 'msg 3')
          publish_message(channel, {'Event-Id' => 'event 3'}, 'msg 4')

          sub = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => headers.merge({'Last-Event-Id' => 'event 2'})
          sub.stream do |chunk|
            response += chunk
          end
          sub.callback do |chunk|
            expect(response).to eql("msg 3msg 4")
            EventMachine.stop
          end
        end
      end
    end

    it "should disconnect after timeout is reached" do
      channel = 'ch_test_disconnect_long_polling_subscriber_when_longpolling_timeout_is_set'

      start = Time.now
      nginx_run_server(config.merge(:subscriber_connection_ttl => "10s"), :timeout => 30) do |conf|
        EventMachine.run do
          sub = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s, :inactivity_timeout => 15).get :head => headers
          sub.callback do
            stop = Time.now
            expect(time_diff_sec(start, stop)).to be_in_the_interval(10, 10.5)
            expect(sub).to be_http_status(304).without_body
            expect(Time.parse(sub.response_header['LAST_MODIFIED'].to_s).utc.to_i).to be_in_the_interval(Time.now.utc.to_i-1, Time.now.utc.to_i)
            expect(sub.response_header['ETAG'].to_s).to eql("W/0")
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
            expect(time_diff_sec(start, stop)).to be_in_the_interval(5, 5.5)
            expect(sub).to be_http_status(304).without_body
            expect(Time.parse(sub.response_header['LAST_MODIFIED'].to_s).utc.to_i).to be_in_the_interval(Time.now.utc.to_i-1, Time.now.utc.to_i)
            expect(sub.response_header['ETAG'].to_s).to eql("W/0")
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
            expect(time_diff_sec(start, stop)).to be_in_the_interval(3, 3.5)
            expect(sub).to be_http_status(304).without_body
            expect(Time.parse(sub.response_header['LAST_MODIFIED'].to_s).utc.to_i).to be_in_the_interval(Time.now.utc.to_i-1, Time.now.utc.to_i)
            expect(sub.response_header['ETAG'].to_s).to eql("W/0")
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
            expect(time_diff_sec(start, stop)).to be_in_the_interval(5, 5.5)
            expect(sub).to be_http_status(304).without_body
            EventMachine.stop
          end
        end
      end
    end

    it "should receive a timed out message when timeout_with_body is on" do
      channel = 'ch_test_disconnect_long_polling_subscriber_when_longpolling_timeout_is_set'
      callback_function_name = "callback_function"

      start = Time.now
      nginx_run_server(config.merge(:subscriber_connection_ttl => "1s", :timeout_with_body => 'on', :message_template => '{\"id\":\"~id~\", \"message\":\"~text~\", \"channel\":\"~channel~\", \"tag\":\"~tag~\", \"time\":\"~time~\"}'), :timeout => 30) do |conf|
        EventMachine.run do
          sub = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => headers
          sub.callback do
            stop = Time.now
            expect(time_diff_sec(start, stop)).to be_in_the_interval(1, 1.5)
            expect(sub).to be_http_status(200)
            response = JSON.parse(sub.response)
            expect(response["id"]).to eql("-3")
            expect(response["message"]).to eql("Timed out")
            expect(response["channel"]).to eql("")
            expect(response["tag"]).to eql("0")
            expect(response["time"]).to eql("Thu, 01 Jan 1970 00:00:00 GMT")
            expect(Time.parse(sub.response_header['LAST_MODIFIED'].to_s).utc.to_i).to be_in_the_interval(Time.now.utc.to_i-1, Time.now.utc.to_i)
            expect(sub.response_header['ETAG'].to_s).to eql("W/0")

            sub_1 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s + '?callback=' + callback_function_name).get :head => headers
            sub_1.callback do
              expect(sub_1.response).to eql(%(callback_function([{"id":"-3", "message":"Timed out", "channel":"", "tag":"0", "time":"Thu, 01 Jan 1970 00:00:00 GMT"}]);))
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
            expect(sub_1).to be_http_status(200)
            response = JSON.parse(sub_1.response)
            expect(response["channel"]).to eql(channel_1)

            sub_2 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel_1.to_s + '/' + channel_2.to_s).get :head => headers.merge({'If-Modified-Since' => sub_1.response_header['LAST_MODIFIED'], 'If-None-Match' => sub_1.response_header['ETAG']})
            sub_2.callback do
              expect(sub_2).to be_http_status(200)
              response = JSON.parse(sub_2.response)
              expect(response["channel"]).to eql(channel_2)
              expect(sub_2.response_header['ETAG'].sub("W/", "").to_i).to eql(sub_1.response_header['ETAG'].sub("W/", "").to_i + 1)

              EventMachine.stop
            end
          end

          EM.add_timer(0.5) do
            publish_message(channel_1.to_s, headers, body)
            publish_message(channel_2.to_s, headers, body)
          end
        end
      end
    end

    it "should accept delete a channel with a long polling subscriber" do
      channel = 'ch_test_delete_channel_with_long_polling_subscriber'
      callback_function_name = "callback_function"

      resp = ""
      nginx_run_server(config.merge(:publisher_mode => 'admin', :message_template => '{\"id\":\"~id~\", \"message\":\"~text~\", \"channel\":\"~channel~\"}')) do |conf|
        EventMachine.run do
          sub_1 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => headers
          sub_1.callback do
            expect(sub_1).to be_http_status(200)
            response = JSON.parse(sub_1.response)
            expect(response["channel"]).to eql(channel)
            expect(response["id"].to_i).to eql(-2)
          end

          sub_2 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s + '?callback=' + callback_function_name).get :head => headers
          sub_2.callback do
            expect(sub_2.response).to eql(%(#{callback_function_name}([{"id":"-2", "message":"Channel deleted", "channel":"ch_test_delete_channel_with_long_polling_subscriber"}]);))
            EventMachine.stop
          end

          EM.add_timer(0.5) do
            pub = EventMachine::HttpRequest.new(nginx_address + '/pub?id=' + channel.to_s).delete :head => headers
            pub.callback do
              expect(pub).to be_http_status(200).without_body
              expect(pub.response_header['X_NGINX_PUSHSTREAM_EXPLAIN']).to eql("Channel deleted.")
            end
          end
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
            expect(sub_1.response).to eql("#{callback_function_name}([#{body}]);")
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
          publish_message(channel, {'Event-Id' => 'event_id'}, body)
          publish_message(channel, {}, body + "1")

          sub_1 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s + '.b2' + '?callback=' + callback_function_name).get :head => headers
          sub_1.callback do
            expect(sub_1.response).to eql("#{callback_function_name}([#{body},#{body + "1"}]);")

            sub_2 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s + '?callback=' + callback_function_name).get :head => headers.merge({'Last-Event-Id' => 'event_id'})
            sub_2.callback do
              expect(sub_2.response).to eql("#{callback_function_name}([#{body + "1"}]);")

              sub_3 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s + '?callback=' + callback_function_name).get :head => headers.merge({'If-Modified-Since' => Time.at(0).utc.strftime("%a, %d %b %Y %T %Z")})
              sub_3.callback do
                expect(sub_3.response).to eql("#{callback_function_name}([#{body},#{body + "1"}]);")

                EventMachine.stop
              end
            end
          end
        end
      end
    end

    it "should return messages from different channels on JSONP response" do
      channel_1 = 'ch_test_jsonp_ch1'
      channel_2 = 'ch_test_jsonp_ch2'
      channel_3 = 'ch_test_jsonp_ch3'
      body = 'body'
      response = ""
      callback_function_name = "callback_function"

      nginx_run_server(config) do |conf|
        EventMachine.run do
          publish_message(channel_1, {}, body + "1_1")
          publish_message(channel_2, {}, body + "1_2")
          publish_message(channel_3, {}, body + "1_3")
          publish_message(channel_1, {}, body + "2_1")
          publish_message(channel_2, {}, body + "2_2")
          publish_message(channel_3, {}, body + "2_3")
          publish_message(channel_1, {}, body + "3_1")
          publish_message(channel_2, {}, body + "3_2")
          publish_message(channel_3, {}, body + "3_3")

          sub_1 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel_1.to_s + '.b3/' + channel_2.to_s + '.b3/' + channel_3.to_s + '.b3' + '?callback=' + callback_function_name).get :head => headers
          sub_1.callback do
            expect(sub_1.response).to eql("#{callback_function_name}([#{body}1_1,#{body}2_1,#{body}3_1,#{body}1_2,#{body}2_2,#{body}3_2,#{body}1_3,#{body}2_3,#{body}3_3]);")
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
            expect(sub_1.response_header['CONTENT_TYPE']).to eql('application/javascript')
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
            expect(sub_1.response_header["EXPIRES"]).to eql("Thu, 01 Jan 1970 00:00:01 GMT")
            expect(sub_1.response_header["CACHE_CONTROL"]).to eql("no-cache, no-store, must-revalidate")
            EventMachine.stop
          end
        end
      end
    end

    it "should accept return content gzipped" do
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
            expect(sub_1).to be_http_status(200)

            expect(sub_1.response_header["ETAG"]).to match(/W\/\d+/)
            expect(sub_1.response_header["CONTENT_ENCODING"]).to eql("gzip")
            actual_response = Zlib::GzipReader.new(StringIO.new(actual_response)).read

            expect(actual_response).to eql("#{body}")
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
