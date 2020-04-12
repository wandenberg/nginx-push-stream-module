# encoding: ascii
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
            push_stream_subscriber websocket;

            # positional channel path
            push_stream_channels_path               $1;

            # allow subscriber to publish
            push_stream_websocket_allow_publish     on;
            # store messages
            push_stream_store_messages              on;
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
          expect(multi.responses[:callback].length).to eql(5)

          expect(multi.responses[:callback][:a]).to be_http_status(405)
          expect(multi.responses[:callback][:a].req.method).to eql("HEAD")
          expect(multi.responses[:callback][:a].response_header['ALLOW']).to eql("GET")

          expect(multi.responses[:callback][:b]).to be_http_status(405)
          expect(multi.responses[:callback][:b].req.method).to eql("PUT")
          expect(multi.responses[:callback][:b].response_header['ALLOW']).to eql("GET")

          expect(multi.responses[:callback][:c]).to be_http_status(405)
          expect(multi.responses[:callback][:c].req.method).to eql("POST")
          expect(multi.responses[:callback][:c].response_header['ALLOW']).to eql("GET")

          expect(multi.responses[:callback][:d]).to be_http_status(405)
          expect(multi.responses[:callback][:d].req.method).to eql("DELETE")
          expect(multi.responses[:callback][:d].response_header['ALLOW']).to eql("GET")

          expect(multi.responses[:callback][:e]).not_to be_http_status(405)
          expect(multi.responses[:callback][:e].req.method).to eql("GET")

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
      expect(body).to eql("")
      expect(headers).to match_the_pattern(/Don't have at least one of the mandatory headers: Connection, Upgrade, Sec-WebSocket-Key and Sec-WebSocket-Version/)
      socket.close

      request << "Connection: Upgrade\r\n"

      socket = open_socket(nginx_host, nginx_port)
      socket.print("#{request}\r\n")
      headers, body = read_response_on_socket(socket)
      expect(body).to eql("")
      expect(headers).to match_the_pattern(/Don't have at least one of the mandatory headers: Connection, Upgrade, Sec-WebSocket-Key and Sec-WebSocket-Version/)
      socket.close

      request << "Sec-WebSocket-Key: /mQoZf6pRiv8+6o72GncLQ==\r\n"

      socket = open_socket(nginx_host, nginx_port)
      socket.print("#{request}\r\n")
      headers, body = read_response_on_socket(socket)
      expect(body).to eql("")
      expect(headers).to match_the_pattern(/Don't have at least one of the mandatory headers: Connection, Upgrade, Sec-WebSocket-Key and Sec-WebSocket-Version/)
      socket.close

      request << "Upgrade: websocket\r\n"

      socket = open_socket(nginx_host, nginx_port)
      socket.print("#{request}\r\n")
      headers, body = read_response_on_socket(socket)
      expect(body).to eql("")
      expect(headers).to match_the_pattern(/Don't have at least one of the mandatory headers: Connection, Upgrade, Sec-WebSocket-Key and Sec-WebSocket-Version/)
      socket.close

      request << "Sec-WebSocket-Version: 8\r\n"

      socket = open_socket(nginx_host, nginx_port)
      socket.print("#{request}\r\n")
      headers, body = read_response_on_socket(socket)
      expect(body).to eql("")
      expect(headers).not_to match_the_pattern(/Don't have at least one of the mandatory headers: Connection, Upgrade, Sec-WebSocket-Key and Sec-WebSocket-Version/)
      expect(headers).to match_the_pattern(/HTTP\/1\.1 101 Switching Protocols/)
      socket.close
    end
  end

  it "should check supported versions" do
    channel = 'ch_test_supported_versions'
    request = "GET /ws/#{channel}.b1 HTTP/1.0\r\nConnection: Upgrade\r\nSec-WebSocket-Key: /mQoZf6pRiv8+6o72GncLQ==\r\nUpgrade: websocket\r\n"

    nginx_run_server(config) do |conf|
      socket = open_socket(nginx_host, nginx_port)
      socket.print("#{request}Sec-WebSocket-Version: 7\r\n\r\n")
      headers, body = read_response_on_socket(socket)
      expect(body).to eql("")
      expect(headers).to match_the_pattern(/Sec-WebSocket-Version: 8, 13/)
      expect(headers).to match_the_pattern(/X-Nginx-PushStream-Explain: Version not supported. Supported versions: 8, 13/)
      socket.close

      socket = open_socket(nginx_host, nginx_port)
      socket.print("#{request}Sec-WebSocket-Version: 8\r\n\r\n")
      headers, body = read_response_on_socket(socket)
      expect(body).to eql("")
      expect(headers).not_to match_the_pattern(/Sec-WebSocket-Version: 8, 13/)
      expect(headers).not_to match_the_pattern(/X-Nginx-PushStream-Explain: Version not supported. Supported versions: 8, 13/)
      socket.close

      socket = open_socket(nginx_host, nginx_port)
      socket.print("#{request}Sec-WebSocket-Version: 13\r\n\r\n")
      headers, body = read_response_on_socket(socket)
      expect(body).to eql("")
      expect(headers).not_to match_the_pattern(/Sec-WebSocket-Version: 8, 13/)
      expect(headers).not_to match_the_pattern(/X-Nginx-PushStream-Explain: Version not supported. Supported versions: 8, 13/)
      socket.close
    end
  end

  it "should check response headers" do
    channel = 'ch_test_response_headers'
    request = "GET /ws/#{channel}.b1 HTTP/1.0\r\nConnection: Upgrade\r\nSec-WebSocket-Key: /mQoZf6pRiv8+6o72GncLQ==\r\nUpgrade: websocket\r\nSec-WebSocket-Version: 8\r\n"

    nginx_run_server(config) do |conf|
      socket = open_socket(nginx_host, nginx_port)
      socket.print("#{request}\r\n")
      headers, body = read_response_on_socket(socket)
      socket.close
      expect(body).to eql("")
      expect(headers).to match_the_pattern(/HTTP\/1\.1 101 Switching Protocols/)
      expect(headers).to match_the_pattern(/Sec-WebSocket-Accept: RaIOIcQ6CBoc74B9EKdH0avYZnw=/)
      expect(headers).to match_the_pattern(/Upgrade: WebSocket/)
      expect(headers).to match_the_pattern(/Connection: Upgrade/)
    end
  end

  it "should receive header template" do
    channel = 'ch_test_receive_header_template'
    request = "GET /ws/#{channel}.b1 HTTP/1.0\r\nConnection: Upgrade\r\nSec-WebSocket-Key: /mQoZf6pRiv8+6o72GncLQ==\r\nUpgrade: websocket\r\nSec-WebSocket-Version: 8\r\n"

    nginx_run_server(config.merge(:header_template => "HEADER_TEMPLATE")) do |conf|
      socket = open_socket(nginx_host, nginx_port)
      socket.print("#{request}\r\n")
      headers, body = read_response_on_socket(socket, 'TEMPLATE')
      expect(body).to eql("\201\017HEADER_TEMPLATE")
      socket.close
    end
  end

  it "should send a ping frame to client" do
    channel = 'ch_test_receive_ping_frame'
    request = "GET /ws/#{channel}.b1 HTTP/1.0\r\nConnection: Upgrade\r\nSec-WebSocket-Key: /mQoZf6pRiv8+6o72GncLQ==\r\nUpgrade: websocket\r\nSec-WebSocket-Version: 8\r\n"

    nginx_run_server(config.merge(:ping_message_interval => '1s')) do |conf|
      socket = open_socket(nginx_host, nginx_port)
      socket.print("#{request}\r\n")
      headers, body = read_response_on_socket(socket)
      body, dummy = read_response_on_socket(socket, "\211\000")
      expect(body).to eql("\211\000")
      socket.close
    end
  end

  it "should send a close frame to client" do
    channel = 'ch_test_receive_close_frame'
    request = "GET /ws/#{channel}.b1 HTTP/1.0\r\nConnection: Upgrade\r\nSec-WebSocket-Key: /mQoZf6pRiv8+6o72GncLQ==\r\nUpgrade: websocket\r\nSec-WebSocket-Version: 8\r\n"

    nginx_run_server(config.merge(:subscriber_connection_ttl => '1s')) do |conf|
      socket = open_socket(nginx_host, nginx_port)
      socket.print("#{request}\r\n")
      headers, body = read_response_on_socket(socket)
      body, dummy = read_response_on_socket(socket, "\210\000")
      expect(body).to eql("\210\000")
      socket.close
    end
  end

  it "should send a explain message on close frame" do
    channel = 'ch_test_receive_explain_message_close_frame'
    request = "GET /ws/#{channel}.b1 HTTP/1.0\r\nConnection: Upgrade\r\nSec-WebSocket-Key: /mQoZf6pRiv8+6o72GncLQ==\r\nUpgrade: websocket\r\nSec-WebSocket-Version: 8\r\n"

    nginx_run_server(config.merge(:authorized_channels_only => 'on')) do |conf|
      socket = open_socket(nginx_host, nginx_port)
      socket.print("#{request}\r\n")
      headers, body = read_response_on_socket(socket, "\"}")
      expect(body).to eql("\x88I\x03\xF0{\"http_status\": 403, \"explain\":\"Subscriber could not create channels.\"}")
      socket.close
    end
  end

  it "should receive footer template" do
    channel = 'ch_test_receive_footer_template'
    request = "GET /ws/#{channel}.b1 HTTP/1.0\r\nConnection: Upgrade\r\nSec-WebSocket-Key: /mQoZf6pRiv8+6o72GncLQ==\r\nUpgrade: websocket\r\nSec-WebSocket-Version: 8\r\n"

    nginx_run_server(config.merge(:subscriber_connection_ttl => '1s', :footer_template => "FOOTER_TEMPLATE")) do |conf|
      socket = open_socket(nginx_host, nginx_port)
      socket.print("#{request}\r\n")
      headers, body = read_response_on_socket(socket)
      body, dummy = read_response_on_socket(socket, "\210\000")
      expect(body).to eql("\201\017FOOTER_TEMPLATE\210\000")
      socket.close
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
      expect(body).to eql("\201\005Hello")
      socket.close
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
      expect(body).to match_the_pattern(/^\201\176\377\377aaa/)
      socket.close
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
      expect(body).to match_the_pattern(/^\201\177\000\000\000\000\000\001\000\000aaa/)
      socket.close
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
      expect(body_1).to eql("\201\017{\"text\":\"#{body}\"}")

      socket_2 = open_socket(nginx_host, nginx_port)
      socket_2.print("#{request_2}\r\n")
      headers_2, body_2 = read_response_on_socket(socket_2, '}')
      expect(body_2).to eql("{\"text\":\"#{body}\"}")
      socket_1.close
      socket_2.close
    end
  end

  it "should publish message to all subscribed channels using the same stream" do
    frame = "%c%c%c%c%c%c%c%c%c%c%c" % [0x81, 0x85, 0xBD, 0xD0, 0xE5, 0x2A, 0xD5, 0xB5, 0x89, 0x46, 0xD2] #send 'hello' text

    request = "GET /ws/ch2/ch1 HTTP/1.0\r\nConnection: Upgrade\r\nSec-WebSocket-Key: /mQoZf6pRiv8+6o72GncLQ==\r\nUpgrade: websocket\r\nSec-WebSocket-Version: 8\r\n"

    nginx_run_server(config.merge({ message_template: '{\"channel\":\"~channel~\", \"id\":\"~id~\", \"message\":\"~text~\"}' })) do |conf|
      socket = open_socket(nginx_host, nginx_port)
      socket.print("#{request}\r\n")
      headers, body = read_response_on_socket(socket)
      socket.print(frame)

      body, dummy = read_response_on_socket(socket, "ch1")
      expect(body).to eql("\x81.{\"channel\":\"ch2\", \"id\":\"1\", \"message\":\"hello\"}\x81.{\"channel\":\"ch1\", \"id\":\"1\", \"message\":\"hello\"}")
      socket.close

      EventMachine.run do
        pub = EventMachine::HttpRequest.new(nginx_address + '/channels-stats?id=ALL').get :timeout => 30
        pub.callback do
          expect(pub).to be_http_status(200).with_body
          response = JSON.parse(pub.response)
          expect(response["channels"].to_s).not_to be_empty
          expect(response["channels"].to_i).to eql(2)
          expect(response["infos"][0]["channel"]).to eql("ch2")
          expect(response["infos"][0]["published_messages"]).to eql(1)
          expect(response["infos"][0]["stored_messages"]).to eql(1)
          expect(response["infos"][1]["channel"]).to eql("ch1")
          expect(response["infos"][1]["published_messages"]).to eql(1)
          expect(response["infos"][1]["stored_messages"]).to eql(1)
          EventMachine.stop
        end
      end

      EventMachine.run do
        sub = EventMachine::HttpRequest.new(nginx_address + '/sub/ch1.b1').get :timeout => 30
        sub.stream do |chunk|
          line = JSON.parse(chunk.split("\r\n")[0])
          expect(line['channel']).to eql("ch1")
          expect(line['message']).to eql('hello')
          expect(line['id'].to_i).to eql(1)
          EventMachine.stop
        end
      end

      EventMachine.run do
        sub = EventMachine::HttpRequest.new(nginx_address + '/sub/ch2.b1').get :timeout => 30
        sub.stream do |chunk|
          line = JSON.parse(chunk.split("\r\n")[0])
          expect(line['channel']).to eql("ch2")
          expect(line['message']).to eql('hello')
          expect(line['id'].to_i).to eql(1)
          EventMachine.stop
        end
      end
    end
  end

  it "should publish large message" do
    channel = 'ch_test_publish_large_message'

    small_message = "^|" + ("0123456789" * 1020) + "|$"
    large_message = "^|" + ("0123456789" * 419430) + "|$"

    received_messages = 0;
    nginx_run_server(config.merge({ shared_memory_size: '15m', message_template: '{\"channel\":\"~channel~\", \"id\":\"~id~\", \"message\":\"~text~\"}' }), timeout: 10) do |conf|
      EventMachine.run do
        ws = WebSocket::EventMachine::Client.connect(:uri => "ws://#{nginx_host}:#{nginx_port}/ws/#{channel}")
        ws.onmessage do |text, type|
          received_messages += 1
          msg = JSON.parse(text)
          expect(msg['channel']).to eql(channel)
          if received_messages == 1
            expect(msg['message']).to eql(large_message)
            expect(msg['message'].size).to eql(4194304) # 4mb
            ws.send small_message
          elsif received_messages == 2
            expect(msg['message']).to eql(small_message)
            expect(msg['message'].size).to eql(10204) # 10kb
            EventMachine.stop
          end
        end

        EM.add_timer(1) do
          ws.send large_message
        end
      end
    end
  end

  it "should publish message with a low bitrate" do
    channel = 'ch_test_publish_message_low_bitrate'

    configuration = config.merge({
      shared_memory_size: '15m',
      message_template: '{\"channel\":\"~channel~\", \"message\":\"~text~\"}',
    })

    count = 0
    nginx_run_server(configuration, timeout: 60) do |conf|
      EventMachine.run do
        frame = "%c%c%c%c%c%c%c%c%c%c%c" % [0x81, 0x85, 0x37, 0xfa, 0x21, 0x3d, 0x7f, 0x9f, 0x4d, 0x51, 0x58] #send 'hello' frame

        request = "GET /ws/#{channel} HTTP/1.0\r\nConnection: Upgrade\r\nSec-WebSocket-Key: /mQoZf6pRiv8+6o72GncLQ==\r\nUpgrade: websocket\r\nSec-WebSocket-Version: 8\r\n"

        socket = open_socket(nginx_host, nginx_port)
        socket.print("#{request}\r\n")
        headers, body = read_response_on_socket(socket)

        EM.add_periodic_timer(2) do
          socket.print(frame[count])
          count += 1
        end

        EM.add_timer(frame.size * 3) do
          pub = EventMachine::HttpRequest.new(nginx_address + '/channels-stats?id=' + channel.to_s).get :timeout => 30
          pub.callback do
            body, dummy = read_response_on_socket(socket, "llo")
            expect(body).to include(%[{"channel":"ch_test_publish_message_low_bitrate", "message":"Hello"}])
            socket.close
            expect(pub).to be_http_status(200).with_body
            response = JSON.parse(pub.response)
            expect(response["channel"].to_s).to eql(channel)
            expect(response["published_messages"].to_i).to eql(1)
            expect(response["stored_messages"].to_i).to eql(1)
            expect(response["subscribers"].to_i).to eql(1)
            EventMachine.stop
          end
        end
      end
    end
  end

  it "should accept ping message and return a pong frame" do
    channel = 'ch_test_accept_ping_message'
    frame = "%c%c%c%c%c%c%c" % [0x89, 0x05, 0x48, 0x65, 0x6c, 0x6c, 0x6f] #send 'ping' frame with message

    request = "GET /ws/#{channel}.b1 HTTP/1.0\r\nConnection: Upgrade\r\nSec-WebSocket-Key: /mQoZf6pRiv8+6o72GncLQ==\r\nUpgrade: websocket\r\nSec-WebSocket-Version: 8\r\n"

    nginx_run_server(config) do |conf|
      socket = open_socket(nginx_host, nginx_port)
      socket.print("#{request}\r\n")
      headers, body = read_response_on_socket(socket)
      socket.print(frame)
      body, _ = read_response_on_socket(socket)
      expect(body).to eql("\x8A\x00")

      EventMachine.run do
        pub = EventMachine::HttpRequest.new(nginx_address + '/channels-stats?id=' + channel.to_s).get :timeout => 30
        pub.callback do
          socket.close
          expect(pub).to be_http_status(200).with_body
          response = JSON.parse(pub.response)
          expect(response["channel"].to_s).to eql(channel)
          expect(response["published_messages"].to_i).to eql(0)
          expect(response["stored_messages"].to_i).to eql(0)
          expect(response["subscribers"].to_i).to eql(1)
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
          socket.close
          expect(pub).to be_http_status(200).with_body
          response = JSON.parse(pub.response)
          expect(response["channel"].to_s).to eql(channel)
          expect(response["published_messages"].to_i).to eql(0)
          expect(response["stored_messages"].to_i).to eql(0)
          expect(response["subscribers"].to_i).to eql(1)
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
      body, dummy = read_response_on_socket(socket, "\210\000")
      expect(body).to eql("\210\000")

      EventMachine.run do
        pub = EventMachine::HttpRequest.new(nginx_address + '/channels-stats?id=' + channel.to_s).get :timeout => 30
        pub.callback do
          socket.close
          expect(pub).to be_http_status(200).with_body
          response = JSON.parse(pub.response)
          expect(response["channel"].to_s).to eql(channel)
          expect(response["published_messages"].to_i).to eql(0)
          expect(response["stored_messages"].to_i).to eql(0)
          expect(response["subscribers"].to_i).to eql(0)
          EventMachine.stop
        end
      end
    end
  end

  it "should accept messages with different bytes" do
    nginx_run_server(config.merge(:client_max_body_size => '130k', :client_body_buffer_size => '130k', :message_template => "~text~|")) do |conf|
      ranges = [0..255]
      ranges.each do |range|
        bytes = []
        range.each do |i|
          0.upto(255) do |j|
            bytes << "%s%s" % [i.chr, j.chr]
          end
        end

        channel = "ch_test_publish_messages_with_different_bytes_#{range}"

        body = bytes.join('')
        response = ''

        request = "GET /ws/#{channel} HTTP/1.0\r\nConnection: Upgrade\r\nSec-WebSocket-Key: /mQoZf6pRiv8+6o72GncLQ==\r\nUpgrade: websocket\r\nSec-WebSocket-Version: 8\r\n"

        socket = open_socket(nginx_host, nginx_port)
        socket.print("#{request}\r\n")

        EventMachine.run do
          pub = EventMachine::HttpRequest.new(nginx_address + '/pub?id=' + channel.to_s ).post :head => headers, :body => body
          pub.callback do
            headers, resp = read_response_on_socket(socket, '|')
            expect(resp.bytes.to_a).to eql("\x81\x7F\x00\x00\x00\x00\x00\x02\x00\x01#{body}|".bytes.to_a)
            EventMachine.stop
            socket.close
          end
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
      socket.close

      expect(headers).to include("Expires: Thu, 01 Jan 1970 00:00:01 GMT\r\n")
      expect(headers).to include("Cache-Control: no-cache, no-store, must-revalidate\r\n")
    end
  end

  it "should not try to parse the request line when receive a frame after send close frame" do
    channel = 'ch_test_data_after_close_frame_parse_request_line'
    pid = pid2 = 0

    request = "GET /ws/#{channel}.b1 HTTP/1.1\r\nHost: localhost\r\nConnection: Upgrade\r\nSec-WebSocket-Key: /mQoZf6pRiv8+6o72GncLQ==\r\nUpgrade: websocket\r\nSec-WebSocket-Version: 8\r\n"

    nginx_run_server(config.merge(:subscriber_connection_ttl => '1s')) do |conf|
      File.open(conf.error_log, "a").truncate(0)

      socket = open_socket(nginx_host, nginx_port)
      socket.print("#{request}\r\n")
      headers, body = read_response_on_socket(socket)

      # wait for close frame
      body, dummy = read_response_on_socket(socket, "\210\000")
      expect(body).to eql("\210\000")

      socket.print("WRITE SOMETHING UNKNOWN\r\n")

      error_log = File.read(conf.error_log)
      expect(error_log).not_to include("client sent invalid")
      socket.close
    end
  end

  it "should not try to parse the request line when doing a reload" do
    channel = 'ch_test_reload_not_parse_request_line'
    pid = pid2 = 0

    request = "GET /ws/#{channel}.b1 HTTP/1.1\r\nHost: localhost\r\nConnection: Upgrade\r\nSec-WebSocket-Key: /mQoZf6pRiv8+6o72GncLQ==\r\nUpgrade: websocket\r\nSec-WebSocket-Version: 8\r\n"


    nginx_run_server(config.merge(:ping_message_interval => "1s"), :timeout => 10) do |conf|
      error_log_pre = File.readlines(conf.error_log)

      EventMachine.run do
        publish_message_inline(channel, {}, "body")

        socket = open_socket(nginx_host, nginx_port)
        socket.print("#{request}\r\n")
        headers, body = read_response_on_socket(socket)

        # check statistics
        pub_1 = EventMachine::HttpRequest.new(nginx_address + '/channels-stats').get
        pub_1.callback do
          expect(pub_1).to be_http_status(200).with_body
          resp_1 = JSON.parse(pub_1.response)
          expect(resp_1.has_key?("channels")).to be_truthy
          expect(resp_1["channels"].to_i).to eql(1)
          expect(resp_1["subscribers"].to_i).to eql(1)

          # send reload signal
          `#{ nginx_executable } -c #{ conf.configuration_filename } -s reload > /dev/null 2>&1`

          socket.print("WRITE SOMETHING UNKNOWN\r\n")
          sleep 0.001

          pub_2 = EventMachine::HttpRequest.new(nginx_address + '/channels-stats').get
          pub_2.callback do
            socket.close
            expect(pub_2).to be_http_status(200).with_body
            resp_2 = JSON.parse(pub_2.response)
            expect(resp_2.has_key?("channels")).to be_truthy
            expect(resp_2["channels"].to_i).to eql(1)
            expect(resp_2["subscribers"].to_i).to eql(0)

            error_log_pos = File.readlines(conf.error_log)
            expect((error_log_pos - error_log_pre).join).not_to include("client sent invalid")

            EventMachine.stop
          end
        end
      end
    end
  end

  it "should accept non latin characters" do
    channel = 'ch_test_publish_non_latin'

    nginx_run_server(config) do |conf|
      EventMachine.run do
        ws = WebSocket::EventMachine::Client.connect(:uri => "ws://#{nginx_host}:#{nginx_port}/ws/#{channel}")
        ws.onmessage do |text, type|
          expect(text).to eq("\xD8\xA3\xD9\x8E\xD8\xA8\xD9\x92\xD8\xAC\xD9\x8E\xD8\xAF\xD9\x90\xD9\x8A\xD9\x8E\xD9\x91\xD8\xA9 \xD8\xB9\xD9\x8E")
          EventMachine.stop
        end

        EM.add_timer(1) do
          ws.send "\xD8\xA3\xD9\x8E\xD8\xA8\xD9\x92\xD8\xAC\xD9\x8E\xD8\xAF\xD9\x90\xD9\x8A\xD9\x8E\xD9\x91\xD8\xA9 \xD8\xB9\xD9\x8E"
        end
      end
    end
  end

  it "should reject an invalid utf8 sequence" do
    channel = 'ch_test_publish_invalid_utf8'

    nginx_run_server(config) do |conf|
      EventMachine.run do
        ws = WebSocket::EventMachine::Client.connect(:uri => "ws://#{nginx_host}:#{nginx_port}/ws/#{channel}")
        ws.onmessage do |text, type|
          fail("Should not have received the '#{text.force_encoding('UTF-8')}'")
        end

        ws.onclose do
          EventMachine.stop
        end

        EM.add_timer(1) do
          ws.send "\xA3\xD9\x8E\xD8\xA8\xD9\x92\xD8\xAC\xD9\x8E\xD8\xAF\xD9\x90\xD9\x8A\xD9\x8E\xD9\x91\xD8\xA9 \xD8\xB9\xD9\x8E"
        end
      end
    end
  end

  it "should reject unsupported frames" do
    channel = 'ch_test_reject_unsupported_frames'
    frame = "%c%c%c%c%c%c%c%c%c%c%c" % [0x82, 0x85, 0xBD, 0xD0, 0xE5, 0x2A, 0xD5, 0xB5, 0x89, 0x46, 0xD2] #send binary frame

    request = "GET /ws/#{channel}.b1 HTTP/1.0\r\nConnection: Upgrade\r\nSec-WebSocket-Key: /mQoZf6pRiv8+6o72GncLQ==\r\nUpgrade: websocket\r\nSec-WebSocket-Version: 8\r\n"

    nginx_run_server(config) do |conf|
      socket = open_socket(nginx_host, nginx_port)
      socket.print("#{request}\r\n")
      headers, body = read_response_on_socket(socket)
      socket.print(frame)
      body, dummy = read_response_on_socket(socket, "\210\000")
      expect(body).to eql("\210\000")

      EventMachine.run do
        pub = EventMachine::HttpRequest.new(nginx_address + '/channels-stats?id=' + channel.to_s).get :timeout => 30
        pub.callback do
          socket.close
          expect(pub).to be_http_status(200).with_body
          response = JSON.parse(pub.response)
          expect(response["channel"].to_s).to eql(channel)
          expect(response["published_messages"].to_i).to eql(0)
          expect(response["stored_messages"].to_i).to eql(0)
          expect(response["subscribers"].to_i).to eql(0)
          EventMachine.stop
        end
      end
    end
  end

  it "should accept unmasked frames" do
    channel = 'ch_test_publish_unmasked_frames'

    configuration = config.merge({
      shared_memory_size: '15m',
      message_template: '{\"channel\":\"~channel~\", \"message\":\"~text~\"}',
      subscriber_mode: 'long-polling',
    })

    nginx_run_server(configuration, timeout: 60) do |conf|
      EventMachine.run do
        frame = "%c%c%c%c%c%c%c" % [0x81, 0x05, 0x48, 0x65, 0x6c, 0x6c, 0x6f] #send 'hello' frame

        request = "GET /ws/#{channel} HTTP/1.0\r\nConnection: Upgrade\r\nSec-WebSocket-Key: /mQoZf6pRiv8+6o72GncLQ==\r\nUpgrade: websocket\r\nSec-WebSocket-Version: 8\r\n"

        socket = open_socket(nginx_host, nginx_port)
        socket.print("#{request}\r\n")
        headers, body = read_response_on_socket(socket)
        socket.print(frame)
        body, dummy = read_response_on_socket(socket, "llo")
        expect(body).to include(%[{"channel":"ch_test_publish_unmasked_frames", "message":"Hello"}])
        socket.close

        sub = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s + '.b1').get :timeout => 30
        sub.callback do
          expect(sub).to be_http_status(200)
          response = JSON.parse(sub.response)
          expect(response["channel"].to_s).to eql(channel)
          expect(response["message"]).to eql("Hello")
          EventMachine.stop
        end
      end
    end
  end

  it "should accept masked frames" do
    channel = 'ch_test_publish_masked_frames'

    configuration = config.merge({
      shared_memory_size: '15m',
      message_template: '{\"channel\":\"~channel~\", \"message\":\"~text~\"}',
      subscriber_mode: 'long-polling',
    })

    nginx_run_server(configuration, timeout: 60) do |conf|
      EventMachine.run do
        frame = "%c%c%c%c%c%c%c%c%c%c%c" % [0x81, 0x85, 0x37, 0xfa, 0x21, 0x3d, 0x7f, 0x9f, 0x4d, 0x51, 0x58] #send 'hello' frame

        request = "GET /ws/#{channel} HTTP/1.0\r\nConnection: Upgrade\r\nSec-WebSocket-Key: /mQoZf6pRiv8+6o72GncLQ==\r\nUpgrade: websocket\r\nSec-WebSocket-Version: 8\r\n"

        socket = open_socket(nginx_host, nginx_port)
        socket.print("#{request}\r\n")
        headers, body = read_response_on_socket(socket)
        socket.print(frame)
        body, dummy = read_response_on_socket(socket, "llo")
        expect(body).to include(%[{"channel":"ch_test_publish_masked_frames", "message":"Hello"}])
        socket.close

        sub = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s + '.b1').get :timeout => 30
        sub.callback do
          expect(sub).to be_http_status(200)
          response = JSON.parse(sub.response)
          expect(response["channel"].to_s).to eql(channel)
          expect(response["message"]).to eql("Hello")
          EventMachine.stop
        end
      end
    end
  end

  it "should accept fragmented unmasked frames" do
    channel = 'ch_test_publish_fragmented_unmasked_frames'

    configuration = config.merge({
      shared_memory_size: '15m',
      message_template: '{\"channel\":\"~channel~\", \"message\":\"~text~\"}',
      subscriber_mode: 'long-polling',
    })

    nginx_run_server(configuration, timeout: 60) do |conf|
      EventMachine.run do
        frame_part1 = "%c%c%c%c%c" % [0x01, 0x03, 0x48, 0x65, 0x6c] #send 'Hel' frame
        frame_part2 = "%c%c%c%c" % [0x80, 0x02, 0x6c, 0x6f] #send 'lo' frame

        request = "GET /ws/#{channel} HTTP/1.0\r\nConnection: Upgrade\r\nSec-WebSocket-Key: /mQoZf6pRiv8+6o72GncLQ==\r\nUpgrade: websocket\r\nSec-WebSocket-Version: 8\r\n"

        socket = open_socket(nginx_host, nginx_port)
        socket.print("#{request}\r\n")
        headers, body = read_response_on_socket(socket)
        socket.print(frame_part1)
        sleep 0.0001
        socket.print(frame_part2)
        body, dummy = read_response_on_socket(socket, "llo")
        expect(body).to include(%[{"channel":"ch_test_publish_fragmented_unmasked_frames", "message":"Hello"}])
        socket.close

        sub = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s + '.b1').get :timeout => 30
        sub.callback do
          expect(sub).to be_http_status(200)
          response = JSON.parse(sub.response)
          expect(response["channel"].to_s).to eql(channel)
          expect(response["message"]).to eql("Hello")
          EventMachine.stop
        end
      end
    end
  end

  it "should accept all kinds of frames mixed" do
    channel = 'ch_test_publish_frames_mixed'

    configuration = config.merge({
      shared_memory_size: '15m',
      message_template: '{\"channel\":\"~channel~\", \"id\":\"~id~\", \"message\":\"~text~\"}',
    })

    nginx_run_server(configuration, timeout: 60) do |conf|
      frame_unmasked = "%c%c%c%c%c%c%c" % [0x81, 0x05, 0x48, 0x65, 0x6c, 0x6c, 0x6f] #send 'hello' frame
      frame_masked = "%c%c%c%c%c%c%c%c%c%c%c" % [0x81, 0x85, 0x37, 0xfa, 0x21, 0x3d, 0x7f, 0x9f, 0x4d, 0x51, 0x58] #send 'hello' frame
      frame_part1 = "%c%c%c%c%c" % [0x01, 0x03, 0x48, 0x65, 0x6c] #send 'Hel' frame
      frame_part2 = "%c%c%c%c" % [0x80, 0x02, 0x6c, 0x6f] #send 'lo' frame

      request = "GET /ws/#{channel} HTTP/1.0\r\nConnection: Upgrade\r\nSec-WebSocket-Key: /mQoZf6pRiv8+6o72GncLQ==\r\nUpgrade: websocket\r\nSec-WebSocket-Version: 8\r\n"

      socket = open_socket(nginx_host, nginx_port)
      socket.print("#{request}\r\n")
      headers, body = read_response_on_socket(socket)

      socket.print(frame_unmasked)
      body, dummy = read_response_on_socket(socket, "llo")
      expect(body).to include(%[{"channel":"ch_test_publish_frames_mixed", "id":"1", "message":"Hello"}])

      socket.print(frame_masked)
      body, dummy = read_response_on_socket(socket, "llo")
      expect(body).to include(%[{"channel":"ch_test_publish_frames_mixed", "id":"2", "message":"Hello"}])

      socket.print(frame_part1)
      sleep 0.0001
      socket.write(frame_part2)
      body, dummy = read_response_on_socket(socket, "llo")
      expect(body).to include(%[{"channel":"ch_test_publish_frames_mixed", "id":"3", "message":"Hello"}])

      socket.print(frame_masked)
      body, dummy = read_response_on_socket(socket, "llo")
      expect(body).to include(%[{"channel":"ch_test_publish_frames_mixed", "id":"4", "message":"Hello"}])

      socket.print(frame_unmasked)
      body, dummy = read_response_on_socket(socket, "llo")
      expect(body).to include(%[{"channel":"ch_test_publish_frames_mixed", "id":"5", "message":"Hello"}])

      socket.print(frame_part1)
      sleep 0.0001
      socket.print(frame_part2)
      body, dummy = read_response_on_socket(socket, "llo")
      expect(body).to include(%[{"channel":"ch_test_publish_frames_mixed", "id":"6", "message":"Hello"}])

      socket.print(frame_unmasked)
      body, dummy = read_response_on_socket(socket, "llo")
      expect(body).to include(%[{"channel":"ch_test_publish_frames_mixed", "id":"7", "message":"Hello"}])

      socket.print(frame_masked)
      body, dummy = read_response_on_socket(socket, "llo")
      expect(body).to include(%[{"channel":"ch_test_publish_frames_mixed", "id":"8", "message":"Hello"}])

      socket.close
    end
  end
end
