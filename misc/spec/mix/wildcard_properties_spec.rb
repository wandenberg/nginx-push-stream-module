require 'spec_helper'

describe "Wildcard Properties" do
  let(:config) do
    {
      :authorized_channels_only => "on",
      :header_template => 'connected',
      :wildcard_channel_prefix => "XXX_"
    }
  end

  it "should identify wildcard channels by prefix" do
    channel = 'ch_test_wildcard_channel_prefix'
    channel_broad = 'XXX_123'
    channel_broad_fail = 'YYY_123'

    body = 'wildcard channel prefix'

    nginx_run_server(config) do |conf|
      EventMachine.run do
        pub = EventMachine::HttpRequest.new(nginx_address + '/pub?id=' + channel.to_s ).post :head => headers, :body => body
        pub.callback do
          sub_1 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s + '/' + channel_broad_fail).get :head => headers
          sub_1.callback do |chunk|
            expect(sub_1).to be_http_status(403).without_body

            sub_2 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s + '/' + channel_broad).get :head => headers
            sub_2.stream do |chunk2|
              expect(chunk2).to eql(conf.header_template)
              EventMachine.stop
            end
          end
        end
      end
    end
  end

  it "should limit the number of wildcard channels in the same request" do
    channel = 'ch_test_wildcard_channel_max_qtd'
    channel_broad1 = 'XXX_123'
    channel_broad2 = 'XXX_321'
    channel_broad3 = 'XXX_213'
    body = 'wildcard channel prefix'

    nginx_run_server(config.merge(:wildcard_channel_max_qtd => 2)) do |conf|
      EventMachine.run do
        pub = EventMachine::HttpRequest.new(nginx_address + '/pub?id=' + channel.to_s ).post :head => headers, :body => body
        pub.callback do
          sub_1 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s + '/' + channel_broad1 + '/' + channel_broad2  + '/' + channel_broad3).get :head => headers
          sub_1.callback do |chunk|
            expect(sub_1).to be_http_status(403).without_body
            sub_2 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s + '/' + channel_broad1 + '/' + channel_broad2).get :head => headers
            sub_2.stream do
              EventMachine.stop
            end
          end
        end
      end
    end
  end
end
