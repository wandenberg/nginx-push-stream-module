require 'spec_helper'

describe "Subscriber WebSocket" do
  let(:config) do
    {
      :header_template => nil,
      :message_template => nil,
      :footer_template => nil,
      :extra_location => %q{
        location ~ /ws/(.*)? {
            # activate websocket mode for this location
            push_stream_websocket;

            # positional channel path
            set $push_stream_channels_path          $1;
        }
      }
    }
  end

  it "should check accepted methods" do
    nginx_run_server(config) do |conf|
      EventMachine.run do
        multi = EventMachine::MultiRequest.new

        multi.add(:a, EventMachine::HttpRequest.new(nginx_address + '/ws/ch_test_accepted_methods_1').head)
        multi.add(:b, EventMachine::HttpRequest.new(nginx_address + '/ws/ch_test_accepted_methods_2').put(:body => 'body'))
        multi.add(:c, EventMachine::HttpRequest.new(nginx_address + '/ws/ch_test_accepted_methods_3').post)
        multi.add(:d, EventMachine::HttpRequest.new(nginx_address + '/ws/ch_test_accepted_methods_4').delete)
        multi.add(:e, EventMachine::HttpRequest.new(nginx_address + '/ws/ch_test_accepted_methods_5').get)


        multi.callback do
          multi.responses[:callback].length.should eql(5)

          multi.responses[:callback][:a].should be_http_status(405)
          multi.responses[:callback][:a].req.method.should eql("HEAD")
          multi.responses[:callback][:a].response_header['ALLOW'].should eql("GET")

          multi.responses[:callback][:b].should be_http_status(405)
          multi.responses[:callback][:b].req.method.should eql("PUT")
          multi.responses[:callback][:b].response_header['ALLOW'].should eql("GET")

          multi.responses[:callback][:c].should be_http_status(405)
          multi.responses[:callback][:c].req.method.should eql("POST")
          multi.responses[:callback][:c].response_header['ALLOW'].should eql("GET")

          multi.responses[:callback][:d].should be_http_status(405)
          multi.responses[:callback][:d].req.method.should eql("DELETE")
          multi.responses[:callback][:d].response_header['ALLOW'].should eql("GET")

          multi.responses[:callback][:e].should_not be_http_status(405)
          multi.responses[:callback][:e].req.method.should eql("GET")

          EventMachine.stop
        end
      end
    end
  end

  it "should check mandatory headers" do
    channel = 'ch_test_check_mandatory_headers'
    request = "GET /ws/#{channel}.b1 HTTP/1.0\r\n"

    nginx_run_server(config) do |conf|
      socket = open_socket(nginx_host, nginx_port)
      socket.print("#{request}\r\n")
      headers, body = read_response_on_socket(socket)
      body.should eql("")
      headers.should match_the_pattern(/Don't have at least one of the mandatory headers: Connection, Upgrade, Sec-WebSocket-Key and Sec-WebSocket-Version/)

      request << "Connection: Upgrade\r\n"

      socket = open_socket(nginx_host, nginx_port)
      socket.print("#{request}\r\n")
      headers, body = read_response_on_socket(socket)
      body.should eql("")
      headers.should match_the_pattern(/Don't have at least one of the mandatory headers: Connection, Upgrade, Sec-WebSocket-Key and Sec-WebSocket-Version/)

      request << "Sec-WebSocket-Key: /mQoZf6pRiv8+6o72GncLQ==\r\n"

      socket = open_socket(nginx_host, nginx_port)
      socket.print("#{request}\r\n")
      headers, body = read_response_on_socket(socket)
      body.should eql("")
      headers.should match_the_pattern(/Don't have at least one of the mandatory headers: Connection, Upgrade, Sec-WebSocket-Key and Sec-WebSocket-Version/)

      request << "Upgrade: websocket\r\n"

      socket = open_socket(nginx_host, nginx_port)
      socket.print("#{request}\r\n")
      headers, body = read_response_on_socket(socket)
      body.should eql("")
      headers.should match_the_pattern(/Don't have at least one of the mandatory headers: Connection, Upgrade, Sec-WebSocket-Key and Sec-WebSocket-Version/)

      request << "Sec-WebSocket-Version: 8\r\n"

      socket = open_socket(nginx_host, nginx_port)
      socket.print("#{request}\r\n")
      headers, body = read_response_on_socket(socket)
      body.should eql("")
      headers.should_not match_the_pattern(/Don't have at least one of the mandatory headers: Connection, Upgrade, Sec-WebSocket-Key and Sec-WebSocket-Version/)
      headers.should match_the_pattern(/HTTP\/1\.1 101 Switching Protocols/)
    end
  end

  it "should check supported versions" do
    channel = 'ch_test_supported_versions'
    request = "GET /ws/#{channel}.b1 HTTP/1.0\r\nConnection: Upgrade\r\nSec-WebSocket-Key: /mQoZf6pRiv8+6o72GncLQ==\r\nUpgrade: websocket\r\n"

    nginx_run_server(config) do |conf|
      socket = open_socket(nginx_host, nginx_port)
      socket.print("#{request}Sec-WebSocket-Version: 7\r\n\r\n")
      headers, body = read_response_on_socket(socket)
      body.should eql("")
      headers.should match_the_pattern(/Sec-WebSocket-Version: 8, 13/)
      headers.should match_the_pattern(/X-Nginx-PushStream-Explain: Version not supported. Supported versions: 8, 13/)

      socket = open_socket(nginx_host, nginx_port)
      socket.print("#{request}Sec-WebSocket-Version: 8\r\n\r\n")
      headers, body = read_response_on_socket(socket)
      body.should eql("")
      headers.should_not match_the_pattern(/Sec-WebSocket-Version: 8, 13/)
      headers.should_not match_the_pattern(/X-Nginx-PushStream-Explain: Version not supported. Supported versions: 8, 13/)

      socket = open_socket(nginx_host, nginx_port)
      socket.print("#{request}Sec-WebSocket-Version: 13\r\n\r\n")
      headers, body = read_response_on_socket(socket)
      body.should eql("")
      headers.should_not match_the_pattern(/Sec-WebSocket-Version: 8, 13/)
      headers.should_not match_the_pattern(/X-Nginx-PushStream-Explain: Version not supported. Supported versions: 8, 13/)
    end
  end

  it "should check response headers" do
    channel = 'ch_test_response_headers'
    request = "GET /ws/#{channel}.b1 HTTP/1.0\r\nConnection: Upgrade\r\nSec-WebSocket-Key: /mQoZf6pRiv8+6o72GncLQ==\r\nUpgrade: websocket\r\nSec-WebSocket-Version: 8\r\n"

    nginx_run_server(config) do |conf|
      socket = open_socket(nginx_host, nginx_port)
      socket.print("#{request}\r\n")
      headers, body = read_response_on_socket(socket)
      body.should eql("")
      headers.should match_the_pattern(/HTTP\/1\.1 101 Switching Protocols/)
      headers.should match_the_pattern(/Sec-WebSocket-Accept: RaIOIcQ6CBoc74B9EKdH0avYZnw=/)
      headers.should match_the_pattern(/Upgrade: WebSocket/)
      headers.should match_the_pattern(/Connection: Upgrade/)
    end
  end

  it "should receive header template" do
    channel = 'ch_test_receive_header_template'
    request = "GET /ws/#{channel}.b1 HTTP/1.0\r\nConnection: Upgrade\r\nSec-WebSocket-Key: /mQoZf6pRiv8+6o72GncLQ==\r\nUpgrade: websocket\r\nSec-WebSocket-Version: 8\r\n"

    nginx_run_server(config.merge(:header_template => "HEADER_TEMPLATE")) do |conf|
      socket = open_socket(nginx_host, nginx_port)
      socket.print("#{request}\r\n")
      sleep(0.5)
      headers, body = read_response_on_socket(socket, 'TEMPLATE')
      body.should eql("\201\017HEADER_TEMPLATE")
    end
  end

  it "should receive ping frame" do
    channel = 'ch_test_receive_ping_frame'
    request = "GET /ws/#{channel}.b1 HTTP/1.0\r\nConnection: Upgrade\r\nSec-WebSocket-Key: /mQoZf6pRiv8+6o72GncLQ==\r\nUpgrade: websocket\r\nSec-WebSocket-Version: 8\r\n"

    nginx_run_server(config.merge(:ping_message_interval => '1s')) do |conf|
      socket = open_socket(nginx_host, nginx_port)
      socket.print("#{request}\r\n")
      headers, body = read_response_on_socket(socket)
      #wait for ping message
      sleep(1)
      body, dummy = read_response_on_socket(socket)
      body.should eql("\211\000")
    end
  end

  it "should receive close frame" do
    channel = 'ch_test_receive_close_frame'
    request = "GET /ws/#{channel}.b1 HTTP/1.0\r\nConnection: Upgrade\r\nSec-WebSocket-Key: /mQoZf6pRiv8+6o72GncLQ==\r\nUpgrade: websocket\r\nSec-WebSocket-Version: 8\r\n"

    nginx_run_server(config.merge(:subscriber_connection_ttl => '1s')) do |conf|
      socket = open_socket(nginx_host, nginx_port)
      socket.print("#{request}\r\n")
      headers, body = read_response_on_socket(socket)
      #wait for disconnect
      sleep(1)
      body, dummy = read_response_on_socket(socket, "\210\000")
      body.should eql("\210\000")
    end
  end

  it "should receive footer template" do
    channel = 'ch_test_receive_footer_template'
    request = "GET /ws/#{channel}.b1 HTTP/1.0\r\nConnection: Upgrade\r\nSec-WebSocket-Key: /mQoZf6pRiv8+6o72GncLQ==\r\nUpgrade: websocket\r\nSec-WebSocket-Version: 8\r\n"

    nginx_run_server(config.merge(:subscriber_connection_ttl => '1s', :footer_template => "FOOTER_TEMPLATE")) do |conf|
      socket = open_socket(nginx_host, nginx_port)
      socket.print("#{request}\r\n")
      headers, body = read_response_on_socket(socket)
      #wait for disconnect
      sleep(1.5)
      body, dummy = read_response_on_socket(socket, "\210\000")
      body.should eql("\201\017FOOTER_TEMPLATE\210\000")
    end
  end

  it "should check frames for messages with less than 125 bytes" do
    channel = 'ch_test_receive_message_length_less_than_125'
    request = "GET /ws/#{channel}.b1 HTTP/1.0\r\nConnection: Upgrade\r\nSec-WebSocket-Key: /mQoZf6pRiv8+6o72GncLQ==\r\nUpgrade: websocket\r\nSec-WebSocket-Version: 8\r\n"

    nginx_run_server(config) do |conf|
      socket = open_socket(nginx_host, nginx_port)
      socket.print("#{request}\r\n")
      headers, body = read_response_on_socket(socket)

      publish_message(channel, {}, "Hello")

      body, dummy = read_response_on_socket(socket, "Hello")
      body.should eql("\201\005Hello")
    end
  end

  it "should check frames for messages with more than 125 and less than 65535 bytes" do
    message = ""
    channel = 'ch_test_receive_message_length_more_than_125_less_then_65535'
    request = "GET /ws/#{channel}.b1 HTTP/1.0\r\nConnection: Upgrade\r\nSec-WebSocket-Key: /mQoZf6pRiv8+6o72GncLQ==\r\nUpgrade: websocket\r\nSec-WebSocket-Version: 8\r\n"

    65535.times { message << "a" }

    nginx_run_server(config.merge(:client_max_body_size => '65k', :client_body_buffer_size => '65k')) do |conf|
      publish_message(channel, {}, message)

      socket = open_socket(nginx_host, nginx_port)
      socket.print("#{request}\r\n")
      headers, body = read_response_on_socket(socket, "aaa")
      body.should match_the_pattern(/^\201~\377\377aaa/)
    end
  end

  it "should check frames for messages with more than 65535 bytes" do
    message = ""
    channel = 'ch_test_receive_message_length_more_than_65535'
    request = "GET /ws/#{channel}.b1 HTTP/1.0\r\nConnection: Upgrade\r\nSec-WebSocket-Key: /mQoZf6pRiv8+6o72GncLQ==\r\nUpgrade: websocket\r\nSec-WebSocket-Version: 8\r\n"

    65536.times { message << "a" }

    nginx_run_server(config.merge(:client_max_body_size => '70k', :client_body_buffer_size => '70k')) do |conf|
      publish_message(channel, {}, message)

      socket = open_socket(nginx_host, nginx_port)
      socket.print("#{request}\r\n")
      headers, body = read_response_on_socket(socket, "aaa")
      body.should match_the_pattern(/^\201\177\000\000\000\000\000\001\000\000aaa/)
    end
  end

  it "should accept same message template in different locations" do
    channel = 'ch_test_same_message_template_different_locations'
    body = 'body'

    nginx_run_server(config.merge(:message_template => '{\"text\":\"~text~\"}', :subscriber_connection_ttl => '1s')) do |conf|
      publish_message(channel, {}, body)

      request_1 = "GET /ws/#{channel}.b1 HTTP/1.0\r\nConnection: Upgrade\r\nSec-WebSocket-Key: /mQoZf6pRiv8+6o72GncLQ==\r\nUpgrade: websocket\r\nSec-WebSocket-Version: 8\r\n"
      request_2 = "GET /sub/#{channel}.b1 HTTP/1.0\r\n"

      socket_1 = open_socket(nginx_host, nginx_port)
      socket_1.print("#{request_1}\r\n")
      headers_1, body_1 = read_response_on_socket(socket_1, '}')
      body_1.should eql("\201\017{\"text\":\"#{body}\"}")

      socket_2 = open_socket(nginx_host, nginx_port)
      socket_2.print("#{request_2}\r\n")
      headers_2, body_2 = read_response_on_socket(socket_2, '}')
      body_2.should eql("{\"text\":\"#{body}\"}\r\n")
    end
  end

  it "should accept publish message on same stream" do
    configuration = config.merge({
      :message_template => '{\"channel\":\"~channel~\", \"id\":\"~id~\", \"message\":\"~text~\"}',
      :extra_location => %q{
        location ~ /ws/(.*)? {
            # activate websocket mode for this location
            push_stream_websocket;

            # positional channel path
            set $push_stream_channels_path          $1;

            # allow subscriber to publish
            push_stream_websocket_allow_publish on;
            # store messages
            push_stream_store_messages on;
        }
      }
    })

    channel = 'ch_test_publish_message_same_stream'
    frame = "%c%c%c%c%c%c%c%c%c%c%c" % [0x81, 0x85, 0xBD, 0xD0, 0xE5, 0x2A, 0xD5, 0xB5, 0x89, 0x46, 0xD2] #send 'hello' text

    request = "GET /ws/#{channel}.b1 HTTP/1.0\r\nConnection: Upgrade\r\nSec-WebSocket-Key: /mQoZf6pRiv8+6o72GncLQ==\r\nUpgrade: websocket\r\nSec-WebSocket-Version: 8\r\n"

    nginx_run_server(configuration) do |conf|
      socket = open_socket(nginx_host, nginx_port)
      socket.print("#{request}\r\n")
      headers, body = read_response_on_socket(socket)
      socket.print(frame)

      body, dummy = read_response_on_socket(socket)
      body.should eql("\211\000")

      body, dummy = read_response_on_socket(socket)
      body.should eql("\x81N{\"channel\":\"ch_test_publish_message_same_stream\", \"id\":\"1\", \"message\":\"hello\"}")


      EventMachine.run do
        pub = EventMachine::HttpRequest.new(nginx_address + '/channels-stats?id=' + channel.to_s).get :timeout => 30
        pub.callback do
          pub.should be_http_status(200).with_body
          response = JSON.parse(pub.response)
          response["channel"].to_s.should eql(channel)
          response["published_messages"].to_i.should eql(1)
          response["stored_messages"].to_i.should eql(1)
          response["subscribers"].to_i.should eql(1)
          EventMachine.stop
        end
      end

      EventMachine.run do
        sub = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s + '.b1').get :timeout => 30
        sub.stream do |chunk|
          line = JSON.parse(chunk.split("\r\n")[0])
          line['channel'].should eql(channel.to_s)
          line['message'].should eql('hello')
          line['id'].to_i.should eql(1)
          EventMachine.stop
        end
      end
    end
  end

  it "should accept pong message" do
    channel = 'ch_test_accept_pong_message'
    frame = "%c%c%c%c%c%c" % [0x8A, 0x80, 0xBD, 0xD0, 0xE5, 0x2A] #send 'pong' frame

    request = "GET /ws/#{channel}.b1 HTTP/1.0\r\nConnection: Upgrade\r\nSec-WebSocket-Key: /mQoZf6pRiv8+6o72GncLQ==\r\nUpgrade: websocket\r\nSec-WebSocket-Version: 8\r\n"

    nginx_run_server(config) do |conf|
      socket = open_socket(nginx_host, nginx_port)
      socket.print("#{request}\r\n")
      headers, body = read_response_on_socket(socket)
      socket.print(frame)

      EventMachine.run do
        pub = EventMachine::HttpRequest.new(nginx_address + '/channels-stats?id=' + channel.to_s).get :timeout => 30
        pub.callback do
          pub.should be_http_status(200).with_body
          response = JSON.parse(pub.response)
          response["channel"].to_s.should eql(channel)
          response["published_messages"].to_i.should eql(0)
          response["stored_messages"].to_i.should eql(0)
          response["subscribers"].to_i.should eql(1)
          EventMachine.stop
        end
      end
    end
  end

  it "should accept close message" do
    channel = 'ch_test_accept_close_message'
    frame = "%c%c%c%c%c%c" % [0x88, 0x80, 0xBD, 0xD0, 0xE5, 0x2A] #send 'close' frame

    request = "GET /ws/#{channel}.b1 HTTP/1.0\r\nConnection: Upgrade\r\nSec-WebSocket-Key: /mQoZf6pRiv8+6o72GncLQ==\r\nUpgrade: websocket\r\nSec-WebSocket-Version: 8\r\n"

    nginx_run_server(config) do |conf|
      socket = open_socket(nginx_host, nginx_port)
      socket.print("#{request}\r\n")
      headers, body = read_response_on_socket(socket)
      socket.print(frame)

      EventMachine.run do
        pub = EventMachine::HttpRequest.new(nginx_address + '/channels-stats?id=' + channel.to_s).get :timeout => 30
        pub.callback do
          pub.should be_http_status(200).with_body
          response = JSON.parse(pub.response)
          response["channel"].to_s.should eql(channel)
          response["published_messages"].to_i.should eql(0)
          response["stored_messages"].to_i.should eql(0)
          response["subscribers"].to_i.should eql(0)
          EventMachine.stop
        end
      end
    end
  end

  it "should not cache the response" do
    channel = 'ch_test_not_cache_the_response'

    request = "GET /ws/#{channel}.b1 HTTP/1.0\r\nConnection: Upgrade\r\nSec-WebSocket-Key: /mQoZf6pRiv8+6o72GncLQ==\r\nUpgrade: websocket\r\nSec-WebSocket-Version: 8\r\n"

    nginx_run_server(config) do |conf|
      socket = open_socket(nginx_host, nginx_port)
      socket.print("#{request}\r\n")
      headers, body = read_response_on_socket(socket)

      headers.should include("Expires: Thu, 01 Jan 1970 00:00:01 GMT\r\n")
      headers.should include("Cache-Control: no-cache, no-store, must-revalidate\r\n")
    end
  end
end
