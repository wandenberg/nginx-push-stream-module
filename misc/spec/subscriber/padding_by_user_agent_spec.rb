require 'spec_helper'

describe "Subscriber Padding by user agent" do
  let(:default_config) do
    {
      :padding_by_user_agent => "[T|t]est 1,0,508",
      :user_agent => nil,
      :subscriber_connection_ttl => '1s',
      :header_template => nil,
      :message_template => nil,
      :footer_template => nil
    }
  end

  shared_examples_for "apply padding" do
    it "should apply a padding to the header" do
      channel = 'ch_test_header_padding'

      nginx_run_server(config.merge(:header_template => "0123456789", :padding_by_user_agent => "[T|t]est 1,1024,508:[T|t]est 2,4097,0")) do |conf|
        EventMachine.run do
          expected_size = conf.header_template.size + header_delta

          sub_1 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => headers.merge("User-Agent" => "Test 1")
          sub_1.callback do
            expect(sub_1).to be_http_status(200)
            expect(sub_1.response.size).to eql(1100 + expected_size)
            expect(sub_1.response).to match padding_pattern

            sub_2 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => headers.merge("User-Agent" => "Test 2")
            sub_2.callback do
              expect(sub_2).to be_http_status(200)
              expect(sub_2.response.size).to eql(4097 + expected_size)

              sub_3 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => headers.merge("User-Agent" => "Test 3")
              sub_3.callback do
                expect(sub_3).to be_http_status(200)
                expect(sub_3.response.size).to eql(expected_size)

                EventMachine.stop
              end
            end
          end
        end
      end
    end

    it "should apply a padding to the message" do
      channel = 'ch_test_message_padding'

      body = "0123456789"

      nginx_run_server(config) do |conf|
        EventMachine.run do
          expected_size = body.size + header_delta + body_delta

          sub_1 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => headers.merge("User-Agent" => "Test 1")
          sub_1.callback {
            expect(sub_1).to be_http_status(200)
            expect(sub_1.response.size).to eql(500 + expected_size)
            expect(sub_1.response).to match padding_pattern

            sub_2 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => headers.merge("User-Agent" => "Test 2")
            sub_2.callback {
              expect(sub_2).to be_http_status(200)
              expect(sub_2.response.size).to eql(expected_size)

              sub_3 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => headers.merge("User-Agent" => "Test 3")
              sub_3.callback {
                expect(sub_3).to be_http_status(200)
                expect(sub_3.response.size).to eql(expected_size)

                EventMachine.stop
              }
              publish_message_inline(channel, headers, body)
            }
            publish_message_inline(channel, headers, body)
          }
          publish_message_inline(channel, headers, body)
        end
      end
    end

    it "should apply a padding to the message with different sizes" do
      channel = 'ch_test_message_padding_with_different_sizes'

      nginx_run_server(config.merge(:padding_by_user_agent => "[T|t]est 1,0,545"), :timeout => 10) do |conf|
        EventMachine.run do
          i = 1
          expected_padding = 545
          expected_size = header_delta + body_delta

          sub_1 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => headers.merge("User-Agent" => "Test 1")
          sub_1.callback do
            expect(sub_1).to be_http_status(200)
            expect(sub_1.response.size).to eql(expected_padding + i + expected_size)
            expect(sub_1.response).to match padding_pattern

            i = 105
            expected_padding = 600 - ((i/100).to_i * 100)

            sub_1 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => headers.merge("User-Agent" => "Test 1")
            sub_1.callback do
              expect(sub_1).to be_http_status(200)
              expect(sub_1.response.size).to eql(expected_padding + i + expected_size)

              i = 221
              expected_padding = 600 - ((i/100).to_i * 100)

              sub_1 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => headers.merge("User-Agent" => "Test 1")
              sub_1.callback do
                expect(sub_1).to be_http_status(200)
                expect(sub_1.response.size).to eql(expected_padding + i + expected_size)

                i = 331
                expected_padding = 600 - ((i/100).to_i * 100)

                sub_1 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => headers.merge("User-Agent" => "Test 1")
                sub_1.callback do
                  expect(sub_1).to be_http_status(200)
                  expect(sub_1.response.size).to eql(expected_padding + i + expected_size)

                  i = 435
                  expected_padding = 600 - ((i/100).to_i * 100)

                  sub_1 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => headers.merge("User-Agent" => "Test 1")
                  sub_1.callback do
                    expect(sub_1).to be_http_status(200)
                    expect(sub_1.response.size).to eql(expected_padding + i + expected_size)

                    i = 502
                    expected_padding = 600 - ((i/100).to_i * 100)

                    sub_1 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => headers.merge("User-Agent" => "Test 1")
                    sub_1.callback do
                      expect(sub_1).to be_http_status(200)
                      expect(sub_1.response.size).to eql(expected_padding + i + expected_size)

                      i = 550

                      sub_1 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => headers.merge("User-Agent" => "Test 1")
                      sub_1.callback do
                        expect(sub_1).to be_http_status(200)
                        expect(sub_1.response.size).to eql(i + expected_size)

                        EventMachine.stop
                      end
                      publish_message_inline(channel, headers, "_" * i)
                    end
                    publish_message_inline(channel, headers, "_" * i)
                  end
                  publish_message_inline(channel, headers, "_" * i)
                end
                publish_message_inline(channel, headers, "_" * i)
              end
              publish_message_inline(channel, headers, "_" * i)
            end
            publish_message_inline(channel, headers, "_" * i)
          end
          publish_message_inline(channel, headers, "_" * i)
        end
      end
    end

    it "should accept the user agent set by a complex value" do
      channel = 'ch_test_user_agent_by_complex_value'

      nginx_run_server(config.merge(:padding_by_user_agent => "[T|t]est 1,1024,512", :user_agent => "$arg_ua", :header_template => "0123456789"), :timeout => 10) do |conf|
        EventMachine.run do
          expected_size = conf.header_template.size + header_delta

          sub_1 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s + '?ua=test%201').get :head => headers
          sub_1.callback do
            expect(sub_1).to be_http_status(200)
            expect(sub_1.response.size).to eql(1024 + expected_size)
            expect(sub_1.response).to match padding_pattern

            sub_2 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s + '?ua=test%202').get :head => headers
            sub_2.callback do
              expect(sub_2).to be_http_status(200)
              expect(sub_2.response.size).to eql(expected_size)

              EventMachine.stop
            end
          end
        end
      end
    end
  end

  describe "for non EventSource mode" do
    let(:config) { default_config }
    let(:padding_pattern) { /(\r\n)+\r\n\r\n\r\n$/ }
    let(:header_delta) { 0 }
    let(:body_delta) { 0 }

    it_should_behave_like "apply padding"
  end

  describe "for EventSource mode" do
    let(:config) { default_config.merge(:subscriber_mode => "eventsource") }
    let(:padding_pattern) { /(:::)+\n$/ }
    let(:header_delta) { 3 }
    let(:body_delta) { 8 }

    it_should_behave_like "apply padding"
  end
end
