require 'spec_helper'

describe "Broadcast Properties" do
  let(:config) do
    {
      :authorized_channels_only => "on",
      :header_template => 'connected',
      :broadcast_channel_prefix => "XXX_"
    }
  end

  it "should identify broadcast channels by prefix" do
    channel = 'ch_test_broadcast_channel_prefix'
    channel_broad = 'XXX_123'
    channel_broad_fail = 'YYY_123'

    body = 'broadcast channel prefix'

    nginx_run_server(config, :timeout => 5) do |conf|
      EventMachine.run do
        pub = EventMachine::HttpRequest.new(nginx_address + '/pub?id=' + channel.to_s ).post :head => headers, :body => body, :timeout => 30
        pub.callback do
          sub_1 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s + '/' + channel_broad_fail).get :head => headers, :timeout => 60
          sub_1.callback do |chunk|
            sub_1.response_header.status.should eql(403)

            sub_2 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s + '/' + channel_broad).get :head => headers, :timeout => 60
            sub_2.stream do |chunk2|
              chunk2.should eql("#{conf.header_template}\r\n")
              EventMachine.stop
            end
          end
        end
      end
    end
  end

  it "should limit the number of broadcast channels in the same request" do
    channel = 'ch_test_broadcast_channel_max_qtd'
    channel_broad1 = 'XXX_123'
    channel_broad2 = 'XXX_321'
    channel_broad3 = 'XXX_213'
    body = 'broadcast channel prefix'

    nginx_run_server(config.merge(:broadcast_channel_max_qtd => 2), :timeout => 5) do |conf|
      EventMachine.run do
        pub = EventMachine::HttpRequest.new(nginx_address + '/pub?id=' + channel.to_s ).post :head => headers, :body => body, :timeout => 30
        pub.callback do
          sub_1 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s + '/' + channel_broad1 + '/' + channel_broad2  + '/' + channel_broad3).get :head => headers, :timeout => 60
          sub_1.callback do |chunk|
            sub_1.response_header.status.should eql(403)
            sub_2 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s + '/' + channel_broad1 + '/' + channel_broad2).get :head => headers, :timeout => 60
            sub_2.stream do
              EventMachine.stop
            end
          end
        end
      end
    end
  end
end
