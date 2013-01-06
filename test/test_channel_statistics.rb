require File.expand_path('base_test_case', File.dirname(__FILE__))

class TestChannelStatistics < Test::Unit::TestCase
  include BaseTestCase

  def test_get_channel_statistics_whithout_created_channel
    headers = {'accept' => 'application/json'}
    channel = 'ch_test_get_channel_statistics_whithout_created_channel'

    EventMachine.run {
      pub_1 = EventMachine::HttpRequest.new(nginx_address + '/channels-stats?id=' + channel.to_s).get :head => headers, :timeout => 30
      pub_1.callback {
        assert_equal(404, pub_1.response_header.status, "Channel was founded")
        assert_equal(0, pub_1.response_header.content_length, "Recieved a non empty response")
        EventMachine.stop
      }
    }
  end

  def test_get_channel_statistics_to_existing_channel
    headers = {'accept' => 'application/json'}
    channel = 'ch_test_get_channel_statistics_to_existing_channel'
    body = 'body'

    #create channel
    publish_message(channel, headers, body)

    EventMachine.run {
      pub_2 = EventMachine::HttpRequest.new(nginx_address + '/channels-stats?id=' + channel.to_s).get :head => headers, :timeout => 30
      pub_2.callback {
        assert_equal(200, pub_2.response_header.status, "Request was not accepted")
        assert_not_equal(0, pub_2.response_header.content_length, "Empty response was received")
        response = JSON.parse(pub_2.response)
        assert_equal(channel, response["channel"].to_s, "Channel was not recognized")
        assert_equal(1, response["published_messages"].to_i, "Message was not published")
        assert_equal(1, response["stored_messages"].to_i, "Message was not stored")
        assert_equal(0, response["subscribers"].to_i, "Wrong number for subscribers")
        EventMachine.stop
      }
    }
  end

  def test_get_channel_statistics_to_existing_channel_with_subscriber
    headers = {'accept' => 'application/json'}
    channel = 'ch_test_get_channel_statistics_to_existing_channel_with_subscriber'
    body = 'body'

    create_channel_by_subscribe(channel, headers) do
      pub_1 = EventMachine::HttpRequest.new(nginx_address + '/channels-stats?id=' + channel.to_s).get :head => headers, :timeout => 30
      pub_1.callback {
        assert_equal(200, pub_1.response_header.status, "Request was not accepted")
        assert_not_equal(0, pub_1.response_header.content_length, "Empty response was received")
        response = JSON.parse(pub_1.response)
        assert_equal(channel, response["channel"].to_s, "Channel was not recognized")
        assert_equal(0, response["published_messages"].to_i, "Wrong number for published messages")
        assert_equal(0, response["stored_messages"].to_i, "Wrong number for stored messages")
        assert_equal(1, response["subscribers"].to_i, "Wrong number for subscribers")
        EventMachine.stop
      }
    end
  end

  def test_get_detailed_channels_statistics_whithout_created_channels
    headers = {'accept' => 'application/json'}

    EventMachine.run {
      pub_2 = EventMachine::HttpRequest.new(nginx_address + '/channels-stats?id=ALL').get :head => headers, :timeout => 30
      pub_2.callback {
        assert_equal(200, pub_2.response_header.status, "Request was not accepted")
        assert_not_equal(0, pub_2.response_header.content_length, "Empty response was received")
        response = JSON.parse(pub_2.response)
        assert_equal(0, response["infos"].length, "Received info whithout_created_channels")
        EventMachine.stop
      }
    }
  end

  def test_get_detailed_channels_statistics_to_existing_channel
    headers = {'accept' => 'application/json'}
    channel = 'ch_test_get_detailed_channels_statistics_to_existing_channel'
    body = 'body'

    #create channel
    publish_message(channel, headers, body)

    EventMachine.run {
      pub_2 = EventMachine::HttpRequest.new(nginx_address + '/channels-stats?id=ALL').get :head => headers, :timeout => 30
      pub_2.callback {
        assert_equal(200, pub_2.response_header.status, "Request was not accepted")
        assert_not_equal(0, pub_2.response_header.content_length, "Empty response was received")
        response = JSON.parse(pub_2.response)
        assert_equal(1, response["infos"].length, "Didn't received info about the only created channel")
        assert_equal(channel, response["infos"][0]["channel"].to_s, "Channel was not recognized")
        assert_equal(1, response["infos"][0]["published_messages"].to_i, "Message was not published")
        assert_equal(1, response["infos"][0]["stored_messages"].to_i, "Message was not stored")
        assert_equal(0, response["infos"][0]["subscribers"].to_i, "Wrong number for subscribers")
        EventMachine.stop
      }
    }
  end

  def config_test_get_detailed_channels_statistics_to_existing_broadcast_channel
    @broadcast_channel_prefix = 'bd_'
    @broadcast_channel_max_qtd = 1
  end

  def test_get_detailed_channels_statistics_to_existing_broadcast_channel
    headers = {'accept' => 'application/json'}
    channel = 'bd_test_get_detailed_channels_statistics_to_existing_broadcast_channel'
    body = 'body'

    #create channel
    publish_message(channel, headers, body)

    EventMachine.run {
      pub_2 = EventMachine::HttpRequest.new(nginx_address + '/channels-stats?id=ALL').get :head => headers, :timeout => 30
      pub_2.callback {
        assert_equal(200, pub_2.response_header.status, "Request was not accepted")
        assert_not_equal(0, pub_2.response_header.content_length, "Empty response was received")
        response = JSON.parse(pub_2.response)
        assert_equal(1, response["infos"].length, "Didn't received info about the only created channel")
        assert_equal(0, response["channels"].to_i, "Channel was not recognized")
        assert_equal(1, response["broadcast_channels"].to_i, "Channel was not recognized")
        assert_equal(channel, response["infos"][0]["channel"].to_s, "Channel was not recognized")
        assert_equal(1, response["infos"][0]["published_messages"].to_i, "Message was not published")
        assert_equal(1, response["infos"][0]["stored_messages"].to_i, "Message was not stored")
        assert_equal(0, response["infos"][0]["subscribers"].to_i, "Wrong number for subscribers")
        EventMachine.stop
      }
    }
  end

  def test_detailed_channels_statistics_to_existing_channel_with_subscriber
    headers = {'accept' => 'application/json'}
    channel = 'ch_test_detailed_channels_statistics_to_existing_channel_with_subscriber'
    body = 'body'

    create_channel_by_subscribe(channel, headers) do
      pub_1 = EventMachine::HttpRequest.new(nginx_address + '/channels-stats?id=ALL').get :head => headers, :timeout => 30
      pub_1.callback {
        assert_equal(200, pub_1.response_header.status, "Request was not accepted")
        assert_not_equal(0, pub_1.response_header.content_length, "Empty response was received")
        response = JSON.parse(pub_1.response)
        assert_equal(1, response["infos"].length, "Didn't received info about the only created channel")
        assert_equal(channel, response["infos"][0]["channel"].to_s, "Channel was not recognized")
        assert_equal(0, response["infos"][0]["published_messages"].to_i, "Wrong number for published messages")
        assert_equal(0, response["infos"][0]["stored_messages"].to_i, "Wrong number for stored messages")
        assert_equal(1, response["infos"][0]["subscribers"].to_i, "Wrong number for subscribers")
        EventMachine.stop
      }
    end
  end

  def test_get_summarized_channels_statistics_whithout_created_channels
    headers = {'accept' => 'application/json'}

    EventMachine.run {
      pub_1 = EventMachine::HttpRequest.new(nginx_address + '/channels-stats').get :head => headers, :timeout => 30
      pub_1.callback {
        assert_equal(200, pub_1.response_header.status, "Don't get channels statistics")
        assert_not_equal(0, pub_1.response_header.content_length, "Don't received channels statistics")
        begin
          response = JSON.parse(pub_1.response)
          assert(response.has_key?("channels"), "Didn't received the correct answer with channels info")
          assert_equal(0, response["channels"].to_i, "Returned values with channels created")
        rescue JSON::ParserError
          fail("Didn't receive a valid response")
        end
        EventMachine.stop
      }
    }
  end

  def test_get_summarized_channels_statistics_to_existing_channel
    headers = {'accept' => 'application/json'}
    channel = 'ch_test_get_summarized_channels_statistics_to_existing_channel'
    body = 'body'

    #create channel
    publish_message(channel, headers, body)

    EventMachine.run {
      pub_2 = EventMachine::HttpRequest.new(nginx_address + '/channels-stats').get :head => headers, :timeout => 30
      pub_2.callback {
        assert_equal(200, pub_2.response_header.status, "Don't get channels statistics")
        assert_not_equal(0, pub_2.response_header.content_length, "Don't received channels statistics")
        begin
          response = JSON.parse(pub_2.response)
          assert(response.has_key?("channels"), "Didn't received the correct answer with channels info")
          assert_equal(1, response["channels"].to_i, "Don't returned values with created channel")
          assert_equal(1, response["published_messages"].to_i, "Message was not published")
          assert_equal(0, response["subscribers"].to_i, "Wrong number for subscribers")
        rescue JSON::ParserError
          fail("Didn't receive a valid response")
        end
        EventMachine.stop
      }
    }
  end

  def config_test_get_summarized_channels_statistics_to_existing_broadcast_channel
    @broadcast_channel_prefix = 'bd_'
    @broadcast_channel_max_qtd = 1
  end

  def test_get_summarized_channels_statistics_to_existing_broadcast_channel
    headers = {'accept' => 'application/json'}
    channel = 'bd_test_get_summarized_channels_statistics_to_existing_broadcast_channel'
    body = 'body'

    #create channel
    publish_message(channel, headers, body)

    EventMachine.run {
      pub_2 = EventMachine::HttpRequest.new(nginx_address + '/channels-stats').get :head => headers, :timeout => 30
      pub_2.callback {
        assert_equal(200, pub_2.response_header.status, "Don't get channels statistics")
        assert_not_equal(0, pub_2.response_header.content_length, "Don't received channels statistics")
        begin
          response = JSON.parse(pub_2.response)
          assert(response.has_key?("channels"), "Didn't received the correct answer with channels info")
          assert_equal(0, response["channels"].to_i, "Don't returned values with created channel")
          assert_equal(1, response["broadcast_channels"].to_i, "Don't returned values with created channel")
          assert_equal(1, response["published_messages"].to_i, "Message was not published")
          assert_equal(0, response["subscribers"].to_i, "Wrong number for subscribers")
        rescue JSON::ParserError
          fail("Didn't receive a valid response")
        end
        EventMachine.stop
      }
    }
  end

  def test_summarized_channels_statistics_to_existing_channel_with_subscriber
    headers = {'accept' => 'application/json'}
    channel = 'ch_test_summarized_channels_statistics_to_existing_channel_with_subscriber'
    body = 'body'

    create_channel_by_subscribe(channel, headers) do
      pub_1 = EventMachine::HttpRequest.new(nginx_address + '/channels-stats').get :head => headers, :timeout => 30
      pub_1.callback {
        assert_equal(200, pub_1.response_header.status, "Request was not accepted")
        assert_not_equal(0, pub_1.response_header.content_length, "Empty response was received")
        response = JSON.parse(pub_1.response)
        assert(response.has_key?("channels"), "Didn't received the correct answer with channels info")
        assert_equal(1, response["channels"].to_i, "Don't returned values with created channel")
        assert_equal(0, response["published_messages"].to_i, "Wrong number for published messages")
        assert_equal(1, response["subscribers"].to_i, "Wrong number for subscribers")
        EventMachine.stop
      }
    end
  end

  def test_accepted_methods_channel_statistics
    EventMachine.run {
      multi = EventMachine::MultiRequest.new

      multi.add(:a, EventMachine::HttpRequest.new(nginx_address + '/channels-stats').get)
      multi.add(:b, EventMachine::HttpRequest.new(nginx_address + '/channels-stats').put(:body => 'body'))
      multi.add(:c, EventMachine::HttpRequest.new(nginx_address + '/channels-stats').post)
      multi.add(:d, EventMachine::HttpRequest.new(nginx_address + '/channels-stats').delete)
      multi.add(:e, EventMachine::HttpRequest.new(nginx_address + '/channels-stats').head)

      multi.callback  {
        assert_equal(5, multi.responses[:callback].length)

        assert_not_equal(405, multi.responses[:callback][:a].response_header.status, "Statistics does accept GET")
        assert_equal("GET", multi.responses[:callback][:a].req.method, "Array is with wrong order")

        assert_equal(405, multi.responses[:callback][:b].response_header.status, "Statistics does not accept PUT")
        assert_equal("PUT", multi.responses[:callback][:b].req.method, "Array is with wrong order")

        assert_equal(405, multi.responses[:callback][:c].response_header.status, "Statistics does not accept POST")
        assert_equal("POST", multi.responses[:callback][:c].req.method, "Array is with wrong order")

        assert_equal(405, multi.responses[:callback][:d].response_header.status, "Statistics does not accept DELETE")
        assert_equal("DELETE", multi.responses[:callback][:d].req.method, "Array is with wrong order")

        assert_equal(405, multi.responses[:callback][:e].response_header.status, "Statistics does not accept HEAD")
        assert_equal("HEAD", multi.responses[:callback][:e].req.method, "Array is with wrong order")

        EventMachine.stop
      }
    }
  end

  def test_accepted_content_types
    headers = {'accept' => 'application/json'}
    channel = 'ch_test_accepted_content_types'
    body = 'body'

    #create channel
    publish_message(channel, headers, body)

    EventMachine.run {

      multi = EventMachine::MultiRequest.new

      multi.add(:a, EventMachine::HttpRequest.new(nginx_address + '/channels-stats').get) # default content_type
      multi.add(:b, EventMachine::HttpRequest.new(nginx_address + '/channels-stats').get(:head => {'accept' => 'text/plain'}))
      multi.add(:c, EventMachine::HttpRequest.new(nginx_address + '/channels-stats').get(:head => {'accept' => 'application/json'}))
      multi.add(:d, EventMachine::HttpRequest.new(nginx_address + '/channels-stats').get(:head => {'accept' => 'application/yaml'}))
      multi.add(:e, EventMachine::HttpRequest.new(nginx_address + '/channels-stats').get(:head => {'accept' => 'application/xml'}))
      multi.add(:f, EventMachine::HttpRequest.new(nginx_address + '/channels-stats').get(:head => {'accept' => 'text/x-json'}))
      multi.add(:g, EventMachine::HttpRequest.new(nginx_address + '/channels-stats').get(:head => {'accept' => 'text/x-yaml'}))

      multi.callback  {
        assert_equal(7, multi.responses[:callback].length)

        assert_equal(200, multi.responses[:callback][:a].response_header.status, "Statistics does accept GET")
        assert_equal("GET", multi.responses[:callback][:a].req.method, "Array is with wrong order")
        assert_equal("application/json", multi.responses[:callback][:a].response_header["CONTENT_TYPE"], "wrong content-type")

        assert_equal(200, multi.responses[:callback][:b].response_header.status, "Statistics does accept GET")
        assert_equal("GET", multi.responses[:callback][:b].req.method, "Array is with wrong order")
        assert_equal("text/plain", multi.responses[:callback][:b].response_header["CONTENT_TYPE"], "wrong content-type")

        assert_equal(200, multi.responses[:callback][:c].response_header.status, "Statistics does accept GET")
        assert_equal("GET", multi.responses[:callback][:c].req.method, "Array is with wrong order")
        assert_equal("application/json", multi.responses[:callback][:c].response_header["CONTENT_TYPE"], "wrong content-type")

        assert_equal(200, multi.responses[:callback][:d].response_header.status, "Statistics does accept GET")
        assert_equal("GET", multi.responses[:callback][:d].req.method, "Array is with wrong order")
        assert_equal("application/yaml", multi.responses[:callback][:d].response_header["CONTENT_TYPE"], "wrong content-type")

        assert_equal(200, multi.responses[:callback][:e].response_header.status, "Statistics does accept GET")
        assert_equal("GET", multi.responses[:callback][:e].req.method, "Array is with wrong order")
        assert_equal("application/xml", multi.responses[:callback][:e].response_header["CONTENT_TYPE"], "wrong content-type")

        assert_equal(200, multi.responses[:callback][:f].response_header.status, "Statistics does accept GET")
        assert_equal("GET", multi.responses[:callback][:f].req.method, "Array is with wrong order")
        assert_equal("text/x-json", multi.responses[:callback][:f].response_header["CONTENT_TYPE"], "wrong content-type")

        assert_equal(200, multi.responses[:callback][:g].response_header.status, "Statistics does accept GET")
        assert_equal("GET", multi.responses[:callback][:g].req.method, "Array is with wrong order")
        assert_equal("text/x-yaml", multi.responses[:callback][:g].response_header["CONTENT_TYPE"], "wrong content-type")

        EventMachine.stop
      }
    }
  end

  def config_test_get_detailed_channels_statistics_to_many_channels
    @max_reserved_memory = '200m'
    @keepalive = "on"
  end

  def test_get_detailed_channels_statistics_to_many_channels
    headers = {'accept' => 'application/json'}
    channel = 'ch_test_get_detailed_channels_statistics_to_many_channels_'
    body = 'body'
    number_of_channels = 20000

    #create channels
    0.step(number_of_channels - 1, 10) do |i|
      socket = open_socket
      1.upto(10) do |j|
        channel_name = "#{channel}#{i + j}"
        headers, body = publish_message_in_socket(channel_name, body, socket)
        fail("Don't create the channel") unless headers.include?("HTTP/1.1 200 OK")
      end
      socket.close
    end

    EventMachine.run {
      pub_2 = EventMachine::HttpRequest.new(nginx_address + '/channels-stats?id=ALL').get :head => headers, :timeout => 30
      pub_2.callback {
        assert_equal(200, pub_2.response_header.status, "Request was not accepted")
        assert_not_equal(0, pub_2.response_header.content_length, "Empty response was received")
        response = JSON.parse(pub_2.response)
        assert_equal(number_of_channels, response["infos"].length, "Didn't received info about the created channels")
        EventMachine.stop
      }
    }
  end

  def test_get_detailed_channels_statistics_whithout_created_channels_using_prefix
    headers = {'accept' => 'application/json'}

    EventMachine.run {
      pub_2 = EventMachine::HttpRequest.new(nginx_address + '/channels-stats?id=prefix_*').get :head => headers, :timeout => 30
      pub_2.callback {
        assert_equal(200, pub_2.response_header.status, "Request was not accepted")
        assert_not_equal(0, pub_2.response_header.content_length, "Empty response was received")
        response = JSON.parse(pub_2.response)
        assert_equal(0, response["infos"].length, "Received info whithout_created_channels")
        EventMachine.stop
      }
    }
  end

  def test_get_detailed_channels_statistics_to_existing_channel_using_prefix
    headers = {'accept' => 'application/json'}
    channel = 'ch_test_get_detailed_channels_statistics_to_existing_channel_using_prefix'
    channel_1 = 'another_ch_test_get_detailed_channels_statistics_to_existing_channel_using_prefix'
    body = 'body'

    #create channels
    publish_message(channel, headers, body)
    publish_message(channel_1, headers, body)

    EventMachine.run {
      pub_2 = EventMachine::HttpRequest.new(nginx_address + '/channels-stats?id=ch_test_*').get :head => headers, :timeout => 30
      pub_2.callback {
        assert_equal(200, pub_2.response_header.status, "Request was not accepted")
        assert_not_equal(0, pub_2.response_header.content_length, "Empty response was received")
        response = JSON.parse(pub_2.response)
        assert_equal(1, response["infos"].length, "Didn't received info about the only created channel")
        assert_equal(channel, response["infos"][0]["channel"].to_s, "Channel was not recognized")
        assert_equal(1, response["infos"][0]["published_messages"].to_i, "Message was not published")
        assert_equal(1, response["infos"][0]["stored_messages"].to_i, "Message was not stored")
        assert_equal(0, response["infos"][0]["subscribers"].to_i, "Wrong number for subscribers")
        EventMachine.stop
      }
    }
  end

  def test_get_detailed_channels_statistics_using_prefix_as_same_behavior_ALL
    headers = {'accept' => 'application/json'}
    channel = 'ch_test_get_detailed_channels_statistics_using_prefix_as_same_behavior_ALL'
    channel_1 = 'another_ch_test_get_detailed_channels_statistics_using_prefix_as_same_behavior_ALL'
    body = 'body'

    #create channels
    publish_message(channel, headers, body)
    publish_message(channel_1, headers, body)

    EventMachine.run {
      pub_2 = EventMachine::HttpRequest.new(nginx_address + '/channels-stats?id=*').get :head => headers, :timeout => 30
      pub_2.callback {
        assert_equal(200, pub_2.response_header.status, "Request was not accepted")
        assert_not_equal(0, pub_2.response_header.content_length, "Empty response was received")
        response = JSON.parse(pub_2.response)
        assert_equal(2, response["infos"].length, "Didn't received info about the only created channel")
        assert_equal(channel, response["infos"][0]["channel"].to_s, "Channel was not recognized")
        assert_equal(1, response["infos"][0]["published_messages"].to_i, "Message was not published")
        assert_equal(1, response["infos"][0]["stored_messages"].to_i, "Message was not stored")
        assert_equal(0, response["infos"][0]["subscribers"].to_i, "Wrong number for subscribers")
        assert_equal(channel_1, response["infos"][1]["channel"].to_s, "Channel was not recognized")
        assert_equal(1, response["infos"][1]["published_messages"].to_i, "Message was not published")
        assert_equal(1, response["infos"][1]["stored_messages"].to_i, "Message was not stored")
        assert_equal(0, response["infos"][1]["subscribers"].to_i, "Wrong number for subscribers")
        EventMachine.stop
      }
    }
  end

  def config_test_get_detailed_channels_statistics_to_existing_broadcast_channel_using_prefix
    @broadcast_channel_prefix = 'bd_'
    @broadcast_channel_max_qtd = 1
  end

  def test_get_detailed_channels_statistics_to_existing_broadcast_channel_using_prefix
    headers = {'accept' => 'application/json'}
    channel = 'bd_test_get_detailed_channels_statistics_to_existing_broadcast_channel_using_prefix'
    body = 'body'

    #create channel
    publish_message(channel, headers, body)

    EventMachine.run {
      pub_2 = EventMachine::HttpRequest.new(nginx_address + '/channels-stats?id=bd_test_*').get :head => headers, :timeout => 30
      pub_2.callback {
        assert_equal(200, pub_2.response_header.status, "Request was not accepted")
        assert_not_equal(0, pub_2.response_header.content_length, "Empty response was received")
        response = JSON.parse(pub_2.response)
        assert_equal(1, response["infos"].length, "Didn't received info about the only created channel")
        assert_equal(0, response["channels"].to_i, "Channel was not recognized")
        assert_equal(1, response["broadcast_channels"].to_i, "Channel was not recognized")
        assert_equal(channel, response["infos"][0]["channel"].to_s, "Channel was not recognized")
        assert_equal(1, response["infos"][0]["published_messages"].to_i, "Message was not published")
        assert_equal(1, response["infos"][0]["stored_messages"].to_i, "Message was not stored")
        assert_equal(0, response["infos"][0]["subscribers"].to_i, "Wrong number for subscribers")
        EventMachine.stop
      }
    }
  end

  def test_detailed_channels_statistics_to_existing_channel_with_subscriber_using_prefix
    headers = {'accept' => 'application/json'}
    channel = 'ch_test_detailed_channels_statistics_to_existing_channel_with_subscriber_using_prefix'
    body = 'body'

    create_channel_by_subscribe(channel, headers) do
      pub_1 = EventMachine::HttpRequest.new(nginx_address + '/channels-stats?id=ch_test_*').get :head => headers, :timeout => 30
      pub_1.callback {
        assert_equal(200, pub_1.response_header.status, "Request was not accepted")
        assert_not_equal(0, pub_1.response_header.content_length, "Empty response was received")
        response = JSON.parse(pub_1.response)
        assert_equal(1, response["infos"].length, "Didn't received info about the only created channel")
        assert_equal(channel, response["infos"][0]["channel"].to_s, "Channel was not recognized")
        assert_equal(0, response["infos"][0]["published_messages"].to_i, "Wrong number for published messages")
        assert_equal(0, response["infos"][0]["stored_messages"].to_i, "Wrong number for stored messages")
        assert_equal(1, response["infos"][0]["subscribers"].to_i, "Wrong number for subscribers")
        EventMachine.stop
      }
    end
  end

  def config_test_get_detailed_channels_statistics_to_many_channels_using_prefix
    @max_reserved_memory = '200m'
    @keepalive = "on"
  end

  def test_get_detailed_channels_statistics_to_many_channels_using_prefix
    headers = {'accept' => 'application/json'}
    channel = 'ch_test_get_detailed_channels_statistics_to_many_channels_using_prefix_'
    body = 'body'
    number_of_channels = 20000

    #create channels
    0.step(number_of_channels - 1, 10) do |i|
      socket = open_socket
      1.upto(10) do |j|
        channel_name = "#{channel}#{i + j}"
        headers, body = publish_message_in_socket(channel_name, body, socket)
        fail("Don't create the channel") unless headers.include?("HTTP/1.1 200 OK")
      end
      socket.close
    end

    EventMachine.run {
      pub_2 = EventMachine::HttpRequest.new(nginx_address + '/channels-stats?id=ch_test_get_detailed_channels_statistics_to_many_channels_using_prefix_10*').get :head => headers, :timeout => 30
      pub_2.callback {
        assert_equal(200, pub_2.response_header.status, "Request was not accepted")
        assert_not_equal(0, pub_2.response_header.content_length, "Empty response was received")
        response = JSON.parse(pub_2.response)
        assert_equal(1111, response["infos"].length, "Didn't received info about the created channels")
        EventMachine.stop
      }
    }
  end

  def test_get_uptime_in_detailed_channels_statistics
    headers = {'accept' => 'application/json'}
    channel = 'ch_test_get_uptime_in_detailed_channels_statistics'
    body = 'body'

    #create channel
    publish_message(channel, headers, body)

    EventMachine.run {
      pub_2 = EventMachine::HttpRequest.new(nginx_address + '/channels-stats?id=ALL').get :head => headers, :timeout => 30
      pub_2.callback {
        assert_equal(200, pub_2.response_header.status, "Request was not accepted")
        assert_not_equal(0, pub_2.response_header.content_length, "Empty response was received")
        response = JSON.parse(pub_2.response)
        assert(response.has_key?("hostname") && !response["hostname"].empty?, "Hasn't a key hostname")
        assert(response.has_key?("time") && !response["time"].empty?, "Hasn't a key time")
        assert(response.has_key?("channels") && !response["channels"].empty?, "Hasn't a key channels")
        assert(response.has_key?("broadcast_channels") && !response["broadcast_channels"].empty?, "Hasn't a key broadcast_channels")
        assert(response.has_key?("uptime") && !response["uptime"].empty?, "Hasn't a key uptime")
        assert(response.has_key?("infos") && !response["infos"].empty?, "Hasn't a key infos")

        sleep(2)
        pub_3 = EventMachine::HttpRequest.new(nginx_address + '/channels-stats?id=ALL').get :head => headers, :timeout => 30
        pub_3.callback {
          assert_equal(200, pub_3.response_header.status, "Request was not accepted")
          assert_not_equal(0, pub_3.response_header.content_length, "Empty response was received")
          response = JSON.parse(pub_3.response)
          assert(response["uptime"].to_i >= 2, "Don't get server uptime")
          EventMachine.stop
        }
      }
    }
  end

  def test_get_uptime_in_summarized_channels_statistics
    headers = {'accept' => 'application/json'}
    channel = 'ch_test_get_uptime_in_summarized_channels_statistics'
    body = 'body'

    #create channel
    publish_message(channel, headers, body)

    EventMachine.run {
      pub_2 = EventMachine::HttpRequest.new(nginx_address + '/channels-stats').get :head => headers, :timeout => 30
      pub_2.callback {
        assert_equal(200, pub_2.response_header.status, "Request was not accepted")
        assert_not_equal(0, pub_2.response_header.content_length, "Empty response was received")
        response = JSON.parse(pub_2.response)
        assert(response.has_key?("hostname") && !response["hostname"].empty?, "Hasn't a key hostname")
        assert(response.has_key?("time") && !response["time"].empty?, "Hasn't a key time")
        assert(response.has_key?("channels") && !response["channels"].empty?, "Hasn't a key channels")
        assert(response.has_key?("broadcast_channels") && !response["broadcast_channels"].empty?, "Hasn't a key broadcast_channels")
        assert(response.has_key?("published_messages") && !response["published_messages"].empty?, "Hasn't a key published_messages")
        assert(response.has_key?("subscribers") && !response["subscribers"].empty?, "Hasn't a key subscribers")
        assert(response.has_key?("uptime") && !response["uptime"].empty?, "Hasn't a key uptime")
        assert(response.has_key?("by_worker") && !response["by_worker"].empty?, "Hasn't a key by_worker")
        assert(response["by_worker"][0].has_key?("pid") && !response["by_worker"][0]["pid"].empty?, "Hasn't a key pid on worker info")
        assert(response["by_worker"][0].has_key?("subscribers") && !response["by_worker"][0]["subscribers"].empty?, "Hasn't a key subscribers on worker info")
        assert(response["by_worker"][0].has_key?("uptime") && !response["by_worker"][0]["uptime"].empty?, "Hasn't a key uptime on worker info")


        sleep(2)
        pub_3 = EventMachine::HttpRequest.new(nginx_address + '/channels-stats').get :head => headers, :timeout => 30
        pub_3.callback {
          assert_equal(200, pub_3.response_header.status, "Request was not accepted")
          assert_not_equal(0, pub_3.response_header.content_length, "Empty response was received")
          response = JSON.parse(pub_3.response)
          assert(response["uptime"].to_i >= 2, "Don't get server uptime")
          assert(response["by_worker"][0]["uptime"].to_i >= 2, "Don't get worker uptime")
          EventMachine.stop
        }
      }
    }
  end
end
