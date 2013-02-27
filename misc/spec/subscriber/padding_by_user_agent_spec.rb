require 'spec_helper'

describe "Subscriber Padding by user agent" do
  let(:config) do
    {
      :padding_by_user_agent => "[T|t]est 1,1024,508:[T|t]est 2,4097,0",
      :user_agent => nil,
      :subscriber_connection_ttl => '1s',
      :header_template => nil,
      :message_template => nil,
      :footer_template => nil
    }
  end

  it "should apply a padding to the header" do
    channel = 'ch_test_header_padding'

    nginx_run_server(config.merge(:header_template => "0123456789")) do |conf|
      EventMachine.run do
        sub_1 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => headers.merge("User-Agent" => "Test 1")
        sub_1.callback do
          sub_1.response_header.status.should eql(200)
          sub_1.response.size.should eql(1100 + conf.header_template.size + 4)

          sub_2 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => headers.merge("User-Agent" => "Test 2")
          sub_2.callback do
            sub_2.response_header.status.should eql(200)
            sub_2.response.size.should eql(4097 + conf.header_template.size + 4)

            sub_3 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => headers.merge("User-Agent" => "Test 3")
            sub_3.callback do
              sub_3.response_header.status.should eql(200)
              sub_3.response.size.should eql(conf.header_template.size + 2)

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
        sub_1 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => headers.merge("User-Agent" => "Test 1")
        sub_1.callback {
          sub_1.response_header.status.should eql(200)
          sub_1.response.size.should eql(500 + body.size + 4)

          sub_2 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => headers.merge("User-Agent" => "Test 2")
          sub_2.callback {
            sub_2.response_header.status.should eql(200)
            sub_2.response.size.should eql(body.size + 2)

            sub_3 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => headers.merge("User-Agent" => "Test 3")
            sub_3.callback {
              sub_3.response_header.status.should eql(200)
              sub_3.response.size.should eql(body.size + 2)

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

        sub_1 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => headers.merge("User-Agent" => "Test 1")
        sub_1.callback do
          sub_1.response_header.status.should eql(200)
          sub_1.response.size.should eql(expected_padding + i + 4)

          i = 105
          expected_padding = 600 - ((i/100).to_i * 100)

          sub_1 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => headers.merge("User-Agent" => "Test 1")
          sub_1.callback do
            sub_1.response_header.status.should eql(200)
            sub_1.response.size.should eql(expected_padding + i + 4)

            i = 221
            expected_padding = 600 - ((i/100).to_i * 100)

            sub_1 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => headers.merge("User-Agent" => "Test 1")
            sub_1.callback do
              sub_1.response_header.status.should eql(200)
              sub_1.response.size.should eql(expected_padding + i + 4)

              i = 331
              expected_padding = 600 - ((i/100).to_i * 100)

              sub_1 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => headers.merge("User-Agent" => "Test 1")
              sub_1.callback do
                sub_1.response_header.status.should eql(200)
                sub_1.response.size.should eql(expected_padding + i + 4)

                i = 435
                expected_padding = 600 - ((i/100).to_i * 100)

                sub_1 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => headers.merge("User-Agent" => "Test 1")
                sub_1.callback do
                  sub_1.response_header.status.should eql(200)
                  sub_1.response.size.should eql(expected_padding + i + 4)

                  i = 502
                  expected_padding = 600 - ((i/100).to_i * 100)

                  sub_1 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => headers.merge("User-Agent" => "Test 1")
                  sub_1.callback do
                    sub_1.response_header.status.should eql(200)
                    sub_1.response.size.should eql(expected_padding + i + 4)

                    i = 550

                    sub_1 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => headers.merge("User-Agent" => "Test 1")
                    sub_1.callback do
                      sub_1.response_header.status.should eql(200)
                      sub_1.response.size.should eql(i + 2)

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
        sub_1 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s + '?ua=test 1').get :head => headers
        sub_1.callback do
          sub_1.response_header.status.should eql(200)
          sub_1.response.size.should eql(1024 + conf.header_template.size + 4)

          sub_2 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s + '?ua=test 2').get :head => headers
          sub_2.callback do
            sub_2.response_header.status.should eql(200)
            sub_2.response.size.should eql(conf.header_template.size + 2)

            EventMachine.stop
          end
        end
      end
    end
  end
end
