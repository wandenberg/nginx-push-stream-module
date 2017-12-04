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
              expect(sub_1).to be_http_status(304).without_body
              expect(sub_1.response_header['LAST_MODIFIED'].to_s).to eql("")
              expect(sub_1.response_header['ETAG'].to_s).to eql("")
              EventMachine.stop
            end
          end
        end
      end

      it "should receive a 304 keeping sent headers" do
        channel = 'ch_test_receive_a_304_when_has_no_messages_keeping_headers'

        sent_headers = headers.merge({'If-Modified-Since' => Time.now.utc.strftime("%a, %d %b %Y %T %Z"), 'If-None-Match' => 'W/3'})
        nginx_run_server(config) do |conf|
          EventMachine.run do
            sub_1 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => sent_headers
            sub_1.callback do
              expect(sub_1).to be_http_status(304).without_body
              expect(Time.parse(sub_1.response_header['LAST_MODIFIED'].to_s)).to eql(Time.parse(sent_headers['If-Modified-Since']))
              expect(sub_1.response_header['ETAG'].to_s).to eql(sent_headers['If-None-Match'])
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
            publish_message(channel, {}, body)

            sub_1 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => headers.merge({'If-Modified-Since' => Time.at(0).utc.strftime("%a, %d %b %Y %T %Z")})
            sub_1.callback do
              expect(sub_1).to be_http_status(200)
              expect(sub_1.response_header['LAST_MODIFIED'].to_s).not_to eql("")
              expect(sub_1.response_header['ETAG'].to_s).to eql("W/1")
              expect(sub_1.response).to eql("#{body}")
              EventMachine.stop
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
            publish_message(channel, {}, body)
            sub_1 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s + '?callback=' + callback_function_name).get :head => headers.merge({'If-Modified-Since' => Time.at(0).utc.strftime("%a, %d %b %Y %T %Z")})
            sub_1.callback do
              expect(sub_1.response).to eql("#{callback_function_name}([#{body}]);")
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

      it "should force content_type to be application/javascript when using function name specified in callback parameter" do
        channel = 'test_force_content_type_to_be_application_javascript_when_using_function_name_specified_in_callback_parameter_when_polling'
        body = 'body'
        response = ""
        callback_function_name = "callback_function"

        nginx_run_server(config.merge({:content_type => "anything/value"})) do |conf|
          EventMachine.run do
            publish_message(channel, {}, body)
            sent_headers = headers.merge({'accept' => 'otherknown/value'})
            sub_1 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s + '?callback=' + callback_function_name).get :head => sent_headers
            sub_1.callback do
              expect(sub_1.response_header['CONTENT_TYPE']).to eql('application/javascript')
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
            publish_message(channel, {}, body)

            sent_headers = headers.merge({'accept-encoding' => 'gzip, compressed', 'If-Modified-Since' => Time.at(0).utc.strftime("%a, %d %b %Y %T %Z")})
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
            expect(sub_1.response_header["EXPIRES"]).to eql("Thu, 01 Jan 1970 00:00:01 GMT")
            expect(sub_1.response_header["CACHE_CONTROL"]).to eql("no-cache, no-store, must-revalidate")
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
