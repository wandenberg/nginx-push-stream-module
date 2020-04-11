require 'spec_helper'

describe "Keepalive" do
  let(:config) do
    {
      :shared_memory_size => '256m',
      :keepalive_requests => 500,
      :header_template => '',
      :message_template => '~text~',
      :footer_template => '',
      :publisher_mode => 'admin'
    }
  end

  it "should create many channels on the same socket" do
    channel = 'ch_test_create_many_channels_'
    body = 'channel started'
    channels_to_be_created = 4000

    nginx_run_server(config, :timeout => 25) do |conf|
      http_single = Net::HTTP::Persistent.new name: "single_channel"
      http_double = Net::HTTP::Persistent.new name: "double_channel"
      uri = URI.parse nginx_address

      0.step(channels_to_be_created - 1, 500) do |i|
        1.upto(500) do |j|
          post_single = Net::HTTP::Post.new "/pub?id=#{channel}#{i + j}"
          post_single.body = body
          response_single = http_single.request(uri, post_single)
          expect(response_single.code).to eql("200")
          expect(response_single.body).to eql(%({"channel": "#{channel}#{i + j}", "published_messages": 1, "stored_messages": 1, "subscribers": 0}\r\n))

          post_double = Net::HTTP::Post.new "/pub?id=#{channel}#{i + j}/#{channel}#{i}_#{j}"
          post_double.body = body
          response_double = http_double.request(uri, post_double)
          expect(response_double.code).to eql("200")
          expect(response_double.body).to match_the_pattern(/"hostname": "[^"]*", "time": "\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}", "channels": #{(i + j) * 2}, "wildcard_channels": 0, "uptime": [0-9]*, "infos": \[\r\n/)
          expect(response_double.body).to match_the_pattern(/"channel": "#{channel}#{i + j}", "published_messages": 2, "stored_messages": 2, "subscribers": 0},\r\n/)
          expect(response_double.body).to match_the_pattern(/"channel": "#{channel}#{i}_#{j}", "published_messages": 1, "stored_messages": 1, "subscribers": 0}\r\n/)
        end
      end
    end
  end

  it "should create many channels on the same socket without info on response" do
    channel = 'ch_test_create_many_channels_'
    body = 'channel started'
    channels_to_be_created = 4000

    nginx_run_server(config.merge({:channel_info_on_publish => "off"}), :timeout => 25) do |conf|
      uri = URI.parse nginx_address
      0.step(channels_to_be_created - 1, 500) do |i|
        http = Net::HTTP::Persistent.new
        1.upto(500) do |j|
          post = Net::HTTP::Post.new "/pub?id=#{channel}#{i + j}"
          post.body = body
          response = http.request(uri, post)
          expect(response.code).to eql("200")
          expect(response.body).to eql("")
        end
      end
    end
  end

  it "should execute different operations using the same socket" do
    channel = 'ch_test_different_operation_with_keepalive'
    content = 'message to be sent'

    nginx_run_server(config) do |conf|
      socket = open_socket(nginx_host, nginx_port)

      headers, body = get_in_socket("/pub", socket)
      expect(body).to eql("")
      expect(headers).to include("No channel id provided.")

      headers, body = post_in_socket("/pub?id=#{channel}", content, socket, {:wait_for => "}\r\n"})
      expect(body).to eql("{\"channel\": \"#{channel}\", \"published_messages\": 1, \"stored_messages\": 1, \"subscribers\": 0}\r\n")

      headers, body = get_in_socket("/channels-stats", socket)

      expect(body).to match_the_pattern(/"channels": 1, "wildcard_channels": 0, "published_messages": 1, "stored_messages": 1, "messages_in_trash": 0, "channels_in_delete": 0, "channels_in_trash": 0, "subscribers": 0, "uptime": [0-9]*, "by_worker": \[\r\n/)
      expect(body).to match_the_pattern(/\{"pid": "[0-9]*", "subscribers": 0, "uptime": [0-9]*\}/)

      socket.print("DELETE /pub?id=#{channel}_1 HTTP/1.1\r\nHost: test\r\n\r\n")
      headers, body = read_response_on_socket(socket)
      expect(headers).to include("HTTP/1.1 404 Not Found")

      headers, body = get_in_socket("/channels-stats?id=ALL", socket)

      expect(body).to match_the_pattern(/"hostname": "[^"]*", "time": "\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}", "channels": 1, "wildcard_channels": 0, "uptime": [0-9]*, "infos": \[\r\n/)
      expect(body).to match_the_pattern(/"channel": "#{channel}", "published_messages": 1, "stored_messages": 1, "subscribers": 0}\r\n/)

      headers, body = get_in_socket("/pub?id=#{channel}", socket)
      expect(body).to eql("{\"channel\": \"#{channel}\", \"published_messages\": 1, \"stored_messages\": 1, \"subscribers\": 0}\r\n")

      headers, body = post_in_socket("/pub?id=#{channel}/broad_#{channel}", content, socket, {:wait_for => "}\r\n"})
      expect(body).to match_the_pattern(/"hostname": "[^"]*", "time": "\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}", "channels": 1, "wildcard_channels": 1, "uptime": [0-9]*, "infos": \[\r\n/)
      expect(body).to match_the_pattern(/"channel": "#{channel}", "published_messages": 2, "stored_messages": 2, "subscribers": 0},\r\n/)
      expect(body).to match_the_pattern(/"channel": "broad_#{channel}", "published_messages": 1, "stored_messages": 1, "subscribers": 0}\r\n/)

      headers, body = get_in_socket("/channels-stats?id=#{channel}", socket)
      expect(body).to match_the_pattern(/{"channel": "#{channel}", "published_messages": 2, "stored_messages": 2, "subscribers": 0}\r\n/)

      socket.print("DELETE /pub?id=#{channel} HTTP/1.1\r\nHost: test\r\n\r\n")
      headers, body = read_response_on_socket(socket)
      expect(headers).to include("X-Nginx-PushStream-Explain: Channel deleted.")

      socket.close
    end
  end

  it "should accept subscribe many times using the same socket" do
    channel = 'ch_test_subscribe_with_keepalive'
    body_prefix = 'message to be sent'
    get_messages = "GET /sub/#{channel} HTTP/1.1\r\nHost: test\r\n\r\n"

    nginx_run_server(config.merge(:store_messages => 'off', :subscriber_mode => 'long-polling'), :timeout => 5) do |conf|
      socket = open_socket(nginx_host, nginx_port)
      socket_pub = open_socket(nginx_host, nginx_port)

      1.upto(500) do |j|
        socket.print(get_messages)
        post_in_socket("/pub?id=#{channel}", "#{body_prefix} #{j.to_s.rjust(3, '0')}", socket_pub, {:wait_for => "}\r\n"})
        headers, body = read_response_on_socket(socket, "\r\n0\r\n\r\n")
        expect(body).to eql("16\r\nmessage to be sent #{j.to_s.rjust(3, '0')}\r\n0\r\n\r\n")
      end

      socket.close
      socket_pub.close
    end
  end
end
