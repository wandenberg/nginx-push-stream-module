require File.expand_path('base_test_case', File.dirname(__FILE__))

class TestKeepalive < Test::Unit::TestCase
  include BaseTestCase

  def config_test_different_operation_with_keepalive
    @keepalive = 'on'
  end

  def test_different_operation_with_keepalive
    channel = 'ch_test_different_operation_with_keepalive'
    body = 'message to be sent'

    get_without_channel_id = "GET /pub HTTP/1.0\r\n\r\n"
    post_channel_message = "POST /pub?id=#{channel} HTTP/1.0\r\nContent-Length: #{body.size}\r\n\r\n#{body}"
    get_channels_stats = "GET /channels-stats HTTP/1.0\r\n\r\n"
    get_channel_stats = "GET /pub?id=#{channel} HTTP/1.0\r\n\r\n"

    socket = TCPSocket.open(nginx_host, nginx_port)

    socket.print(get_without_channel_id)
    headers, body = read_response(socket)
    assert_equal("", body, "Wrong response")
    assert(headers.match(/No channel id provided\./), "Didn't receive error message")

    socket.print(post_channel_message)
    headers, body = read_response(socket)
    assert_equal("{\"channel\": \"#{channel}\", \"published_messages\": \"1\", \"stored_messages\": \"1\", \"subscribers\": \"0\"}\r\n", body, "Wrong response")

    socket.print(get_channels_stats)
    headers, body = read_response(socket)
    assert(body.match(/"channels": "1", "broadcast_channels": "0", "published_messages": "1", "subscribers": "0", "uptime": "[0-9]*", "by_worker": \[\r\n/), "Didn't receive message")
    assert(body.match(/\{"pid": "[0-9]*", "subscribers": "0", "uptime": "[0-9]*"\}/), "Didn't receive message")

    socket.print(get_channel_stats)
    headers, body = read_response(socket)
    assert_equal("{\"channel\": \"#{channel}\", \"published_messages\": \"1\", \"stored_messages\": \"1\", \"subscribers\": \"0\"}\r\n", body, "Wrong response")

  end
end
