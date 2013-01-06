require File.expand_path('base_test_case', File.dirname(__FILE__))

class TestPublisher < Test::Unit::TestCase
  include BaseTestCase

  def test_access_whithout_channel_id
    headers = {'accept' => 'application/json'}

    EventMachine.run {
      pub = EventMachine::HttpRequest.new(nginx_address + '/pub?id=').get :head => headers, :timeout => 30
      pub.callback {
        assert_equal(0, pub.response_header.content_length, "Should response only with headers")
        assert_equal(400, pub.response_header.status, "Request was not understood as a bad request")
        assert_equal("No channel id provided.", pub.response_header['X_NGINX_PUSHSTREAM_EXPLAIN'], "Didn't receive the right error message")
        EventMachine.stop
      }
    }
  end

  def test_access_whith_channel_id_to_absent_channel
    headers = {'accept' => 'application/json'}
    channel_1 = 'ch_test_access_whith_channel_id_to_absent_channel_1'
    channel_2 = 'ch_test_access_whith_channel_id_to_absent_channel_2'
    body = 'body'

    EventMachine.run {
      pub_1 = EventMachine::HttpRequest.new(nginx_address + '/pub?id=' + channel_1.to_s).get :head => headers, :timeout => 30
      pub_1.callback {
        assert_equal(404, pub_1.response_header.status, "Channel was founded")
        assert_equal(0, pub_1.response_header.content_length, "Recieved a non empty response")
        EventMachine.stop
      }
    }

    EventMachine.run {
      pub_2 = EventMachine::HttpRequest.new(nginx_address + '/pub?id=' + channel_2.to_s ).post :head => headers, :body => body, :timeout => 30
      pub_2.callback {
        assert_equal(200, pub_2.response_header.status, "Request was not accepted")
        assert_not_equal(0, pub_2.response_header.content_length, "Empty response was received")
        response = JSON.parse(pub_2.response)
        assert_equal(channel_2, response["channel"].to_s, "Channel was not recognized")
        EventMachine.stop
      }
    }
  end

  def test_access_whith_channel_id_to_existing_channel
    headers = {'accept' => 'application/json'}
    channel = 'ch_test_access_whith_channel_id_to_existing_channel'
    body = 'body'

    #create channel
    EventMachine.run {
      pub_1 = EventMachine::HttpRequest.new(nginx_address + '/pub?id=' + channel.to_s).post :head => headers, :body => body, :timeout => 30
      pub_1.callback {
        assert_equal(200, pub_1.response_header.status, "Request was not accepted")
        assert_not_equal(0, pub_1.response_header.content_length, "Empty response was received")
        response = JSON.parse(pub_1.response)
        assert_equal(channel, response["channel"].to_s, "Channel was not recognized")
        EventMachine.stop
      }
    }

    EventMachine.run {
      pub_2 = EventMachine::HttpRequest.new(nginx_address + '/pub?id=' + channel.to_s).get :head => headers, :timeout => 30
      pub_2.callback {
        assert_equal(200, pub_2.response_header.status, "Request was not accepted")
        assert_not_equal(0, pub_2.response_header.content_length, "Empty response was received")
        response = JSON.parse(pub_2.response)
        assert_equal(channel, response["channel"].to_s, "Channel was not recognized")
        EventMachine.stop
      }
    }
  end

  def test_accepted_methods
    EventMachine.run {
      multi = EventMachine::MultiRequest.new

      multi.add(:a, EventMachine::HttpRequest.new(nginx_address + '/pub?id=ch_test_accepted_methods_1').get)
      multi.add(:b, EventMachine::HttpRequest.new(nginx_address + '/pub?id=ch_test_accepted_methods_2').put(:body => 'body'))
      multi.add(:c, EventMachine::HttpRequest.new(nginx_address + '/pub?id=ch_test_accepted_methods_3').post)
      multi.add(:d, EventMachine::HttpRequest.new(nginx_address + '/pub?id=ch_test_accepted_methods_4').delete)
      multi.add(:e, EventMachine::HttpRequest.new(nginx_address + '/pub?id=ch_test_accepted_methods_5').head)

      multi.callback  {
        assert_equal(5, multi.responses[:callback].length)

        assert_not_equal(405, multi.responses[:callback][:a].response_header.status, "Publisher does accept GET")
        assert_equal("GET", multi.responses[:callback][:a].req.method, "Array is with wrong order")

        assert_equal(405, multi.responses[:callback][:b].response_header.status, "Publisher does not accept PUT")
        assert_equal("GET, POST", multi.responses[:callback][:b].response_header['ALLOW'], "Didn't receive the right error message")
        assert_equal("PUT", multi.responses[:callback][:b].req.method, "Array is with wrong order")

        assert_not_equal(405, multi.responses[:callback][:c].response_header.status, "Publisher does accept POST")
        assert_equal("POST", multi.responses[:callback][:c].req.method, "Array is with wrong order")

        assert_equal(405, multi.responses[:callback][:d].response_header.status, "Publisher does not accept DELETE")
        assert_equal("DELETE", multi.responses[:callback][:d].req.method, "Array is with wrong order")
        assert_equal("GET, POST", multi.responses[:callback][:d].response_header['ALLOW'], "Didn't receive the right error message")

        assert_equal(405, multi.responses[:callback][:e].response_header.status, "Publisher does not accept HEAD")
        assert_equal("HEAD", multi.responses[:callback][:e].req.method, "Array is with wrong order")
        assert_equal("GET, POST", multi.responses[:callback][:e].response_header['ALLOW'], "Didn't receive the right error message")

        EventMachine.stop
      }
    }
  end

  def test_cannot_create_a_channel_with_id_ALL
    headers = {'accept' => 'application/json'}
    channel = 'ALL'
    body = 'body'

    EventMachine.run {
      pub_1 = EventMachine::HttpRequest.new(nginx_address + '/pub?id=' + channel.to_s).post :head => headers, :body => body, :timeout => 30
      pub_1.callback {
        assert_equal(403, pub_1.response_header.status, "Channel was created")
        assert_equal(0, pub_1.response_header.content_length, "Received response for creating channel with id ALL")
        assert_equal("Channel id not authorized for this method.", pub_1.response_header['X_NGINX_PUSHSTREAM_EXPLAIN'], "Didn't receive the right error message")
        EventMachine.stop
      }
    }
  end

  def test_cannot_create_a_channel_with_id_containing_wildcard
    headers = {'accept' => 'application/json'}
    body = 'body'
    channel_1 = 'abcd*efgh'
    channel_2 = '*abcdefgh'
    channel_3 = 'abcdefgh*'

    EventMachine.run {
      multi = EventMachine::MultiRequest.new

      multi.add(:a, EventMachine::HttpRequest.new(nginx_address + '/pub?id=' + channel_1).post(:head => headers, :body => body, :timeout => 30))
      multi.add(:b, EventMachine::HttpRequest.new(nginx_address + '/pub?id=' + channel_2).post(:head => headers, :body => body, :timeout => 30))
      multi.add(:c, EventMachine::HttpRequest.new(nginx_address + '/pub?id=' + channel_3).post(:head => headers, :body => body, :timeout => 30))
      multi.callback  {
        assert_equal(3, multi.responses[:callback].length)
        multi.responses[:callback].each do |name, response|
          assert_equal(403, response.response_header.status, "Channel was created")
          assert_equal(0, response.response_header.content_length, "Received response for creating channel with id containing wildcard")
          assert_equal("Channel id not authorized for this method.", response.response_header['X_NGINX_PUSHSTREAM_EXPLAIN'], "Didn't receive the right error message")
        end

        EventMachine.stop
      }
    }
  end

  def config_test_post_message_larger_than_max_body_size_should_be_rejected
    @client_max_body_size = '2k'
    @client_body_buffer_size = '1k'
  end


  def test_post_message_larger_than_max_body_size_should_be_rejected
    headers = {'accept' => 'application/json'}
    channel = 'ch_test_post_message_larger_than_max_body_size_should_be_rejected'
    body = '^'
    (1..40).each do |n|
      body += '0123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789|'
    end
    body += '$'

    EventMachine.run {
      pub_1 = EventMachine::HttpRequest.new(nginx_address + '/pub?id=' + channel.to_s).post :head => headers, :body => body, :timeout => 30
      pub_1.callback {
        assert_equal(413, pub_1.response_header.status, "Request was accepted")
        EventMachine.stop
      }
    }
  end

  def config_test_post_message_larger_than_body_buffer_size_should_be_accepted
    @client_max_body_size = '10k'
    @client_body_buffer_size = '1k'
  end


  def test_post_message_larger_than_body_buffer_size_should_be_accepted
    headers = {'accept' => 'application/json'}
    channel = 'ch_test_post_message_larger_than_body_buffer_size_should_be_accepted'
    body = '^'
    (1..80).each do |n|
      body += '0123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789|'
    end
    body += '$'

    EventMachine.run {
      pub_1 = EventMachine::HttpRequest.new(nginx_address + '/pub?id=' + channel.to_s).post :head => headers, :body => body, :timeout => 30
      pub_1.callback {
        assert_equal(200, pub_1.response_header.status, "Request was not accepted")
        fail("Let a file on client body temp dir") unless Dir.entries(@client_body_temp).select {|f| f if File.file?(File.expand_path(f, @client_body_temp)) }.empty?
        EventMachine.stop
      }
    }
  end

  def config_test_post_message_shorter_than_body_buffer_size_should_be_accepted
    @client_max_body_size = '10k'
    @client_body_buffer_size = '6k'
  end


  def test_post_message_shorter_than_body_buffer_size_should_be_accepted
    headers = {'accept' => 'application/json'}
    channel = 'ch_test_post_message_shorter_than_body_buffer_size_should_be_accepted'
    body = '^'
    (1..40).each do |n|
      body += '0123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789|'
    end
    body += '$'

    EventMachine.run {
      pub_1 = EventMachine::HttpRequest.new(nginx_address + '/pub?id=' + channel.to_s).post :head => headers, :body => body, :timeout => 30
      pub_1.callback {
        assert_equal(200, pub_1.response_header.status, "Request was not accepted")
        fail("Let a file on client body temp dir") unless Dir.entries(@client_body_temp).select {|f| f if File.file?(File.expand_path(f, @client_body_temp)) }.empty?
        EventMachine.stop
      }
    }
  end

  def config_test_stored_messages
    @store_messages = "on"
  end

  def test_stored_messages
    headers = {'accept' => 'application/json'}
    body = 'published message'
    channel = 'ch_test_stored_messages'

    EventMachine.run {
      pub_1 = EventMachine::HttpRequest.new(nginx_address + '/pub?id=' + channel.to_s ).post :head => headers, :body => body, :timeout => 30
      pub_1.callback {
        response = JSON.parse(pub_1.response)
        assert_equal(1, response["stored_messages"].to_i, "Not stored messages")

        pub_2 = EventMachine::HttpRequest.new(nginx_address + '/pub?id=' + channel.to_s ).post :head => headers, :body => body, :timeout => 30
        pub_2.callback {
          response = JSON.parse(pub_2.response)
          assert_equal(2, response["stored_messages"].to_i, "Not stored messages")
          EventMachine.stop
        }
      }
    }
  end

  def config_test_not_stored_messages
    @store_messages = "off"
  end

  def test_not_stored_messages
    headers = {'accept' => 'application/json'}
    body = 'published message'
    channel = 'ch_test_not_stored_messages'

    EventMachine.run {
      pub = EventMachine::HttpRequest.new(nginx_address + '/pub?id=' + channel.to_s ).post :head => headers, :body => body, :timeout => 30
      pub.callback {
        response = JSON.parse(pub.response)
        assert_equal(0, response["stored_messages"].to_i, "Stored messages")
        EventMachine.stop
      }
    }
  end

  def config_test_max_stored_messages
    @store_messages = "on"
    @max_message_buffer_length = 4
  end

  def test_max_stored_messages
    headers = {'accept' => 'application/json'}
    body_prefix = 'published message '
    channel = 'ch_test_max_stored_messages'
    messagens_to_publish = 10

    EventMachine.run {

      i = 0
      stored_messages = 0
      EM.add_periodic_timer(0.001) do
        i += 1
        if i <= messagens_to_publish
          pub = EventMachine::HttpRequest.new(nginx_address + '/pub?id=' + channel.to_s ).post :head => headers, :body => body_prefix + i.to_s, :timeout => 30
          pub.callback {
            response = JSON.parse(pub.response)
            stored_messages = response["stored_messages"].to_i
          }
        else
          EventMachine.stop
          assert(stored_messages == @max_message_buffer_length, "Stored more messages then configured")
        end
      end
    }
  end

  def config_test_max_channel_id_length
    @max_channel_id_length = 5
  end

  def test_max_channel_id_length
    headers = {'accept' => 'application/json'}
    body = 'published message'
    channel = '123456'

    EventMachine.run {
      pub = EventMachine::HttpRequest.new(nginx_address + '/pub?id=' + channel.to_s ).post :head => headers, :body => body, :timeout => 30
      pub.callback {
        assert_equal(0, pub.response_header.content_length, "Should response only with headers")
        assert_equal(400, pub.response_header.status, "Request was not understood as a bad request")
        assert_equal("Channel id is too large.", pub.response_header['X_NGINX_PUSHSTREAM_EXPLAIN'], "Didn't receive the right error message")
        EventMachine.stop
      }
    }
  end

  def config_test_max_number_of_channels
    @max_number_of_channels = 1
  end

  def test_max_number_of_channels
    headers = {'accept' => 'application/json'}
    body = 'published message'
    channel = 'ch_test_max_number_of_channels_'

    EventMachine.run {
      pub = EventMachine::HttpRequest.new(nginx_address + '/pub?id=' + channel.to_s + 1.to_s).post :head => headers, :body => body, :timeout => 30
      pub.callback {
        assert_equal(200, pub.response_header.status, "Channel was not created")
        assert_not_equal(0, pub.response_header.content_length, "Should response channel info")
        EventMachine.stop
      }
    }

    EventMachine.run {
      pub = EventMachine::HttpRequest.new(nginx_address + '/pub?id=' + channel.to_s + 2.to_s).post :head => headers, :body => body, :timeout => 30
      pub.callback {
        assert_equal(403, pub.response_header.status, "Request was not forbidden")
        assert_equal(0, pub.response_header.content_length, "Should response only with headers")
        assert_equal("Number of channels were exceeded.", pub.response_header['X_NGINX_PUSHSTREAM_EXPLAIN'], "Didn't receive the right error message")
        EventMachine.stop
      }
    }
  end

  def config_test_max_number_of_broadcast_channels
    @max_number_of_broadcast_channels = 1
    @broadcast_channel_prefix = 'bd_'
    @broadcast_channel_max_qtd = 1
  end

  def test_max_number_of_broadcast_channels
    headers = {'accept' => 'application/json'}
    body = 'published message'
    channel = 'bd_test_max_number_of_broadcast_channels_'

    EventMachine.run {
      pub = EventMachine::HttpRequest.new(nginx_address + '/pub?id=' + channel.to_s + 1.to_s).post :head => headers, :body => body, :timeout => 30
      pub.callback {
        assert_equal(200, pub.response_header.status, "Channel was not created")
        assert_not_equal(0, pub.response_header.content_length, "Should response channel info")
        EventMachine.stop
      }
    }

    EventMachine.run {
      pub = EventMachine::HttpRequest.new(nginx_address + '/pub?id=' + channel.to_s + 2.to_s).post :head => headers, :body => body, :timeout => 30
      pub.callback {
        assert_equal(403, pub.response_header.status, "Request was not forbidden")
        assert_equal(0, pub.response_header.content_length, "Should response only with headers")
        assert_equal("Number of channels were exceeded.", pub.response_header['X_NGINX_PUSHSTREAM_EXPLAIN'], "Didn't receive the right error message")
        EventMachine.stop
      }
    }
  end

  def test_default_access_control_allow_origin_header
    headers = {'accept' => 'application/json'}
    channel = 'test_default_access_control_allow_origin_header'

    EventMachine.run {
      pub = EventMachine::HttpRequest.new(nginx_address + '/pub?id=' + channel).get :head => headers, :timeout => 30
      pub.callback {
        assert_equal("*", pub.response_header['ACCESS_CONTROL_ALLOW_ORIGIN'], "Didn't receive the right header")
        EventMachine.stop
      }
    }
  end

  def config_test_custom_access_control_allow_origin_header
    @allowed_origins = "custom.domain.com"
  end

  def test_custom_access_control_allow_origin_header
    headers = {'accept' => 'application/json'}
    channel = 'test_custom_access_control_allow_origin_header'

    EventMachine.run {
      pub = EventMachine::HttpRequest.new(nginx_address + '/pub?id=' + channel).get :head => headers, :timeout => 30
      pub.callback {
        assert_equal("custom.domain.com", pub.response_header['ACCESS_CONTROL_ALLOW_ORIGIN'], "Didn't receive the right header")
        EventMachine.stop
      }
    }
  end
end
