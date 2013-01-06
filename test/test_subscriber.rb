require File.expand_path('base_test_case', File.dirname(__FILE__))

class TestSubscriber < Test::Unit::TestCase
  include BaseTestCase

  def config_test_accepted_methods
    @subscriber_connection_timeout = '1s'
  end

  def test_accepted_methods
    # testing OPTIONS method, EventMachine::HttpRequest does not have support to it
    socket = TCPSocket.open(nginx_host, nginx_port)
    socket.print("OPTIONS /sub/ch_test_accepted_methods_0 HTTP/1.0\r\n\r\n")
    headers, body = read_response(socket)
    assert(headers.match(/HTTP\/1\.1 200 OK/), "Didn't receive right header")
    assert(headers.match(/Content-Length: 0/), "Didn't receive right header")

    EventMachine.run {
      multi = EventMachine::MultiRequest.new

      multi.add(:a, EventMachine::HttpRequest.new(nginx_address + '/sub/ch_test_accepted_methods_1').head)
      multi.add(:b, EventMachine::HttpRequest.new(nginx_address + '/sub/ch_test_accepted_methods_2').put(:body => 'body'))
      multi.add(:c, EventMachine::HttpRequest.new(nginx_address + '/sub/ch_test_accepted_methods_3').post)
      multi.add(:d, EventMachine::HttpRequest.new(nginx_address + '/sub/ch_test_accepted_methods_4').delete)
      multi.add(:e, EventMachine::HttpRequest.new(nginx_address + '/sub/ch_test_accepted_methods_5').get)

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

  def test_access_whithout_channel_path
    headers = {'accept' => 'application/json'}

    EventMachine.run {
      sub = EventMachine::HttpRequest.new(nginx_address + '/sub/').get :head => headers, :timeout => 30
      sub.callback {
        assert_equal(0, sub.response_header.content_length, "Should response only with headers")
        assert_equal(400, sub.response_header.status, "Request was not understood as a bad request")
        assert_equal("No channel id provided.", sub.response_header['X_NGINX_PUSHSTREAM_EXPLAIN'], "Didn't receive the right error message")
        EventMachine.stop
      }
    }
  end

  def config_test_multi_channels
    @subscriber_connection_timeout = '1s'
  end

  def test_multi_channels
    EventMachine.run {
      multi = EventMachine::MultiRequest.new

      multi.add(:a, EventMachine::HttpRequest.new(nginx_address + '/sub/ch_multi_channels_1').get)
      multi.add(:b, EventMachine::HttpRequest.new(nginx_address + '/sub/ch_multi_channels_1.b10').get)
      multi.add(:c, EventMachine::HttpRequest.new(nginx_address + '/sub/ch_multi_channels_2/ch_multi_channels_3').get)
      multi.add(:d, EventMachine::HttpRequest.new(nginx_address + '/sub/ch_multi_channels_2.b2/ch_multi_channels_3').get)
      multi.add(:e, EventMachine::HttpRequest.new(nginx_address + '/sub/ch_multi_channels_2/ch_multi_channels_3.b3').get)
      multi.add(:f, EventMachine::HttpRequest.new(nginx_address + '/sub/ch_multi_channels_2.b2/ch_multi_channels_3.b3').get)
      multi.add(:g, EventMachine::HttpRequest.new(nginx_address + '/sub/ch_multi_channels_4.b').get)

      multi.callback  {
        assert_equal(7, multi.responses[:callback].length)
        multi.responses[:callback].each do |name, response|
          assert_equal(200, response.response_header.status, "Subscriber not accepted")
        end

        EventMachine.stop
      }
    }
  end

  def config_test_max_channel_id_length
    @max_channel_id_length = 5
  end

  def test_max_channel_id_length
    headers = {'accept' => 'application/json'}
    channel = '123456'

    EventMachine.run {
      sub = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s ).get :head => headers, :timeout => 30
      sub.callback {
        assert_equal(0, sub.response_header.content_length, "Should response only with headers")
        assert_equal(400, sub.response_header.status, "Request was not understood as a bad request")
        assert_equal("Channel id is too large.", sub.response_header['X_NGINX_PUSHSTREAM_EXPLAIN'], "Didn't receive the right error message")
        EventMachine.stop
      }
    }
  end

  def test_cannot_access_a_channel_with_id_ALL
    headers = {'accept' => 'application/json'}
    channel = 'ALL'

    EventMachine.run {
      sub_1 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => headers, :timeout => 30
      sub_1.callback {
        assert_equal(403, sub_1.response_header.status, "Channel was created")
        assert_equal(0, sub_1.response_header.content_length, "Received response for creating channel with id ALL")
        assert_equal("Channel id not authorized for this method.", sub_1.response_header['X_NGINX_PUSHSTREAM_EXPLAIN'], "Didn't receive the right error message")
        EventMachine.stop
      }
    }
  end

  def test_cannot_access_a_channel_with_id_containing_wildcard
    headers = {'accept' => 'application/json'}
    channel_1 = 'abcd*efgh'
    channel_2 = '*abcdefgh'
    channel_3 = 'abcdefgh*'

    EventMachine.run {
      multi = EventMachine::MultiRequest.new

      multi.add(:a, EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel_1).get(:head => headers, :timeout => 30))
      multi.add(:b, EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel_2).get(:head => headers, :timeout => 30))
      multi.add(:c, EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel_3).get(:head => headers, :timeout => 30))
      multi.callback  {
        assert_equal(3, multi.responses[:callback].length)
        multi.responses[:callback].each do |name, response|
          assert_equal(403, response.response_header.status, "Channel was created")
          assert_equal(0, response.response_header.content_length, "Received response for creating channel with id containing wildcard")
          assert_equal("Channel id not authorized for this method.", response.response_header['X_NGINX_PUSHSTREAM_EXPLAIN'], "Didn't receive the right error message")
        end

        EventMachine.stop
      }

      EM.add_timer(5) do
        fail("Subscribers didn't disconnect")
        EventMachine.stop
      end
    }
  end

  def config_test_broadcast_channels_without_common_channel
    @subscriber_connection_timeout = '1s'
    @broadcast_channel_prefix = "bd_"
  end

  def test_broadcast_channels_without_common_channel
    headers = {'accept' => 'application/json'}

    EventMachine.run {
      multi = EventMachine::MultiRequest.new

      multi.add(:a, EventMachine::HttpRequest.new(nginx_address + '/sub/bd_test_broadcast_channels_without_common_channel').get)
      multi.add(:b, EventMachine::HttpRequest.new(nginx_address + '/sub/bd_').get)
      multi.add(:c, EventMachine::HttpRequest.new(nginx_address + '/sub/bd1').get)
      multi.add(:d, EventMachine::HttpRequest.new(nginx_address + '/sub/bd').get)

      multi.callback  {
        assert_equal(4, multi.responses[:callback].length)

        assert_equal(0, multi.responses[:callback][:a].response_header.content_length, "Should response only with headers")
        assert_equal(403, multi.responses[:callback][:a].response_header.status, "Request was not understood as a bad request")
        assert_equal("Subscribed too much broadcast channels.", multi.responses[:callback][:a].response_header['X_NGINX_PUSHSTREAM_EXPLAIN'], "Didn't receive the right error message")
        assert_equal(nginx_address + '/sub/bd_test_broadcast_channels_without_common_channel', multi.responses[:callback][:a].req.uri.to_s, "Array is with wrong order")

        assert_equal(0, multi.responses[:callback][:b].response_header.content_length, "Should response only with headers")
        assert_equal(403, multi.responses[:callback][:b].response_header.status, "Request was not understood as a bad request")
        assert_equal("Subscribed too much broadcast channels.", multi.responses[:callback][:b].response_header['X_NGINX_PUSHSTREAM_EXPLAIN'], "Didn't receive the right error message")
        assert_equal(nginx_address + '/sub/bd_', multi.responses[:callback][:b].req.uri.to_s, "Array is with wrong order")

        assert_equal(200, multi.responses[:callback][:c].response_header.status, "Channel id starting with different prefix from broadcast was not accept")
        assert_equal(nginx_address + '/sub/bd1', multi.responses[:callback][:c].req.uri.to_s, "Array is with wrong order")

        assert_equal(200, multi.responses[:callback][:d].response_header.status, "Channel id starting with different prefix from broadcast was not accept")
        assert_equal(nginx_address + '/sub/bd', multi.responses[:callback][:d].req.uri.to_s, "Array is with wrong order")

        EventMachine.stop
      }
    }
  end

  def config_test_broadcast_channels_with_common_channels
    @subscriber_connection_timeout = '1s'
    @authorized_channels_only  = "off"
    @broadcast_channel_prefix = "bd_"
    @broadcast_channel_max_qtd = 2
  end

  def test_broadcast_channels_with_common_channels
    headers = {'accept' => 'application/json'}

    EventMachine.run {
      multi = EventMachine::MultiRequest.new

      multi.add(:a, EventMachine::HttpRequest.new(nginx_address + '/sub/bd1/bd2/bd3/bd4/bd_1/bd_2/bd_3').get)
      multi.add(:b, EventMachine::HttpRequest.new(nginx_address + '/sub/bd1/bd2/bd_1/bd_2').get)
      multi.add(:c, EventMachine::HttpRequest.new(nginx_address + '/sub/bd1/bd_1').get)
      multi.add(:d, EventMachine::HttpRequest.new(nginx_address + '/sub/bd1/bd2').get)

      multi.callback  {
        assert_equal(4, multi.responses[:callback].length)

        assert_equal(0, multi.responses[:callback][:a].response_header.content_length, "Should response only with headers")
        assert_equal(403, multi.responses[:callback][:a].response_header.status, "Request was not understood as a bad request")
        assert_equal("Subscribed too much broadcast channels.", multi.responses[:callback][:a].response_header['X_NGINX_PUSHSTREAM_EXPLAIN'], "Didn't receive the right error message")
        assert_equal(nginx_address + '/sub/bd1/bd2/bd3/bd4/bd_1/bd_2/bd_3', multi.responses[:callback][:a].req.uri.to_s, "Array is with wrong order")

        assert_equal(200, multi.responses[:callback][:b].response_header.status, "Request was not understood as a bad request")
        assert_equal(nginx_address + '/sub/bd1/bd2/bd_1/bd_2', multi.responses[:callback][:b].req.uri.to_s, "Array is with wrong order")

        assert_equal(200, multi.responses[:callback][:c].response_header.status, "Channel id starting with different prefix from broadcast was not accept")
        assert_equal(nginx_address + '/sub/bd1/bd_1', multi.responses[:callback][:c].req.uri.to_s, "Array is with wrong order")

        assert_equal(200, multi.responses[:callback][:d].response_header.status, "Channel id starting with different prefix from broadcast was not accept")
        assert_equal(nginx_address + '/sub/bd1/bd2', multi.responses[:callback][:d].req.uri.to_s, "Array is with wrong order")

        EventMachine.stop
      }
    }
  end

  def config_test_subscribe_an_absent_channel_with_authorized_only_on
    @authorized_channels_only = 'on'
  end

  def test_subscribe_an_absent_channel_with_authorized_only_on
    headers = {'accept' => 'application/json'}
    channel = 'ch_test_subscribe_an_absent_channel_with_authorized_only_on'

    EventMachine.run {
      sub_1 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => headers, :timeout => 30
      sub_1.callback {
        assert_equal(403, sub_1.response_header.status, "Channel was founded")
        assert_equal(0, sub_1.response_header.content_length, "Recieved a non empty response")
        assert_equal("Subscriber could not create channels.", sub_1.response_header['X_NGINX_PUSHSTREAM_EXPLAIN'], "Didn't receive the right error message")
        EventMachine.stop
      }
    }
  end

  def config_test_subscribe_an_existing_channel_with_authorized_only_on
    @authorized_channels_only = 'on'
    @subscriber_connection_timeout = '1s'
  end

  def test_subscribe_an_existing_channel_with_authorized_only_on
    headers = {'accept' => 'application/json'}
    channel = 'ch_test_subscribe_an_existing_channel_with_authorized_only_on'
    body = 'body'

    #create channel
    publish_message(channel, headers, body)

    EventMachine.run {
      sub_1 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => headers, :timeout => 30
      sub_1.callback {
        assert_equal(200, sub_1.response_header.status, "Channel was founded")
        EventMachine.stop
      }

      add_test_timeout
    }
  end

  def config_test_subscribe_an_existing_channel_and_absent_broadcast_channel_with_authorized_only_on
    @authorized_channels_only = 'on'
    @subscriber_connection_timeout = '1s'
    @broadcast_channel_prefix = "bd_"
    @broadcast_channel_max_qtd = 1
  end

  def test_subscribe_an_existing_channel_and_absent_broadcast_channel_with_authorized_only_on
    headers = {'accept' => 'application/json'}
    channel = 'ch_test_subscribe_an_existing_channel_and_absent_broadcast_channel_with_authorized_only_on'
    broadcast_channel = 'bd_test_subscribe_an_existing_channel_and_absent_broadcast_channel_with_authorized_only_on'

    body = 'body'

    #create channel
    publish_message(channel, headers, body)

    EventMachine.run {
      sub_1 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s + '/' + broadcast_channel.to_s).get :head => headers, :timeout => 30
      sub_1.callback {
        assert_equal(200, sub_1.response_header.status, "Channel was founded")
        EventMachine.stop
      }
      add_test_timeout
    }
  end

  def config_test_subscribe_an_existing_channel_without_messages_and_with_authorized_only_on
    @min_message_buffer_timeout = '1s'
    @authorized_channels_only = 'on'
  end

  def test_subscribe_an_existing_channel_without_messages_and_with_authorized_only_on
    headers = {'accept' => 'application/json'}
    channel = 'ch_test_subscribe_an_existing_channel_without_messages_and_with_authorized_only_on'

    body = 'body'

    #create channel
    publish_message(channel, headers, body)
    sleep(5) #to ensure message was gone

    EventMachine.run {
      sub_1 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => headers, :timeout => 30
      sub_1.callback {
        assert_equal(403, sub_1.response_header.status, "Channel was founded")
        assert_equal(0, sub_1.response_header.content_length, "Recieved a non empty response")
        assert_equal("Subscriber could not create channels.", sub_1.response_header['X_NGINX_PUSHSTREAM_EXPLAIN'], "Didn't receive the right error message")
        EventMachine.stop
      }
      add_test_timeout
    }
  end

  def config_test_subscribe_an_existing_channel_without_messages_and_absent_broadcast_channel_and_with_authorized_only_on_should_fail
    @min_message_buffer_timeout = '1s'
    @authorized_channels_only = 'on'
    @broadcast_channel_prefix = "bd_"
    @broadcast_channel_max_qtd = 1
  end

  def test_subscribe_an_existing_channel_without_messages_and_absent_broadcast_channel_and_with_authorized_only_on_should_fail
    headers = {'accept' => 'application/json'}
    channel = 'ch_test_subscribe_an_existing_channel_without_messages_and_absent_broadcast_channel_and_with_authorized_only_on_should_fail'
    broadcast_channel = 'bd_test_subscribe_an_existing_channel_without_messages_and_absent_broadcast_channel_and_with_authorized_only_on_should_fail'

    body = 'body'

    #create channel
    publish_message(channel, headers, body)
    sleep(5) #to ensure message was gone

    EventMachine.run {
      sub_1 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s + '/' + broadcast_channel.to_s).get :head => headers, :timeout => 30
      sub_1.callback {
        assert_equal(403, sub_1.response_header.status, "Channel was founded")
        assert_equal(0, sub_1.response_header.content_length, "Recieved a non empty response")
        assert_equal("Subscriber could not create channels.", sub_1.response_header['X_NGINX_PUSHSTREAM_EXPLAIN'], "Didn't receive the right error message")
        EventMachine.stop
      }
      add_test_timeout
    }
  end

  def config_test_retreive_old_messages_in_multichannel_subscribe
    @header_template = 'HEADER'
    @message_template = '{\"channel\":\"~channel~\", \"id\":\"~id~\", \"message\":\"~text~\"}'
  end

  def test_retreive_old_messages_in_multichannel_subscribe
    headers = {'accept' => 'application/json'}
    channel_1 = 'ch_test_retreive_old_messages_in_multichannel_subscribe_1'
    channel_2 = 'ch_test_retreive_old_messages_in_multichannel_subscribe_2'
    channel_3 = 'ch_test_retreive_old_messages_in_multichannel_subscribe_3'

    body = 'body'

    #create channels with some messages
    1.upto(3) do |i|
      publish_message(channel_1, headers, body + i.to_s)
      publish_message(channel_2, headers, body + i.to_s)
      publish_message(channel_3, headers, body + i.to_s)
    end

    response = ""
    EventMachine.run {
      sub_1 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel_1.to_s + '/' + channel_2.to_s + '.b5' + '/' + channel_3.to_s + '.b2').get :head => headers, :timeout => 30
      sub_1.stream { |chunk|
        response += chunk
        lines = response.split("\r\n")

        if lines.length >= 6
          assert_equal('HEADER', lines[0], "Header was not received")
          line = JSON.parse(lines[1])
          assert_equal(channel_2.to_s, line['channel'], "Wrong channel")
          assert_equal('body1', line['message'], "Wrong message")
          assert_equal(1, line['id'].to_i, "Wrong message")

          line = JSON.parse(lines[2])
          assert_equal(channel_2.to_s, line['channel'], "Wrong channel")
          assert_equal('body2', line['message'], "Wrong message")
          assert_equal(2, line['id'].to_i, "Wrong message")

          line = JSON.parse(lines[3])
          assert_equal(channel_2.to_s, line['channel'], "Wrong channel")
          assert_equal('body3', line['message'], "Wrong message")
          assert_equal(3, line['id'].to_i, "Wrong message")

          line = JSON.parse(lines[4])
          assert_equal(channel_3.to_s, line['channel'], "Wrong channel")
          assert_equal('body2', line['message'], "Wrong message")
          assert_equal(2, line['id'].to_i, "Wrong message")

          line = JSON.parse(lines[5])
          assert_equal(channel_3.to_s, line['channel'], "Wrong channel")
          assert_equal('body3', line['message'], "Wrong message")
          assert_equal(3, line['id'].to_i, "Wrong message")

          EventMachine.stop
        end
      }
      add_test_timeout(10)
    }
  end

  def config_test_retreive_new_messages_in_multichannel_subscribe
    @header_template = nil
    @message_template = '{\"channel\":\"~channel~\", \"id\":\"~id~\", \"message\":\"~text~\"}'
  end

  def test_retreive_new_messages_in_multichannel_subscribe
    headers = {'accept' => 'application/json'}
    channel_1 = 'test_retreive_new_messages_in_multichannel_subscribe_1'
    channel_2 = 'test_retreive_new_messages_in_multich_subscribe_2'
    channel_3 = 'test_retreive_new_messages_in_multchannel_subscribe_3'
    channel_4 = 'test_retreive_new_msgs_in_multichannel_subscribe_4'
    channel_5 = 'test_retreive_new_messages_in_multichannel_subs_5'
    channel_6 = 'test_retreive_new_msgs_in_multichannel_subs_6'

    body = 'body'

    response = ""
    EventMachine.run {
      sub_1 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel_1.to_s + '/' + channel_2.to_s + '/' + channel_3.to_s + '/' + channel_4.to_s + '/' + channel_5.to_s + '/' + channel_6.to_s).get :head => headers, :timeout => 30
      sub_1.stream { |chunk|
        response += chunk
        lines = response.split("\r\n")

        if lines.length >= 6
          line = JSON.parse(lines[0])
          assert_equal(channel_1.to_s, line['channel'], "Wrong channel")
          assert_equal('body' + channel_1.to_s, line['message'], "Wrong message")
          assert_equal(1, line['id'].to_i, "Wrong message")

          line = JSON.parse(lines[1])
          assert_equal(channel_2.to_s, line['channel'], "Wrong channel")
          assert_equal('body' + channel_2.to_s, line['message'], "Wrong message")
          assert_equal(1, line['id'].to_i, "Wrong message")

          line = JSON.parse(lines[2])
          assert_equal(channel_3.to_s, line['channel'], "Wrong channel")
          assert_equal('body' + channel_3.to_s, line['message'], "Wrong message")
          assert_equal(1, line['id'].to_i, "Wrong message")

          line = JSON.parse(lines[3])
          assert_equal(channel_4.to_s, line['channel'], "Wrong channel")
          assert_equal('body' + channel_4.to_s, line['message'], "Wrong message")
          assert_equal(1, line['id'].to_i, "Wrong message")

          line = JSON.parse(lines[4])
          assert_equal(channel_5.to_s, line['channel'], "Wrong channel")
          assert_equal('body' + channel_5.to_s, line['message'], "Wrong message")
          assert_equal(1, line['id'].to_i, "Wrong message")

          line = JSON.parse(lines[5])
          assert_equal(channel_6.to_s, line['channel'], "Wrong channel")
          assert_equal('body' + channel_6.to_s, line['message'], "Wrong message")
          assert_equal(1, line['id'].to_i, "Wrong message")

          EventMachine.stop
        end
      }

      publish_message_inline(channel_1, headers, body + channel_1.to_s)
      publish_message_inline(channel_2, headers, body + channel_2.to_s)
      publish_message_inline(channel_3, headers, body + channel_3.to_s)
      publish_message_inline(channel_4, headers, body + channel_4.to_s)
      publish_message_inline(channel_5, headers, body + channel_5.to_s)
      publish_message_inline(channel_6, headers, body + channel_6.to_s)

      add_test_timeout(10)
    }

  end

  def config_test_retreive_old_messages_in_multichannel_subscribe_using_if_modified_since_header
    @header_template = 'HEADER'
    @message_template = '{\"channel\":\"~channel~\", \"id\":\"~id~\", \"message\":\"~text~\"}'
  end

  def test_retreive_old_messages_in_multichannel_subscribe_using_if_modified_since_header
    headers = {'accept' => 'application/json'}
    channel_1 = 'ch_test_retreive_old_messages_in_multichannel_subscribe_using_if_modified_since_header_1'
    channel_2 = 'ch_test_retreive_old_messages_in_multichannel_subscribe_using_if_modified_since_header_2'
    channel_3 = 'ch_test_retreive_old_messages_in_multichannel_subscribe_using_if_modified_since_header_3'

    body = 'body'

    #create channels with some messages with progressive interval (2,4,6,10,14,18,24,30,36 seconds)
    1.upto(3) do |i|
      sleep(i * 2)
      publish_message(channel_1, headers, body + i.to_s)
      sleep(i * 2)
      publish_message(channel_2, headers, body + i.to_s)
      sleep(i * 2)
      publish_message(channel_3, headers, body + i.to_s)
    end

    #get messages published less then 20 seconds ago
    t = Time.now
    t = t - 20

    headers = headers.merge({'If-Modified-Since' => t.utc.strftime("%a, %d %b %Y %T %Z")})

    response = ""
    EventMachine.run {
      sub_1 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel_1.to_s + '/' + channel_2.to_s + '/' + channel_3.to_s).get :head => headers, :timeout => 30
      sub_1.stream { |chunk|
        response += chunk
        lines = response.split("\r\n")

        if lines.length >= 5
          assert_equal('HEADER', lines[0], "Header was not received")

          line = JSON.parse(lines[1])
          assert_equal(channel_1.to_s, line['channel'], "Wrong channel")
          assert_equal('body3', line['message'], "Wrong message")
          assert_equal(3, line['id'].to_i, "Wrong message")

          line = JSON.parse(lines[2])
          assert_equal(channel_2.to_s, line['channel'], "Wrong channel")
          assert_equal('body3', line['message'], "Wrong message")
          assert_equal(3, line['id'].to_i, "Wrong message")

          line = JSON.parse(lines[3])
          assert_equal(channel_3.to_s, line['channel'], "Wrong channel")
          assert_equal('body2', line['message'], "Wrong message")
          assert_equal(2, line['id'].to_i, "Wrong message")

          line = JSON.parse(lines[4])
          assert_equal(channel_3.to_s, line['channel'], "Wrong channel")
          assert_equal('body3', line['message'], "Wrong message")
          assert_equal(3, line['id'].to_i, "Wrong message")

          EventMachine.stop
        end
      }

      add_test_timeout
    }
  end

  def config_test_retreive_old_messages_in_multichannel_subscribe_using_if_modified_since_header_and_backtrack_mixed
    @header_template = 'HEADER'
    @message_template = '{\"channel\":\"~channel~\", \"id\":\"~id~\", \"message\":\"~text~\"}'
  end

  def test_retreive_old_messages_in_multichannel_subscribe_using_if_modified_since_header_and_backtrack_mixed
    headers = {'accept' => 'application/json'}
    channel_1 = 'ch_test_retreive_old_messages_in_multichannel_subscribe_using_if_modified_since_header_and_backtrack_mixed_1'
    channel_2 = 'ch_test_retreive_old_messages_in_multichannel_subscribe_using_if_modified_since_header_and_backtrack_mixed_2'
    channel_3 = 'ch_test_retreive_old_messages_in_multichannel_subscribe_using_if_modified_since_header_and_backtrack_mixed_3'

    body = 'body'

    #create channels with some messages with progressive interval (2,4,6,10,14,18,24,30,36 seconds)
    1.upto(3) do |i|
      sleep(i * 2)
      publish_message(channel_1, headers, body + i.to_s)
      sleep(i * 2)
      publish_message(channel_2, headers, body + i.to_s)
      sleep(i * 2)
      publish_message(channel_3, headers, body + i.to_s)
    end

    #get messages published less then 20 seconds ago
    t = Time.now
    t = t - 20

    headers = headers.merge({'If-Modified-Since' => t.utc.strftime("%a, %d %b %Y %T %Z")})

    response = ""
    EventMachine.run {
      sub_1 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel_1.to_s + '/' + channel_2.to_s + '.b5' + '/' + channel_3.to_s).get :head => headers, :timeout => 30
      sub_1.stream { |chunk|
        response += chunk
        lines = response.split("\r\n")

        if lines.length >= 7
          assert_equal('HEADER', lines[0], "Header was not received")

          line = JSON.parse(lines[1])
          assert_equal(channel_1.to_s, line['channel'], "Wrong channel")
          assert_equal('body3', line['message'], "Wrong message")
          assert_equal(3, line['id'].to_i, "Wrong message")

          line = JSON.parse(lines[2])
          assert_equal(channel_2.to_s, line['channel'], "Wrong channel")
          assert_equal('body1', line['message'], "Wrong message")
          assert_equal(1, line['id'].to_i, "Wrong message")

          line = JSON.parse(lines[3])
          assert_equal(channel_2.to_s, line['channel'], "Wrong channel")
          assert_equal('body2', line['message'], "Wrong message")
          assert_equal(2, line['id'].to_i, "Wrong message")

          line = JSON.parse(lines[4])
          assert_equal(channel_2.to_s, line['channel'], "Wrong channel")
          assert_equal('body3', line['message'], "Wrong message")
          assert_equal(3, line['id'].to_i, "Wrong message")

          line = JSON.parse(lines[5])
          assert_equal(channel_3.to_s, line['channel'], "Wrong channel")
          assert_equal('body2', line['message'], "Wrong message")
          assert_equal(2, line['id'].to_i, "Wrong message")

          line = JSON.parse(lines[6])
          assert_equal(channel_3.to_s, line['channel'], "Wrong channel")
          assert_equal('body3', line['message'], "Wrong message")
          assert_equal(3, line['id'].to_i, "Wrong message")

          EventMachine.stop
        end
      }

      add_test_timeout
    }
  end

  def config_test_max_number_of_channels
    @max_number_of_channels = 1
  end

  def test_max_number_of_channels
    headers = {'accept' => 'application/json'}
    channel = 'ch_test_max_number_of_channels_'

    EventMachine.run {
      sub_1 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s + 1.to_s).get :head => headers, :timeout => 30
      sub_1.stream {
        assert_equal(200, sub_1.response_header.status, "Channel was not created")
        assert_not_equal(0, sub_1.response_header.content_length, "Should response channel info")

        sub_2 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s + 2.to_s).get :head => headers, :timeout => 30
        sub_2.callback {
          assert_equal(403, sub_2.response_header.status, "Request was not forbidden")
          assert_equal(0, sub_2.response_header.content_length, "Should response only with headers")
          assert_equal("Number of channels were exceeded.", sub_2.response_header['X_NGINX_PUSHSTREAM_EXPLAIN'], "Didn't receive the right error message")
          EventMachine.stop
        }
      }
      add_test_timeout(10)
    }

  end

  def config_test_max_number_of_broadcast_channels
    @max_number_of_broadcast_channels = 1
    @broadcast_channel_prefix = 'bd_'
    @broadcast_channel_max_qtd = 1
  end

  def test_max_number_of_broadcast_channels
    headers = {'accept' => 'application/json'}
    channel = 'bd_test_max_number_of_broadcast_channels_'

    EventMachine.run {
      sub_1 = EventMachine::HttpRequest.new(nginx_address + '/sub/ch1/' + channel.to_s + 1.to_s).get :head => headers, :timeout => 30
      sub_1.stream {
        assert_equal(200, sub_1.response_header.status, "Channel was not created")
        assert_not_equal(0, sub_1.response_header.content_length, "Should response channel info")

        sub_2 = EventMachine::HttpRequest.new(nginx_address + '/sub/ch1/' + channel.to_s + 2.to_s).get :head => headers, :timeout => 30
        sub_2.callback {
          assert_equal(403, sub_2.response_header.status, "Request was not forbidden")
          assert_equal(0, sub_2.response_header.content_length, "Should response only with headers")
          assert_equal("Number of channels were exceeded.", sub_2.response_header['X_NGINX_PUSHSTREAM_EXPLAIN'], "Didn't receive the right error message")
          EventMachine.stop
        }
      }
      add_test_timeout
    }
  end

  def config_test_different_message_templates
    @message_template = '{\"text\":\"~text~\"}'
    @header_template = nil
    @subscriber_connection_timeout = '1s'
    @extra_location = %q{
              location ~ /sub2/(.*)? {
                # activate subscriber mode for this location
                push_stream_subscriber;

                # positional channel path
                set $push_stream_channels_path          $1;
                # message template
                push_stream_message_template "{\"msg\":\"~text~\"}";
            }

    }
  end

  def test_different_message_templates
    headers = {'accept' => 'application/json'}
    channel = 'ch_test_different_message_templates'
    body = 'body'

    EventMachine.run {
      sub_1 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => headers, :timeout => 30
      sub_1.stream { |chunk|
        response = JSON.parse(chunk)
        assert_equal(true, response.has_key?('text'), "Wrong message template")
        assert_equal(false, response.has_key?('msg'), "Wrong message template")
        assert_equal(body, response['text'], "Wrong message")
        EventMachine.stop
      }

      sub_2 = EventMachine::HttpRequest.new(nginx_address + '/sub2/' + channel.to_s + '.b1').get :head => headers, :timeout => 30
      sub_2.stream { |chunk|
        response = JSON.parse(chunk)
        assert_equal(false, response.has_key?('text'), "Wrong message template")
        assert_equal(true, response.has_key?('msg'), "Wrong message template")
        assert_equal(body, response['msg'], "Wrong message")
        EventMachine.stop
      }

      #publish a message
      publish_message_inline(channel, headers, body)
      add_test_timeout
    }

    EventMachine.run {
      sub_3 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s + '.b1').get :head => headers, :timeout => 30
      sub_3.stream { |chunk|
        response = JSON.parse(chunk)
        assert_equal(true, response.has_key?('text'), "Wrong message template")
        assert_equal(false, response.has_key?('msg'), "Wrong message template")
        assert_equal(body, response['text'], "Wrong message")
        EventMachine.stop
      }
      add_test_timeout
    }

    EventMachine.run {
      sub_4 = EventMachine::HttpRequest.new(nginx_address + '/sub2/' + channel.to_s + '.b1').get :head => headers, :timeout => 30
      sub_4.stream { |chunk|
        response = JSON.parse(chunk)
        assert_equal(false, response.has_key?('text'), "Wrong message template")
        assert_equal(true, response.has_key?('msg'), "Wrong message template")
        assert_equal(body, response['msg'], "Wrong message")
        EventMachine.stop
      }
      add_test_timeout
    }
  end

  def config_test_default_message_template
    @message_template = nil
    @header_template = nil
  end

  def test_default_message_template
    headers = {'accept' => 'application/json'}
    channel = 'ch_test_default_message_template'
    body = 'body'

    EventMachine.run {
      sub_1 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => headers, :timeout => 30
      sub_1.stream { |chunk|
        assert_equal("#{body}\r\n", chunk, "Wrong message")
        EventMachine.stop
      }

      #publish a message
      publish_message_inline(channel, headers, body)
      add_test_timeout
    }
  end

  def config_test_ping_message_with_default_message_template
    @message_template = nil
    @header_template = nil
    @ping_message_interval = '1s'
  end

  def test_ping_message_with_default_message_template
    headers = {'accept' => 'application/json'}
    channel = 'ch_test_ping_message_with_default_message_template'
    body = 'body'

    EventMachine.run {
      sub_1 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => headers, :timeout => 30
      sub_1.stream { |chunk|
        assert_equal("\r\n", chunk, "Wrong message")
        EventMachine.stop
      }

      add_test_timeout
    }
  end

  def test_transfer_encoding_chuncked
    headers = {'accept' => 'application/json'}
    channel = 'ch_test_transfer_encoding_chuncked'
    EventMachine.run {
      sub_1 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => headers, :timeout => 30
      sub_1.stream { |chunk|
        assert_equal("chunked", sub_1.response_header['TRANSFER_ENCODING'], "Didn't receive the right transfer  encoding")
        EventMachine.stop
      }
      add_test_timeout
    }
  end

  def config_test_default_ping_message_with_default_message_template
    @header_template = nil
    @message_template = nil
    @ping_message_text = nil
    @ping_message_interval = '1s'
  end

  def test_default_ping_message_with_default_message_template
    headers = {'accept' => 'application/json'}
    channel = 'ch_test_default_ping_message_with_default_message_template'
    body = 'body'

    EventMachine.run {
      sub_1 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => headers, :timeout => 30
      sub_1.stream { |chunk|
        assert_equal("\r\n", chunk, "Wrong message")
        EventMachine.stop
      }

      add_test_timeout
    }
  end

  def config_test_custom_ping_message_with_default_message_template
    @header_template = nil
    @message_template = nil
    @ping_message_text = "pinging you!!!"
    @ping_message_interval = '1s'
  end

  def test_custom_ping_message_with_default_message_template
    headers = {'accept' => 'application/json'}
    channel = 'ch_test_custom_ping_message_with_default_message_template'
    body = 'body'

    EventMachine.run {
      sub_1 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => headers, :timeout => 30
      sub_1.stream { |chunk|
        assert_equal("#{@ping_message_text}\r\n", chunk, "Wrong message")
        EventMachine.stop
      }

      add_test_timeout
    }
  end

  def config_test_default_ping_message_with_custom_message_template
    @header_template = nil
    @message_template = "~id~:~text~"
    @ping_message_text = nil
    @ping_message_interval = '1s'
  end

  def test_default_ping_message_with_custom_message_template
    headers = {'accept' => 'application/json'}
    channel = 'ch_test_default_ping_message_with_custom_message_template'
    body = 'body'

    EventMachine.run {
      sub_1 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => headers, :timeout => 30
      sub_1.stream { |chunk|
        assert_equal("-1:\r\n", chunk, "Wrong message")
        EventMachine.stop
      }

      add_test_timeout
    }
  end

  def config_test_custom_ping_message_with_default_message_template
    @header_template = nil
    @message_template = "~id~:~text~"
    @ping_message_text = "pinging you!!!"
    @ping_message_interval = '1s'
  end

  def test_custom_ping_message_with_default_message_template
    headers = {'accept' => 'application/json'}
    channel = 'ch_test_custom_ping_message_with_default_message_template'
    body = 'body'

    EventMachine.run {
      sub_1 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => headers, :timeout => 30
      sub_1.stream { |chunk|
        assert_equal("-1:#{@ping_message_text}\r\n", chunk, "Wrong message")
        EventMachine.stop
      }

      add_test_timeout
    }
  end

  def config_test_cannot_add_more_subscriber_to_one_channel_than_allowed
    @max_subscribers_per_channel = 3
    @subscriber_connection_timeout = "3s"
  end

  def test_cannot_add_more_subscriber_to_one_channel_than_allowed
    headers = {'accept' => 'application/json'}
    channel = 'ch_test_cannot_add_more_subscriber_to_one_channel_than_allowed'
    other_channel = 'ch_test_cannot_add_more_subscriber_to_one_channel_than_allowed_2'

    EventMachine.run {
      sub_1 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => headers, :timeout => 30
      sub_2 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => headers, :timeout => 30
      sub_3 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => headers, :timeout => 30
      sub_4 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => headers, :timeout => 30
      sub_4.callback {
        assert_equal(403, sub_4.response_header.status, "Channel was created")
        assert_equal(0, sub_4.response_header.content_length, "Received response for exceed subscriber limit")
        assert_equal("Subscribers limit per channel has been exceeded.", sub_4.response_header['X_NGINX_PUSHSTREAM_EXPLAIN'], "Didn't receive the right error message")
      }
      sub_5 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + other_channel.to_s).get :head => headers, :timeout => 30
      sub_5.callback {
        assert_equal(200, sub_5.response_header.status, "Channel was not created")
        EventMachine.stop
      }

      add_test_timeout
    }
  end

  def config_test_accept_channels_name_with_dot_b
    @subscriber_connection_timeout = "1s"
    @ping_message_interval = nil
    @header_template = nil
    @footer_template = nil
    @message_template = nil
  end

  def test_accept_channels_name_with_dot_b
    channel = 'room.b18.beautiful'
    response = ''

    EventMachine.run {
      publish_message_inline(channel, {'accept' => 'text/html'}, 'msg 1')
      publish_message_inline(channel, {'accept' => 'text/html'}, 'msg 2')
      publish_message_inline(channel, {'accept' => 'text/html'}, 'msg 3')
      publish_message_inline(channel, {'accept' => 'text/html'}, 'msg 4')

      sub = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s + '.b3').get
      sub.stream { | chunk |
        response += chunk
      }
      sub.callback {
        assert_equal("msg 2\r\nmsg 3\r\nmsg 4\r\n", response, "The published message was not received correctly")

        response = ''
        sub_1 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get
        sub_1.stream { | chunk |
          response += chunk
        }
        sub_1.callback { |chunk|
          assert_equal("msg 5\r\n", response, "The published message was not received correctly")

          EventMachine.stop
        }

        publish_message_inline(channel, {'accept' => 'text/html'}, 'msg 5')
      }

      add_test_timeout
    }
  end

  def test_access_control_allow_headers
    headers = {'accept' => 'application/json'}
    channel = 'test_access_control_allow_headers'

    EventMachine.run {
      sub_1 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => headers, :timeout => 30
      sub_1.stream { |chunk|
        assert_equal("*", sub_1.response_header['ACCESS_CONTROL_ALLOW_ORIGIN'], "Didn't receive the right header")
        assert_equal("GET", sub_1.response_header['ACCESS_CONTROL_ALLOW_METHODS'], "Didn't receive the right header")
        assert_equal("If-Modified-Since,If-None-Match", sub_1.response_header['ACCESS_CONTROL_ALLOW_HEADERS'], "Didn't receive the right header")
        EventMachine.stop
      }

      add_test_timeout
    }
  end

  def test_default_access_control_allow_origin_header
    headers = {'accept' => 'application/json'}
    channel = 'test_default_access_control_allow_origin_header'

    EventMachine.run {
      sub_1 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => headers, :timeout => 30
      sub_1.stream { |chunk|
        assert_equal("*", sub_1.response_header['ACCESS_CONTROL_ALLOW_ORIGIN'], "Didn't receive the right header")
        EventMachine.stop
      }

      add_test_timeout
    }
  end

  def config_test_custom_access_control_allow_origin_header
    @allowed_origins = "custom.domain.com"
  end

  def test_custom_access_control_allow_origin_header
    headers = {'accept' => 'application/json'}
    channel = 'test_custom_access_control_allow_origin_header'

    EventMachine.run {
      sub_1 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => headers, :timeout => 30
      sub_1.stream { |chunk|
        assert_equal("custom.domain.com", sub_1.response_header['ACCESS_CONTROL_ALLOW_ORIGIN'], "Didn't receive the right header")
        EventMachine.stop
      }

      add_test_timeout
    }
  end
end
