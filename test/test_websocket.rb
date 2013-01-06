require File.expand_path('base_test_case', File.dirname(__FILE__))

class TestWebSocket < Test::Unit::TestCase
  include BaseTestCase

  def global_configuration
    @header_template = nil
    @message_template = nil
    @footer_template = nil

    @extra_location = %q{
      location ~ /ws/(.*)? {
          # activate websocket mode for this location
          push_stream_websocket;

          # positional channel path
          set $push_stream_channels_path          $1;
      }
    }
  end

  def test_accepted_methods
    EventMachine.run {
      multi = EventMachine::MultiRequest.new

      multi.add(:a, EventMachine::HttpRequest.new(nginx_address + '/ws/ch_test_accepted_methods_1').head)
      multi.add(:b, EventMachine::HttpRequest.new(nginx_address + '/ws/ch_test_accepted_methods_2').put(:body => 'body'))
      multi.add(:c, EventMachine::HttpRequest.new(nginx_address + '/ws/ch_test_accepted_methods_3').post)
      multi.add(:d, EventMachine::HttpRequest.new(nginx_address + '/ws/ch_test_accepted_methods_4').delete)
      multi.add(:e, EventMachine::HttpRequest.new(nginx_address + '/ws/ch_test_accepted_methods_5').get)

      multi.callback  {
        assert_equal(5, multi.responses[:callback].length)

        assert_equal(405, multi.responses[:callback][:a].response_header.status, "Publisher does not accept HEAD")
        assert_equal("HEAD", multi.responses[:callback][:a].req.method, "Array is with wrong order")
        assert_equal("GET", multi.responses[:callback][:a].response_header['ALLOW'], "Didn't receive the right error message")

        assert_equal(405, multi.responses[:callback][:b].response_header.status, "Publisher does not accept PUT")
        assert_equal("PUT", multi.responses[:callback][:b].req.method, "Array is with wrong order")
        assert_equal("GET", multi.responses[:callback][:b].response_header['ALLOW'], "Didn't receive the right error message")

        assert_equal(405, multi.responses[:callback][:c].response_header.status, "Publisher does accept POST")
        assert_equal("POST", multi.responses[:callback][:c].req.method, "Array is with wrong order")
        assert_equal("GET", multi.responses[:callback][:b].response_header['ALLOW'], "Didn't receive the right error message")

        assert_equal(405, multi.responses[:callback][:d].response_header.status, "Publisher does not accept DELETE")
        assert_equal("DELETE", multi.responses[:callback][:d].req.method, "Array is with wrong order")
        assert_equal("GET", multi.responses[:callback][:d].response_header['ALLOW'], "Didn't receive the right error message")

        assert_not_equal(405, multi.responses[:callback][:e].response_header.status, "Publisher does accept GET")
        assert_equal("GET", multi.responses[:callback][:e].req.method, "Array is with wrong order")

        EventMachine.stop
      }
    }
  end

  def test_check_mandatory_headers
    channel = 'ch_test_check_mandatory_headers'
    request = "GET /ws/#{channel}.b1 HTTP/1.0\r\n"

    socket = TCPSocket.open(nginx_host, nginx_port)
    socket.print("#{request}\r\n")
    headers, body = read_response(socket)
    assert_equal("", body, "Wrong response")
    assert(headers.match(/Don't have at least one of the mandatory headers: Connection, Upgrade, Sec-WebSocket-Key and Sec-WebSocket-Version/), "Didn't receive error message")

    request << "Connection: Upgrade\r\n"

    socket = TCPSocket.open(nginx_host, nginx_port)
    socket.print("#{request}\r\n")
    headers, body = read_response(socket)
    assert_equal("", body, "Wrong response")
    assert(headers.match(/Don't have at least one of the mandatory headers: Connection, Upgrade, Sec-WebSocket-Key and Sec-WebSocket-Version/), "Didn't receive error message")

    request << "Sec-WebSocket-Key: /mQoZf6pRiv8+6o72GncLQ==\r\n"

    socket = TCPSocket.open(nginx_host, nginx_port)
    socket.print("#{request}\r\n")
    headers, body = read_response(socket)
    assert_equal("", body, "Wrong response")
    assert(headers.match(/Don't have at least one of the mandatory headers: Connection, Upgrade, Sec-WebSocket-Key and Sec-WebSocket-Version/), "Didn't receive error message")

    request << "Upgrade: websocket\r\n"

    socket = TCPSocket.open(nginx_host, nginx_port)
    socket.print("#{request}\r\n")
    headers, body = read_response(socket)
    assert_equal("", body, "Wrong response")
    assert(headers.match(/Don't have at least one of the mandatory headers: Connection, Upgrade, Sec-WebSocket-Key and Sec-WebSocket-Version/), "Didn't receive error message")


    request << "Sec-WebSocket-Version: 8\r\n"

    socket = TCPSocket.open(nginx_host, nginx_port)
    socket.print("#{request}\r\n")
    headers, body = read_response(socket)
    assert_equal("", body, "Wrong response")
    assert(!headers.match(/Don't have at least one of the mandatory headers: Connection, Upgrade, Sec-WebSocket-Key and Sec-WebSocket-Version/), "Didn't receive error message")
    assert(headers.match(/HTTP\/1\.1 101 Switching Protocols/), "Didn't receive 'Switching Protocols' status")
  end

  def test_supported_versions
    channel = 'ch_test_supported_versions'
    request = "GET /ws/#{channel}.b1 HTTP/1.0\r\nConnection: Upgrade\r\nSec-WebSocket-Key: /mQoZf6pRiv8+6o72GncLQ==\r\nUpgrade: websocket\r\n"

    socket = TCPSocket.open(nginx_host, nginx_port)
    socket.print("#{request}Sec-WebSocket-Version: 7\r\n\r\n")
    headers, body = read_response(socket)
    assert_equal("", body, "Wrong response")
    assert(headers.match(/Sec-WebSocket-Version: 8, 13/), "Didn't receive error message")
    assert(headers.match(/X-Nginx-PushStream-Explain: Version not supported. Supported versions: 8, 13/), "Didn't receive error message")

    socket = TCPSocket.open(nginx_host, nginx_port)
    socket.print("#{request}Sec-WebSocket-Version: 8\r\n\r\n")
    headers, body = read_response(socket)
    assert_equal("", body, "Wrong response")
    assert(!headers.match(/Sec-WebSocket-Version: 8, 13/), "Didn't receive error message")
    assert(!headers.match(/X-Nginx-PushStream-Explain: Version not supported. Supported versions: 8, 13/), "Didn't receive error message")

    socket = TCPSocket.open(nginx_host, nginx_port)
    socket.print("#{request}Sec-WebSocket-Version: 13\r\n\r\n")
    headers, body = read_response(socket)
    assert_equal("", body, "Wrong response")
    assert(!headers.match(/Sec-WebSocket-Version: 8, 13/), "Didn't receive error message")
    assert(!headers.match(/X-Nginx-PushStream-Explain: Version not supported. Supported versions: 8, 13/), "Didn't receive error message")
  end

  def test_response_headers
    channel = 'ch_test_response_headers'
    request = "GET /ws/#{channel}.b1 HTTP/1.0\r\nConnection: Upgrade\r\nSec-WebSocket-Key: /mQoZf6pRiv8+6o72GncLQ==\r\nUpgrade: websocket\r\nSec-WebSocket-Version: 8\r\n"

    socket = TCPSocket.open(nginx_host, nginx_port)
    socket.print("#{request}\r\n")
    headers, body = read_response(socket)
    assert_equal("", body, "Wrong response")
    assert(headers.match(/HTTP\/1\.1 101 Switching Protocols/), "Didn't receive status header")
    assert(headers.match(/Sec-WebSocket-Accept: RaIOIcQ6CBoc74B9EKdH0avYZnw=/), "Didn't receive accept header")
    assert(headers.match(/Upgrade: WebSocket/), "Didn't receive upgrade header")
    assert(headers.match(/Connection: Upgrade/), "Didn't receive connection header")
  end

  def config_test_receive_header_template
    @header_template = "HEADER_TEMPLATE"
  end

  def test_receive_header_template
    channel = 'ch_test_receive_header_template'
    request = "GET /ws/#{channel}.b1 HTTP/1.0\r\nConnection: Upgrade\r\nSec-WebSocket-Key: /mQoZf6pRiv8+6o72GncLQ==\r\nUpgrade: websocket\r\nSec-WebSocket-Version: 8\r\n"

    socket = TCPSocket.open(nginx_host, nginx_port)
    socket.print("#{request}\r\n")
    sleep(0.5)
    headers, body = read_response(socket)
    assert_equal("\201\017HEADER_TEMPLATE", body, "Wrong response")
  end

  def config_test_receive_ping_frame
    @ping_message_interval = '1s'
  end

  def test_receive_ping_frame
    channel = 'ch_test_receive_ping_frame'
    request = "GET /ws/#{channel}.b1 HTTP/1.0\r\nConnection: Upgrade\r\nSec-WebSocket-Key: /mQoZf6pRiv8+6o72GncLQ==\r\nUpgrade: websocket\r\nSec-WebSocket-Version: 8\r\n"

    socket = TCPSocket.open(nginx_host, nginx_port)
    socket.print("#{request}\r\n")
    headers, body = read_response(socket)
    #wait for ping message
    sleep(1)
    body, dummy = read_response(socket)
    assert_equal("\211\000", body, "Wrong response")
  end

  def config_test_receive_close_frame
    @subscriber_connection_timeout = '1s'
  end

  def test_receive_close_frame
    channel = 'ch_test_receive_close_frame'
    request = "GET /ws/#{channel}.b1 HTTP/1.0\r\nConnection: Upgrade\r\nSec-WebSocket-Key: /mQoZf6pRiv8+6o72GncLQ==\r\nUpgrade: websocket\r\nSec-WebSocket-Version: 8\r\n"

    socket = TCPSocket.open(nginx_host, nginx_port)
    socket.print("#{request}\r\n")
    headers, body = read_response(socket)
    #wait for disconnect
    sleep(1)
    body, dummy = read_response(socket)
    assert_equal("\210\000", body, "Wrong response")
  end

  def config_test_receive_footer_template
    @footer_template = "FOOTER_TEMPLATE"
    @subscriber_connection_timeout = '1s'
  end

  def test_receive_footer_template
    channel = 'ch_test_receive_footer_template'
    request = "GET /ws/#{channel}.b1 HTTP/1.0\r\nConnection: Upgrade\r\nSec-WebSocket-Key: /mQoZf6pRiv8+6o72GncLQ==\r\nUpgrade: websocket\r\nSec-WebSocket-Version: 8\r\n"

    socket = TCPSocket.open(nginx_host, nginx_port)
    socket.print("#{request}\r\n")
    headers, body = read_response(socket)
    #wait for disconnect
    sleep(1.5)
    body, dummy = read_response(socket)
    assert_equal("\201\017FOOTER_TEMPLATE\210\000", body, "Wrong response")
  end

  def test_receive_message_length_less_than_125
    channel = 'ch_test_receive_message_length_less_than_125'
    request = "GET /ws/#{channel}.b1 HTTP/1.0\r\nConnection: Upgrade\r\nSec-WebSocket-Key: /mQoZf6pRiv8+6o72GncLQ==\r\nUpgrade: websocket\r\nSec-WebSocket-Version: 8\r\n"

    socket = TCPSocket.open(nginx_host, nginx_port)
    socket.print("#{request}\r\n")
    headers, body = read_response(socket)

    publish_message(channel, {}, "Hello")

    body, dummy = read_response(socket)
    assert_equal("\201\005Hello", body, "Wrong response")
  end

  def config_test_receive_message_length_more_than_125_less_then_65535
    @client_max_body_size = '65k'
    @client_body_buffer_size = '65k'
  end

  def test_receive_message_length_more_than_125_less_then_65535
    message = ""
    channel = 'ch_test_receive_message_length_more_than_125_less_then_65535'
    request = "GET /ws/#{channel}.b1 HTTP/1.0\r\nConnection: Upgrade\r\nSec-WebSocket-Key: /mQoZf6pRiv8+6o72GncLQ==\r\nUpgrade: websocket\r\nSec-WebSocket-Version: 8\r\n"

    65535.times { message << "a" }

    publish_message(channel, {}, message)

    socket = TCPSocket.open(nginx_host, nginx_port)
    socket.print("#{request}\r\n")
    headers, body = read_response(socket, "aaa")
    assert(body.start_with?("\201~\377\377aaa"), "Wrong response")
  end

  def config_test_receive_message_length_more_than_65535
    @client_max_body_size = '70k'
    @client_body_buffer_size = '70k'
  end

  def test_receive_message_length_more_than_65535
    message = ""
    channel = 'ch_test_receive_message_length_more_than_65535'
    request = "GET /ws/#{channel}.b1 HTTP/1.0\r\nConnection: Upgrade\r\nSec-WebSocket-Key: /mQoZf6pRiv8+6o72GncLQ==\r\nUpgrade: websocket\r\nSec-WebSocket-Version: 8\r\n"

    65536.times { message << "a" }

    publish_message(channel, {}, message)

    socket = TCPSocket.open(nginx_host, nginx_port)
    socket.print("#{request}\r\n")
    headers, body = read_response(socket, "aaa")
    assert(body.start_with?("\201\177\000\000\000\000\000\001\000\000aaa"), "Wrong response")
  end

  def config_test_same_message_template_different_locations
    @message_template = '{\"text\":\"~text~\"}'
    @subscriber_connection_timeout = '1s'
  end

  def test_same_message_template_different_locations
    channel = 'ch_test_same_message_template_different_locations'
    body = 'body'

    publish_message(channel, {}, body)

    request_1 = "GET /ws/#{channel}.b1 HTTP/1.0\r\nConnection: Upgrade\r\nSec-WebSocket-Key: /mQoZf6pRiv8+6o72GncLQ==\r\nUpgrade: websocket\r\nSec-WebSocket-Version: 8\r\n"
    request_2 = "GET /sub/#{channel}.b1 HTTP/1.0\r\n"

    socket_1 = TCPSocket.open(nginx_host, nginx_port)
    socket_1.print("#{request_1}\r\n")
    headers_1, body_1 = read_response(socket_1)
    assert_equal("\201\017{\"text\":\"#{body}\"}", body_1, "Wrong message")

    socket_2 = TCPSocket.open(nginx_host, nginx_port)
    socket_2.print("#{request_2}\r\n")
    headers_2, body_2 = read_response(socket_2)
    assert_equal("11\r\n{\"text\":\"#{body}\"}\r\n\r\n", body_2, "Wrong message")
  end

  def config_test_publish_message_same_stream
    @extra_location = %q{
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
    @message_template = '{\"channel\":\"~channel~\", \"id\":\"~id~\", \"message\":\"~text~\"}'
  end

  def test_publish_message_same_stream
    channel = 'ch_test_publish_message_same_stream'
    frame = "%c%c%c%c%c%c%c%c%c%c%c" % [0x81, 0x85, 0xBD, 0xD0, 0xE5, 0x2A, 0xD5, 0xB5, 0x89, 0x46, 0xD2] #send 'hello' text

    request = "GET /ws/#{channel}.b1 HTTP/1.0\r\nConnection: Upgrade\r\nSec-WebSocket-Key: /mQoZf6pRiv8+6o72GncLQ==\r\nUpgrade: websocket\r\nSec-WebSocket-Version: 8\r\n"

    socket = TCPSocket.open(nginx_host, nginx_port)
    socket.print("#{request}\r\n")
    headers, body = read_response(socket)
    socket.print(frame)

    EventMachine.run {
      pub = EventMachine::HttpRequest.new(nginx_address + '/channels-stats?id=' + channel.to_s).get :timeout => 30
      pub.callback {
        assert_equal(200, pub.response_header.status, "Request was not accepted")
        assert_not_equal(0, pub.response_header.content_length, "Empty response was received")
        response = JSON.parse(pub.response)
        assert_equal(channel, response["channel"].to_s, "Channel was not recognized")
        assert_equal(1, response["published_messages"].to_i, "Message was not published")
        assert_equal(1, response["stored_messages"].to_i, "Message was not stored")
        assert_equal(1, response["subscribers"].to_i, "Wrong number for subscribers")
        EventMachine.stop
      }
    }

    EventMachine.run {
      sub = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s + '.b1').get :timeout => 30
      sub.stream { |chunk|
        line = JSON.parse(chunk.split("\r\n")[0])
        assert_equal(channel.to_s, line['channel'], "Wrong channel")
        assert_equal('hello', line['message'], "Wrong message")
        assert_equal(1, line['id'].to_i, "Wrong message")
        EventMachine.stop
      }
    }
  end

  def test_accept_pong_message
    channel = 'ch_test_accept_pong_message'
    frame = "%c%c%c%c%c%c" % [0x8A, 0x80, 0xBD, 0xD0, 0xE5, 0x2A] #send 'pong' frame

    request = "GET /ws/#{channel}.b1 HTTP/1.0\r\nConnection: Upgrade\r\nSec-WebSocket-Key: /mQoZf6pRiv8+6o72GncLQ==\r\nUpgrade: websocket\r\nSec-WebSocket-Version: 8\r\n"

    socket = TCPSocket.open(nginx_host, nginx_port)
    socket.print("#{request}\r\n")
    headers, body = read_response(socket)
    socket.print(frame)

    EventMachine.run {
      pub = EventMachine::HttpRequest.new(nginx_address + '/channels-stats?id=' + channel.to_s).get :timeout => 30
      pub.callback {
        assert_equal(200, pub.response_header.status, "Request was not accepted")
        assert_not_equal(0, pub.response_header.content_length, "Empty response was received")
        response = JSON.parse(pub.response)
        assert_equal(channel, response["channel"].to_s, "Channel was not recognized")
        assert_equal(0, response["published_messages"].to_i, "Message was not published")
        assert_equal(0, response["stored_messages"].to_i, "Message was not stored")
        assert_equal(1, response["subscribers"].to_i, "Wrong number for subscribers")
        EventMachine.stop
      }
    }
  end

  def test_accept_close_message
    channel = 'ch_test_accept_close_message'
    frame = "%c%c%c%c%c%c" % [0x88, 0x80, 0xBD, 0xD0, 0xE5, 0x2A] #send 'close' frame

    request = "GET /ws/#{channel}.b1 HTTP/1.0\r\nConnection: Upgrade\r\nSec-WebSocket-Key: /mQoZf6pRiv8+6o72GncLQ==\r\nUpgrade: websocket\r\nSec-WebSocket-Version: 8\r\n"

    socket = TCPSocket.open(nginx_host, nginx_port)
    socket.print("#{request}\r\n")
    headers, body = read_response(socket)
    socket.print(frame)

    EventMachine.run {
      pub = EventMachine::HttpRequest.new(nginx_address + '/channels-stats?id=' + channel.to_s).get :timeout => 30
      pub.callback {
        assert_equal(200, pub.response_header.status, "Request was not accepted")
        assert_not_equal(0, pub.response_header.content_length, "Empty response was received")
        response = JSON.parse(pub.response)
        assert_equal(channel, response["channel"].to_s, "Channel was not recognized")
        assert_equal(0, response["published_messages"].to_i, "Message was not published")
        assert_equal(0, response["stored_messages"].to_i, "Message was not stored")
        assert_equal(0, response["subscribers"].to_i, "Wrong number for subscribers")
        EventMachine.stop
      }
    }
  end

end
