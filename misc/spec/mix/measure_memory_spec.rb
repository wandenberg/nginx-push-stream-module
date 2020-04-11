require 'spec_helper'

describe "Measure Memory" do
  let(:config) do
    {
      :shared_memory_size => "2m",
      :message_ttl => "60m",
      :max_messages_stored_per_channel => nil,
      :keepalive_requests => 15000,
      :header_template => nil,
      :message_template => nil,
      :footer_template => nil,
      :ping_message_interval => nil
    }
  end

  message_estimate_size = 168
  channel_estimate_size = 270
  subscriber_estimate_size = 400
  subscriber_estimate_system_size = 8384

  it "should check message size" do
    channel = 'ch_test_message_size'
    body = '1'

    nginx_run_server(config, :timeout => 30) do |conf|
      shared_size = conf.shared_memory_size.to_i * 1024 * 1024

      post_channel_message = "POST /pub?id=#{channel} HTTP/1.1\r\nHost: localhost\r\nContent-Length: #{body.size}\r\n\r\n#{body}"
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
          expect(pub_2).to be_http_status(200).with_body

          resp = JSON.parse(pub_2.response)
          expected_message = shared_size / (message_estimate_size + body.size)
          expect(resp["published_messages"].to_i).to be_within(80).of(expected_message)
          EventMachine.stop
        end
      end
    end
  end

  it "should check channel size" do
    body = '1'

    nginx_run_server(config, :timeout => 150) do |conf|
      shared_size = conf.shared_memory_size.to_i * 1024 * 1024

      socket = open_socket(nginx_host, nginx_port)

      channel = 1000
      while (true) do
        post_channel_message = "POST /pub?id=#{channel} HTTP/1.1\r\nHost: localhost\r\nContent-Length: #{body.size}\r\n\r\n#{body}"
        socket.print(post_channel_message)
        resp_headers, resp_body = read_response_on_socket(socket, "}\r\n")
        break unless resp_headers.match(/200 OK/)
        channel += 1
      end
      socket.close

      EventMachine.run do
        pub_2 = EventMachine::HttpRequest.new(nginx_address + '/channels-stats').get
        pub_2.callback do
          expect(pub_2).to be_http_status(200).with_body

          resp = JSON.parse(pub_2.response)
          expected_channel = (shared_size - ((body.size + message_estimate_size) * resp["published_messages"].to_i)) / (channel_estimate_size + 4) # 4 channel id size
          expect(resp["channels"].to_i).to be_within(10).of(expected_channel)
          EventMachine.stop
        end
      end
    end
  end

  it "should check subscriber size" do
    nginx_run_server(config.merge({:shared_memory_size => "128k", :header_template => "H"})) do |conf|
      shared_size = conf.shared_memory_size.to_i * 1024 #shm size is in kbytes for this test

      EventMachine.run do
        subscriber_in_loop(1000, headers) do
          pub_2 = EventMachine::HttpRequest.new(nginx_address + '/channels-stats').get :head => headers
          pub_2.callback do
            expect(pub_2).to be_http_status(200).with_body

            resp = JSON.parse(pub_2.response)
            expected_subscriber = (shared_size - ((channel_estimate_size + 4) * resp["channels"].to_i)) / subscriber_estimate_size # 4 channel id size
            expect(resp["subscribers"].to_i).to be_within(10).of(expected_subscriber)
            EventMachine.stop
          end
        end
      end
    end
  end

  it "should check subscriber system size" do
    channel = 'ch_test_subscriber_system_size'

    nginx_run_server(config.merge({:header_template => "H", :master_process => 'off', :daemon => 'off'}), :timeout => 15) do |conf|
      #warming up
      EventMachine.run do
        sub = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_i.to_s).get :head => headers
        sub.stream do |chunk|
          EventMachine.stop
        end
      end

      per_subscriber = 0
      EventMachine.run do
        memory_1 = `ps -o rss= -p #{File.read conf.pid_file}`.split(' ')[0].to_i
        subscriber_in_loop_with_limit(channel, headers, 1000, 1099) do
          sleep(1)
          memory_2 = `ps -o rss= -p #{File.read conf.pid_file}`.split(' ')[0].to_i

          per_subscriber = ((memory_2 - memory_1).to_f / 100) * 1000

          EventMachine.stop
        end
      end

      expect(per_subscriber).to be_within(100).of(subscriber_estimate_system_size)
    end
  end
end

def subscriber_in_loop(channel, headers, &block)
  called = false
  sub = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_i.to_s).get :head => headers
  sub.stream do |chunk|
    next if called
    called = true
    subscriber_in_loop(channel.to_i + 1, headers, &block)
  end
  sub.callback do
    block.call
  end
end

def subscriber_in_loop_with_limit(channel, headers, start, limit, &block)
  called = false
  sub = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_i.to_s).get :head => headers
  sub.stream do |chunk|
    if start == limit
      block.call
    else
      next if called
      called = true
      subscriber_in_loop_with_limit(channel, headers, start + 1, limit, &block)
    end
  end
  sub.callback do
    block.call
  end
end
