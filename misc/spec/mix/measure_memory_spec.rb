require 'spec_helper'

describe "Measure Memory" do
  let(:config) do
    {
      :shared_memory_size => "2m",
      :shared_memory_cleanup_objects_ttl => "60m",
      :message_ttl => "60m",
      :max_messages_stored_per_channel => nil,
      :keepalive => "on",
      :header_template => nil,
      :message_template => nil,
      :footer_template => nil,
      :ping_message_interval => nil
    }
  end

  message_estimate_size = 168
  channel_estimate_size = 270
  subscriber_estimate_size = 160
  subscriber_estimate_system_size = 7000

  it "should check message size" do
    channel = 'ch_test_message_size'
    body = '1'

    nginx_run_server(config) do |conf|
      shared_size = conf.shared_memory_size.to_i * 1024 * 1024

      post_channel_message = "POST /pub?id=#{channel} HTTP/1.0\r\nContent-Length: #{body.size}\r\n\r\n#{body}"
      socket = open_socket(nginx_host, nginx_port)

      while (true) do
        socket.print(post_channel_message)
        resp_headers, resp_body = read_response_on_socket(socket, "}\r\n")
        break unless resp_headers.match(/200 OK/)
      end
      socket.close

      EventMachine.run do
        pub_2 = EventMachine::HttpRequest.new(nginx_address + '/channels-stats').get
        pub_2.callback do
          pub_2.should be_http_status(200).with_body

          resp = JSON.parse(pub_2.response)
          expected_message = shared_size / (message_estimate_size + body.size)
          resp["published_messages"].to_i.should be_within(80).of(expected_message)
          EventMachine.stop
        end
      end
    end
  end

  it "should check channel size" do
    body = '1'

    nginx_run_server(config, :timeout => 1500) do |conf|
      shared_size = conf.shared_memory_size.to_i * 1024 * 1024

      socket = open_socket(nginx_host, nginx_port)

      channel = 1000
      while (true) do
        post_channel_message = "POST /pub?id=#{channel} HTTP/1.0\r\nContent-Length: #{body.size}\r\n\r\n#{body}"
        socket.print(post_channel_message)
        resp_headers, resp_body = read_response_on_socket(socket, "}\r\n")
        break unless resp_headers.match(/200 OK/)
        channel += 1
      end
      socket.close

      EventMachine.run do
        pub_2 = EventMachine::HttpRequest.new(nginx_address + '/channels-stats').get
        pub_2.callback do
          pub_2.should be_http_status(200).with_body

          resp = JSON.parse(pub_2.response)
          expected_channel = (shared_size - ((body.size + message_estimate_size) * resp["published_messages"].to_i)) / (channel_estimate_size + 4) # 4 channel id size
          resp["channels"].to_i.should be_within(10).of(expected_channel)
          EventMachine.stop
        end
      end
    end
  end

  it "should check subscriber size" do
    nginx_run_server(config.merge({:shared_memory_size => "300k", :header_template => "H"})) do |conf|
      shared_size = conf.shared_memory_size.to_i * 1024 #shm size is in kbytes for this test

      EventMachine.run do
        subscriber_in_loop(1000, headers) do
          pub_2 = EventMachine::HttpRequest.new(nginx_address + '/channels-stats').get :head => headers
          pub_2.callback do
            pub_2.should be_http_status(200).with_body

            resp = JSON.parse(pub_2.response)
            expected_subscriber = (shared_size - ((channel_estimate_size + 4) * resp["channels"].to_i)) / subscriber_estimate_size # 4 channel id size
            resp["subscribers"].to_i.should be_within(10).of(expected_subscriber)
            EventMachine.stop
          end
        end
      end
    end
  end

  it "should check subscriber system size" do
    channel = 'ch_test_subscriber_system_size'
    body = '1'

    nginx_run_server(config.merge({:header_template => "H", :master_process => 'off', :daemon => 'off'}), :timeout => 15) do |conf|
      #warming up
      EventMachine.run do
        sub = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_i.to_s).get :head => headers, :body => body
        sub.stream do |chunk|
          EventMachine.stop
        end
      end

      per_subscriber = 0
      EventMachine.run do
        memory_1 = `ps -eo rss,cmd | grep -E 'ngin[xX] -c #{conf.configuration_filename}'`.split(' ')[0].to_i
        subscriber_in_loop_with_limit(channel, headers, body, 1000, 1499) do
          sleep(1)
          memory_2 = `ps -eo rss,cmd | grep -E 'ngin[xX] -c #{conf.configuration_filename}'`.split(' ')[0].to_i

          per_subscriber = ((memory_2 - memory_1).to_f / 500) * 1000

          EventMachine.stop
        end
      end

      per_subscriber.should be_within(100).of(subscriber_estimate_system_size)
    end
  end
end

def subscriber_in_loop(channel, headers, &block)
  sub = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_i.to_s).get :head => headers
  sub.stream do |chunk|
    subscriber_in_loop(channel.to_i + 1, headers) do
      yield block
    end
  end
  sub.callback do
    block.call
  end
end

def subscriber_in_loop_with_limit(channel, headers, body, start, limit, &block)
  sub = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_i.to_s).get :head => headers, :body => body
  sub.stream do |chunk|
    if start == limit
      block.call
      EventMachine.stop
    end
    subscriber_in_loop_with_limit(channel, headers, body, start + 1, limit) do
      yield block
    end
  end
  sub.callback do
    block.call
  end
end
