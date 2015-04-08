require 'spec_helper'

describe "Subscriber Connection Cleanup" do
  let(:config) do
    {
      :subscriber_connection_ttl => '17s',
      :header_template => 'HEADER_TEMPLATE',
      :footer_template => 'FOOTER_TEMPLATE',
      :ping_message_interval => '3s'
    }
  end

  it "should disconnect the subscriber after the configured connection ttl be reached" do
    channel = 'ch_test_subscriber_connection_timeout'

    nginx_run_server(config.merge(:ping_message_interval => nil), :timeout => 25) do |conf|
      start = Time.now
      response = ''

      EventMachine.run do
        sub = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s, :inactivity_timeout => 20).get :head => headers

        sub.stream do |chunk|
          response += chunk
          expect(response).to include(conf.header_template)
        end

        sub.callback do
          stop = Time.now
          expect(time_diff_sec(start, stop)).to be_in_the_interval(17, 17.5)
          expect(response).to include(conf.footer_template)
          EventMachine.stop
        end
      end
    end
  end

  it "should disconnect the subscriber after the configured connection ttl be reached with ping message" do
    channel = 'ch_test_subscriber_connection_timeout_with_ping_message'

    nginx_run_server(config.merge(:header_template => nil, :footer_template => nil), :timeout => 25) do |conf|
      start = Time.now
      chunks_received = 0

      EventMachine.run do
        sub = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => headers

        sub.stream do |chunk|
          chunks_received += 1
        end

        sub.callback do
          stop = Time.now
          expect(time_diff_sec(start, stop)).to be_in_the_interval(17, 17.5)
          expect(chunks_received).to be_eql(5)
          EventMachine.stop
        end
      end
    end
  end

  it "should disconnect each subscriber after the configured connection ttl be reached starting when it connects" do
    channel = 'ch_test_multiple_subscribers_connection_timeout'

    nginx_run_server(config.merge(:subscriber_connection_ttl => '5s', :ping_message_interval => nil), :timeout => 25) do |conf|
      EventMachine.run do
        response_1 = ''
        sub_1 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => headers
        sub_1.stream do |chunk|
          response_1 += chunk
          expect(response_1).to include(conf.header_template)
        end
        sub_1.callback do
          expect(response_1).to include(conf.footer_template)
        end

        sleep(2)

        response_2 = ''
        sub_2 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => headers
        sub_2.stream do |chunk|
          response_2 += chunk
          expect(response_2).to include(conf.header_template)
        end
        sub_2.callback do
          expect(response_2).to include(conf.footer_template)

          response_4 = ''
          sub_4 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => headers
          sub_4.stream do |chunk|
            response_4 += chunk
            expect(response_4).to include(conf.header_template)
          end
          sub_4.callback do
            expect(response_4).to include(conf.footer_template)
            EventMachine.stop
          end
        end

        sleep(6)

        response_3 = ''
        sub_3 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => headers
        sub_3.stream do |chunk|
          response_3 += chunk
          expect(response_3).to include(conf.header_template)
        end
        sub_3.callback do
          expect(response_3).to include(conf.footer_template)
        end

      end
    end
  end
end
