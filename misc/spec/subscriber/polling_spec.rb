require 'spec_helper'

describe "Subscriber Properties" do

  shared_examples_for "polling location" do

    describe "when has no messages" do

      it "should receive a 304" do
        channel = 'ch_test_receive_a_304_when_has_no_messages'

        nginx_run_server(config) do |conf|
          EventMachine.run do
            sub_1 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => headers
            sub_1.callback do
              sub_1.should be_http_status(304).without_body
              sub_1.response_header['LAST_MODIFIED'].to_s.should eql("")
              sub_1.response_header['ETAG'].to_s.should eql("")
              EventMachine.stop
            end
          end
        end
      end

      it "should receive a 304 keeping sent headers" do
        channel = 'ch_test_receive_a_304_when_has_no_messages_keeping_headers'

        sent_headers = headers.merge({'If-Modified-Since' => Time.now.utc.strftime("%a, %d %b %Y %T %Z"), 'If-None-Match' => '3'})
        nginx_run_server(config) do |conf|
          EventMachine.run do
            sub_1 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => sent_headers
            sub_1.callback do
              sub_1.should be_http_status(304).without_body
              Time.parse(sub_1.response_header['LAST_MODIFIED'].to_s).should eql(Time.parse(sent_headers['If-Modified-Since']))
              sub_1.response_header['ETAG'].to_s.should eql(sent_headers['If-None-Match'])
              EventMachine.stop
            end
          end
        end
      end

    end

    describe "when has messages" do

      it "should receive specific headers" do
        channel = 'ch_test_receive_specific_headers_when_has_messages'
        body = 'body'

        nginx_run_server(config) do |conf|
          EventMachine.run do
            publish_message_inline(channel, {}, body)

            sub_1 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => headers
            sub_1.callback do
              sub_1.should be_http_status(200)
              sub_1.response_header['LAST_MODIFIED'].to_s.should_not eql("")
              sub_1.response_header['ETAG'].to_s.should eql("0")
              sub_1.response.should eql("#{body}\r\n")
              EventMachine.stop
            end
          end
        end
      end

      it "should receive old messages by if_modified_since header" do
        channel = 'ch_test_getting_messages_by_if_modified_since_header'
        body = 'body'

        nginx_run_server(config) do |conf|
          EventMachine.run do
            publish_message_inline(channel, {}, body)
            sub_1 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => headers
            sub_1.callback do
              sub_1.should be_http_status(200)
              sub_1.response_header['LAST_MODIFIED'].to_s.should_not eql("")
              sub_1.response_header['ETAG'].to_s.should_not eql("")
              sub_1.response.should eql("#{body}\r\n")

              sent_headers = headers.merge({'If-Modified-Since' => sub_1.response_header['LAST_MODIFIED'], 'If-None-Match' => sub_1.response_header['ETAG']})
              sub_2 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => sent_headers
              sub_2.callback do
                sub_2.should be_http_status(304).without_body
                sub_2.response_header['LAST_MODIFIED'].to_s.should eql(sub_1.response_header['LAST_MODIFIED'])
                sub_2.response_header['ETAG'].to_s.should eql(sub_1.response_header['ETAG'])

                sleep(1) # to publish the second message in a different second from the first
                publish_message_inline(channel, {}, body + "1")

                sent_headers = headers.merge({'If-Modified-Since' => sub_2.response_header['LAST_MODIFIED'], 'If-None-Match' => sub_2.response_header['ETAG']})
                sub_3 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => sent_headers
                sub_3.callback do
                  sub_3.should be_http_status(200)
                  sub_3.response_header['LAST_MODIFIED'].to_s.should_not eql(sub_2.response_header['LAST_MODIFIED'])
                  sub_3.response_header['ETAG'].to_s.should eql("0")
                  sub_3.response.should eql("#{body}1\r\n")

                  EventMachine.stop
                end
              end
            end
          end
        end
      end

      it "should receive old messages by backtrack" do
        channel = 'ch_test_getting_messages_by_backtrack'
        body = 'body'

        nginx_run_server(config) do |conf|
          EventMachine.run do
            publish_message_inline(channel, {}, body)
            publish_message_inline(channel, {}, body + "1")
            publish_message_inline(channel, {}, body + "2")

            sub_1 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s + '.b1').get :head => headers
            sub_1.callback do
              sub_1.should be_http_status(200)
              sub_1.response_header['LAST_MODIFIED'].to_s.should_not eql("")
              sub_1.response_header['ETAG'].to_s.should eql("2")
              sub_1.response.should eql("#{body}2\r\n")

              sent_headers = headers.merge({'If-Modified-Since' => sub_1.response_header['LAST_MODIFIED'], 'If-None-Match' => sub_1.response_header['ETAG']})
              sub_2 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => sent_headers
              sub_2.callback do
                sub_2.should be_http_status(304).without_body
                sub_2.response_header['LAST_MODIFIED'].to_s.should eql(sub_1.response_header['LAST_MODIFIED'])
                sub_2.response_header['ETAG'].to_s.should eql(sub_1.response_header['ETAG'])

                sleep(1) # to publish the second message in a different second from the first
                publish_message_inline(channel, {}, body + "3")

                sent_headers = headers.merge({'If-Modified-Since' => sub_2.response_header['LAST_MODIFIED'], 'If-None-Match' => sub_2.response_header['ETAG']})
                sub_3 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => sent_headers
                sub_3.callback do
                  sub_3.should be_http_status(200)
                  sub_3.response_header['LAST_MODIFIED'].to_s.should_not eql(sub_2.response_header['LAST_MODIFIED'])
                  sub_3.response_header['ETAG'].to_s.should eql("0")
                  sub_3.response.should eql("#{body}3\r\n")

                  EventMachine.stop
                end
              end
            end
          end
        end
      end

      it "should receive old messages by last_event_id header" do
        channel = 'ch_test_getting_messages_by_last_event_id_header'
        body = 'body'

        nginx_run_server(config) do |conf|
          EventMachine.run do
            publish_message_inline(channel, {'Event-Id' => 'event 1'}, 'msg 1')
            publish_message_inline(channel, {'Event-Id' => 'event 2'}, 'msg 2')
            publish_message_inline(channel, {}, 'msg 3')
            publish_message_inline(channel, {'Event-Id' => 'event 3'}, 'msg 4')

            sent_headers = headers.merge({'Last-Event-Id' => 'event 2'})
            sub_1 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => sent_headers
            sub_1.callback do
              sub_1.should be_http_status(200)
              sub_1.response_header['LAST_MODIFIED'].to_s.should_not eql("")
              sub_1.response_header['ETAG'].to_s.should eql("3")
              sub_1.response.should eql("msg 3\r\nmsg 4\r\n")

              sent_headers = headers.merge({'If-Modified-Since' => sub_1.response_header['LAST_MODIFIED'], 'If-None-Match' => sub_1.response_header['ETAG']})
              sub_2 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => sent_headers
              sub_2.callback do
                sub_2.should be_http_status(304).without_body
                sub_2.response_header['LAST_MODIFIED'].to_s.should eql(sub_1.response_header['LAST_MODIFIED'])
                sub_2.response_header['ETAG'].to_s.should eql(sub_1.response_header['ETAG'])

                sleep(1) # to publish the second message in a different second from the first
                publish_message_inline(channel, {}, body + "3")

                sent_headers = headers.merge({'If-Modified-Since' => sub_2.response_header['LAST_MODIFIED'], 'If-None-Match' => sub_2.response_header['ETAG']})
                sub_3 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => sent_headers
                sub_3.callback do
                  sub_3.should be_http_status(200)
                  sub_3.response_header['LAST_MODIFIED'].to_s.should_not eql(sub_2.response_header['LAST_MODIFIED'])
                  sub_3.response_header['ETAG'].to_s.should eql("0")
                  sub_3.response.should eql("#{body}3\r\n")

                  EventMachine.stop
                end
              end
            end
          end
        end
      end

      it "should receive old messages from different channels" do
        channel_1 = 'ch_test_receive_old_messages_from_different_channels_1'
        channel_2 = 'ch_test_receive_old_messages_from_different_channels_2'
        body = 'body'

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
                sub_2.should be_http_status(304).without_body
                sub_2.response_header['LAST_MODIFIED'].to_s.should eql(sub_1.response_header['LAST_MODIFIED'])
                sub_2.response_header['ETAG'].to_s.should eql(sub_1.response_header['ETAG'])

                sleep(1) # to publish the second message in a different second from the first
                publish_message_inline(channel_1, {}, body + "1_1")

                sent_headers = headers.merge({'If-Modified-Since' => sub_2.response_header['LAST_MODIFIED'], 'If-None-Match' => sub_2.response_header['ETAG']})
                sub_3 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel_2.to_s + '/' + channel_1.to_s).get :head => sent_headers
                sub_3.callback do
                  sub_3.should be_http_status(200)
                  sub_3.response_header['LAST_MODIFIED'].to_s.should_not eql(sub_2.response_header['LAST_MODIFIED'])
                  sub_3.response_header['ETAG'].to_s.should eql("0")
                  sub_3.response.should eql("#{body}1_1\r\n")

                  sent_headers = headers.merge({'If-Modified-Since' => sub_3.response_header['LAST_MODIFIED'], 'If-None-Match' => sub_3.response_header['ETAG']})
                  sub_4 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel_2.to_s + '/' + channel_1.to_s).get :head => sent_headers
                  sub_4.callback do
                    sub_4.should be_http_status(304).without_body
                    sub_4.response_header['LAST_MODIFIED'].to_s.should eql(sub_3.response_header['LAST_MODIFIED'])
                    sub_4.response_header['ETAG'].to_s.should eql(sub_3.response_header['ETAG'])

                    sleep(1) # to publish the second message in a different second from the first
                    publish_message_inline(channel_2, {}, body + "1_2")

                    sent_headers = headers.merge({'If-Modified-Since' => sub_4.response_header['LAST_MODIFIED'], 'If-None-Match' => sub_4.response_header['ETAG']})
                    sub_5 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel_2.to_s + '/' + channel_1.to_s).get :head => sent_headers
                    sub_5.callback do
                      sub_5.should be_http_status(200)
                      sub_5.response_header['LAST_MODIFIED'].to_s.should_not eql(sub_4.response_header['LAST_MODIFIED'])
                      sub_5.response_header['ETAG'].to_s.should eql("0")
                      sub_5.response.should eql("#{body}1_2\r\n")

                      EventMachine.stop
                    end
                  end
                end
              end
            end
          end
        end
      end

      it "should accept modified since and none match values not using headers when polling" do
        channel = 'ch_test_send_modified_since_and_none_match_values_not_using_headers_when_polling'
        body = 'body'

        nginx_run_server(config.merge(:last_received_message_time => "$arg_time", :last_received_message_tag => "$arg_tag")) do |conf|
          EventMachine.run do
            publish_message_inline(channel, {}, body)

            sub_1 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => headers
            sub_1.callback do
              sub_1.response.should eql("#{body}\r\n")

              time = sub_1.response_header['LAST_MODIFIED']
              tag = sub_1.response_header['ETAG']

              publish_message_inline(channel, {}, body + " 1")

              response = ""
              sub_2 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s + '?time=' + time + '&tag=' + tag).get :head => headers
              sub_2.callback do
                sub_2.response.should eql("#{body} 1\r\n")
                EventMachine.stop
              end
            end
          end
        end
      end

      it "should accept a callback parameter to works with JSONP" do
        channel = 'ch_test_return_message_using_function_name_specified_in_callback_parameter_when_polling'
        body = 'body'
        response = ""
        callback_function_name = "callback_function"

        nginx_run_server(config) do |conf|
          EventMachine.run do
            publish_message_inline(channel, {}, body)
            sub_1 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s + '?callback=' + callback_function_name).get :head => headers
            sub_1.callback do
              sub_1.response.should eql("#{callback_function_name}\r\n([#{body}\r\n,]);\r\n")
              EventMachine.stop
            end
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
            publish_message_inline(channel, {}, body)
            sent_headers = headers.merge({'accept' => 'otherknown/value'})
            sub_1 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s + '?callback=' + callback_function_name).get :head => sent_headers
            sub_1.callback do
              sub_1.response_header['CONTENT_TYPE'].should eql('application/javascript')
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
            publish_message_inline(channel, {}, body)

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
          end
        end
      end
    end

    it "should not cache the response" do
      channel = 'ch_test_not_cache_the_response'

      nginx_run_server(config) do |conf|
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
  end

  context "when using subscriber push mode config" do
    let(:config) do
      {
        :ping_message_interval => nil,
        :header_template => nil,
        :footer_template => nil,
        :message_template => nil,
        :subscriber_mode => 'polling'
      }
    end

    let(:headers) do
      {'accept' => 'text/html'}
    end

    it_should_behave_like "polling location"
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
      {'accept' => 'text/html', 'X-Nginx-PushStream-Mode' => 'polling'}
    end

    it_should_behave_like "polling location"
  end
end
