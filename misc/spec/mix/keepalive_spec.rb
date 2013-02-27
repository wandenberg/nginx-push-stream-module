require 'spec_helper'

describe "Keepalive" do
  let(:config) do
    {
      :shared_memory_size => '256m',
      :keepalive => "on",
      :header_template => '',
      :message_template => '~text~',
      :footer_template => ''
    }
  end

  it "should create many channels on the same socket" do
    channel = 'ch_test_create_many_channels_'
    body = 'channel started'
    channels_to_be_created = 4000

    nginx_run_server(config, :timeout => 25) do |conf|
      0.step(channels_to_be_created - 1, 500) do |i|
        socket = open_socket(nginx_host, nginx_port)
        1.upto(500) do |j|
          headers, body = post_in_socket("/pub?id=#{channel}#{i + j}", body, socket, "}\r\n")
          headers.should include("HTTP/1.1 200 OK")
        end
        socket.close
      end
    end
  end

  it "should execute different operations using the same socket" do
    channel = 'ch_test_different_operation_with_keepalive'
    content = 'message to be sent'

    nginx_run_server(config) do |conf|
      socket = open_socket(nginx_host, nginx_port)

      headers, body = get_in_socket("/pub", socket)
      body.should eql("")
      headers.should include("No channel id provided.")

      headers, body = post_in_socket("/pub?id=#{channel}", content, socket, "}\r\n")
      body.should eql("{\"channel\": \"#{channel}\", \"published_messages\": \"1\", \"stored_messages\": \"1\", \"subscribers\": \"0\"}\r\n")

      headers, body = get_in_socket("/channels-stats", socket)

      body.should match_the_pattern(/"channels": "1", "broadcast_channels": "0", "published_messages": "1", "subscribers": "0", "uptime": "[0-9]*", "by_worker": \[\r\n/)
      body.should match_the_pattern(/\{"pid": "[0-9]*", "subscribers": "0", "uptime": "[0-9]*"\}/)

      headers, body = get_in_socket("/pub?id=#{channel}", socket)
      body.should eql("{\"channel\": \"#{channel}\", \"published_messages\": \"1\", \"stored_messages\": \"1\", \"subscribers\": \"0\"}\r\n")

      socket.close
    end
  end
end
