require File.expand_path('base_test_case', File.dirname(__FILE__))

class TestPublisherAdmin < Test::Unit::TestCase
  include BaseTestCase

  def global_configuration
    @publisher_mode = 'admin'
  end

  def test_admin_access_whithout_channel_id
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

  def test_admin_access_whith_channel_id_to_absent_channel
    headers = {'accept' => 'application/json'}
    channel_1 = 'ch_test_admin_access_whith_channel_id_to_absent_channel_1'
    channel_2 = 'ch_test_admin_access_whith_channel_id_to_absent_channel_2'
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

  def test_admin_access_whith_channel_id_to_existing_channel
    headers = {'accept' => 'application/json'}
    channel = 'ch_test_admin_access_whith_channel_id_to_existing_channel'
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

  def test_admin_accepted_methods
    EventMachine.run {
      multi = EventMachine::MultiRequest.new

      multi.add(:a, EventMachine::HttpRequest.new(nginx_address + '/pub?id=ch_test_admin_accepted_methods_1').get)
      multi.add(:b, EventMachine::HttpRequest.new(nginx_address + '/pub?id=ch_test_admin_accepted_methods_2').put(:body => 'body'))
      multi.add(:c, EventMachine::HttpRequest.new(nginx_address + '/pub?id=ch_test_admin_accepted_methods_3').post)
      multi.add(:d, EventMachine::HttpRequest.new(nginx_address + '/pub?id=ch_test_admin_accepted_methods_4').delete)
      multi.add(:e, EventMachine::HttpRequest.new(nginx_address + '/pub?id=ch_test_admin_accepted_methods_5').head)

      multi.callback  {
        assert_equal(5, multi.responses[:callback].length)

        assert_not_equal(405, multi.responses[:callback][:a].response_header.status, "Publisher does accept GET")
        assert_equal("GET", multi.responses[:callback][:a].req.method, "Array is with wrong order")

        assert_equal(405, multi.responses[:callback][:b].response_header.status, "Publisher does not accept PUT")
        assert_equal("GET, POST, DELETE", multi.responses[:callback][:b].response_header['ALLOW'], "Didn't receive the right error message")
        assert_equal("PUT", multi.responses[:callback][:b].req.method, "Array is with wrong order")

        assert_not_equal(405, multi.responses[:callback][:c].response_header.status, "Publisher does accept POST")
        assert_equal("POST", multi.responses[:callback][:c].req.method, "Array is with wrong order")

        assert_not_equal(405, multi.responses[:callback][:d].response_header.status, "Publisher does accept DELETE")
        assert_equal("DELETE", multi.responses[:callback][:d].req.method, "Array is with wrong order")

        assert_equal(405, multi.responses[:callback][:e].response_header.status, "Publisher does not accept HEAD")
        assert_equal("HEAD", multi.responses[:callback][:e].req.method, "Array is with wrong order")
        assert_equal("GET, POST, DELETE", multi.responses[:callback][:e].response_header['ALLOW'], "Didn't receive the right error message")

        EventMachine.stop
      }
    }
  end

  def test_admin_cannot_create_a_channel_with_id_ALL
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

  def config_test_admin_post_message_larger_than_max_body_size_should_be_rejected
    @client_max_body_size = '2k'
    @client_body_buffer_size = '1k'
  end


  def test_admin_post_message_larger_than_max_body_size_should_be_rejected
    headers = {'accept' => 'application/json'}
    channel = 'ch_test_admin_post_message_larger_than_max_body_size_should_be_rejected'
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

  def config_test_admin_post_message_larger_than_body_buffer_size_should_be_accepted
    @client_max_body_size = '10k'
    @client_body_buffer_size = '1k'
  end


  def test_admin_post_message_larger_than_body_buffer_size_should_be_accepted
    headers = {'accept' => 'application/json'}
    channel = 'ch_test_admin_post_message_larger_than_body_buffer_size_should_be_accepted'
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

  def config_test_admin_post_message_shorter_than_body_buffer_size_should_be_accepted
    @client_max_body_size = '10k'
    @client_body_buffer_size = '6k'
  end


  def test_admin_post_message_shorter_than_body_buffer_size_should_be_accepted
    headers = {'accept' => 'application/json'}
    channel = 'ch_test_admin_post_message_shorter_than_body_buffer_size_should_be_accepted'
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

  def config_test_admin_stored_messages
    @store_messages = "on"
  end

  def test_admin_stored_messages
    headers = {'accept' => 'application/json'}
    body = 'published message'
    channel = 'ch_test_admin_stored_messages'

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

  def config_test_admin_not_stored_messages
    @store_messages = "off"
  end

  def test_admin_not_stored_messages
    headers = {'accept' => 'application/json'}
    body = 'published message'
    channel = 'ch_test_admin_not_stored_messages'

    EventMachine.run {
      pub = EventMachine::HttpRequest.new(nginx_address + '/pub?id=' + channel.to_s ).post :head => headers, :body => body, :timeout => 30
      pub.callback {
        response = JSON.parse(pub.response)
        assert_equal(0, response["stored_messages"].to_i, "Stored messages")
        EventMachine.stop
      }
    }
  end

  def config_test_admin_max_stored_messages
    @store_messages = "on"
    @max_message_buffer_length = 4
  end

  def test_admin_max_stored_messages
    headers = {'accept' => 'application/json'}
    body_prefix = 'published message '
    channel = 'ch_test_admin_max_stored_messages'
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

  def config_test_admin_max_channel_id_length
    @max_channel_id_length = 5
  end

  def test_admin_max_channel_id_length
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

  def config_test_admin_max_number_of_channels
    @max_number_of_channels = 1
  end

  def test_admin_max_number_of_channels
    headers = {'accept' => 'application/json'}
    body = 'published message'
    channel = 'ch_test_admin_max_number_of_channels_'

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

  def config_test_admin_max_number_of_broadcast_channels
    @max_number_of_broadcast_channels = 1
    @broadcast_channel_prefix = 'bd_'
    @broadcast_channel_max_qtd = 1
  end

  def test_admin_max_number_of_broadcast_channels
    headers = {'accept' => 'application/json'}
    body = 'published message'
    channel = 'bd_test_admin_max_number_of_broadcast_channels_'

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

  def test_delete_channel_whithout_subscribers
    headers = {'accept' => 'application/json'}
    body = 'published message'
    channel = 'test_delete_channel_whithout_subscribers'

    publish_message(channel, headers, body)

    EventMachine.run {
      pub = EventMachine::HttpRequest.new(nginx_address + '/pub?id=' + channel.to_s).delete :head => headers, :timeout => 30
      pub.callback {
        assert_equal(200, pub.response_header.status, "Request was not received")
        assert_equal(0, pub.response_header.content_length, "Should response only with headers")
        assert_equal("Channel deleted.", pub.response_header['X_NGINX_PUSHSTREAM_EXPLAIN'], "Didn't receive the right error message")

        stats = EventMachine::HttpRequest.new(nginx_address + '/channels-stats').get :head => headers, :timeout => 30
        stats.callback {
          assert_equal(200, stats.response_header.status, "Don't get channels statistics")
          assert_not_equal(0, stats.response_header.content_length, "Don't received channels statistics")
          begin
            response = JSON.parse(stats.response)
            assert(response.has_key?("channels"), "Didn't received the correct answer with channels info")
            assert_equal(0, response["channels"].to_i, "Returned values with channels created")
          rescue JSON::ParserError
            fail("Didn't receive a valid response")
          end
          EventMachine.stop
        }
      }
    }
  end

  def config_test_delete_channel_whith_subscriber_in_one_channel
    @header_template = " " # send a space as header to has a chunk received
    @footer_template = nil
    @ping_message_interval = nil
    @message_template = '{\"id\":\"~id~\", \"channel\":\"~channel~\", \"text\":\"~text~\"}'
  end

  def test_delete_channel_whith_subscriber_in_one_channel
    headers = {'accept' => 'application/json'}
    body = 'published message'
    channel = 'test_delete_channel_whith_subscriber_in_one_channel'

    resp = ""
    EventMachine.run {
      sub_1 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => headers, :timeout => 30
      sub_1.stream { |chunk|

        resp = resp + chunk
        if resp.strip.empty?
          stats = EventMachine::HttpRequest.new(nginx_address + '/channels-stats').get :head => {'accept' => 'application/json'}, :timeout => 30
          stats.callback {
            assert_equal(200, stats.response_header.status, "Don't get channels statistics")
            assert_not_equal(0, stats.response_header.content_length, "Don't received channels statistics")
            begin
              response = JSON.parse(stats.response)
              assert_equal(1, response["subscribers"].to_i, "Subscriber was not created")
              assert_equal(1, response["channels"].to_i, "Channel was not created")

              pub = EventMachine::HttpRequest.new(nginx_address + '/pub?id=' + channel.to_s).delete :head => headers, :timeout => 30
              pub.callback {
                assert_equal(200, pub.response_header.status, "Request was not received")
                assert_equal(0, pub.response_header.content_length, "Should response only with headers")
                assert_equal("Channel deleted.", pub.response_header['X_NGINX_PUSHSTREAM_EXPLAIN'], "Didn't receive the right error message")
              }
            rescue JSON::ParserError
              fail("Didn't receive a valid response")
            end
          }
        else
          begin
            response = JSON.parse(resp)
            assert_equal(channel, response["channel"], "Wrong channel")
            assert_equal(-2, response["id"].to_i, "Wrong message id")
            assert_equal("Channel deleted", response["text"], "Wrong message text")

            stats = EventMachine::HttpRequest.new(nginx_address + '/channels-stats').get :head => {'accept' => 'application/json'}, :timeout => 30
            stats.callback {
              assert_equal(200, stats.response_header.status, "Don't get channels statistics")
              assert_not_equal(0, stats.response_header.content_length, "Don't received channels statistics")
              response = JSON.parse(stats.response)
              assert_equal(0, response["subscribers"].to_i, "Subscriber was not deleted")
              assert_equal(0, response["channels"].to_i, "Channel was not deleted")
            }
          rescue JSON::ParserError
            fail("Didn't receive a valid response")
          end
          EventMachine.stop
        end
      }
      add_test_timeout
    }
  end

  def config_test_delete_channel_whith_subscriber_in_two_channels
    @header_template = " " # send a space as header to has a chunk received
    @footer_template = nil
    @ping_message_interval = nil
    @message_template = '{\"id\":\"~id~\", \"channel\":\"~channel~\", \"text\":\"~text~\"}'
  end

  def test_delete_channel_whith_subscriber_in_two_channels
    headers = {'accept' => 'application/json'}
    body = 'published message'
    channel_1 = 'test_delete_channel_whith_subscriber_in_two_channels_1'
    channel_2 = 'test_delete_channel_whith_subscriber_in_two_channels_2'
    stage1_complete = false
    stage2_complete = false

    resp = ""
    EventMachine.run {
      sub_1 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel_1.to_s + '/' + channel_2.to_s).get :head => headers, :timeout => 30
      sub_1.stream { |chunk|

        resp = resp + chunk
        if resp.strip.empty?
          stats = EventMachine::HttpRequest.new(nginx_address + '/channels-stats').get :head => {'accept' => 'application/json'}, :timeout => 30
          stats.callback {
            assert_equal(200, stats.response_header.status, "Don't get channels statistics")
            assert_not_equal(0, stats.response_header.content_length, "Don't received channels statistics")
            begin
              response = JSON.parse(stats.response)
              assert_equal(1, response["subscribers"].to_i, "Subscriber was not created")
              assert_equal(2, response["channels"].to_i, "Channel was not created")

              pub = EventMachine::HttpRequest.new(nginx_address + '/pub?id=' + channel_1.to_s).delete :head => headers, :timeout => 30
              pub.callback {
                assert_equal(200, pub.response_header.status, "Request was not received")
                assert_equal(0, pub.response_header.content_length, "Should response only with headers")
                assert_equal("Channel deleted.", pub.response_header['X_NGINX_PUSHSTREAM_EXPLAIN'], "Didn't receive the right error message")
              }
            rescue JSON::ParserError
              fail("Didn't receive a valid response")
            end
          }
        else
          begin
            if !stage1_complete
              stage1_complete = true
              response = JSON.parse(resp)
              assert_equal(channel_1, response["channel"], "Wrong channel")
              assert_equal(-2, response["id"].to_i, "Wrong message id")
              assert_equal("Channel deleted", response["text"], "Wrong message text")

              stats = EventMachine::HttpRequest.new(nginx_address + '/channels-stats').get :head => {'accept' => 'application/json'}, :timeout => 30
              stats.callback {
                assert_equal(200, stats.response_header.status, "Don't get channels statistics")
                assert_not_equal(0, stats.response_header.content_length, "Don't received channels statistics")
                response = JSON.parse(stats.response)
                assert_equal(1, response["subscribers"].to_i, "Subscriber was not deleted")
                assert_equal(1, response["channels"].to_i, "Channel was not deleted")

                pub = EventMachine::HttpRequest.new(nginx_address + '/pub?id=' + channel_2.to_s).post :head => headers, :body=> body, :timeout => 30
                pub.callback {
                  assert_equal(200, pub.response_header.status, "Request was not received")
                }
              }
            elsif !stage2_complete
              stage2_complete = true
              response = JSON.parse(resp.split("\r\n")[2])
              assert_equal(channel_2, response["channel"], "Wrong channel")
              assert_equal(1, response["id"].to_i, "Wrong message id")
              assert_equal(body, response["text"], "Wrong message id")

              pub = EventMachine::HttpRequest.new(nginx_address + '/pub?id=' + channel_2.to_s).delete :head => headers, :timeout => 30
              pub.callback {
                assert_equal(200, pub.response_header.status, "Request was not received")
                assert_equal(0, pub.response_header.content_length, "Should response only with headers")
                assert_equal("Channel deleted.", pub.response_header['X_NGINX_PUSHSTREAM_EXPLAIN'], "Didn't receive the right error message")
              }
            else
              response = JSON.parse(resp.split("\r\n")[3])
              assert_equal(channel_2, response["channel"], "Wrong channel")
              assert_equal(-2, response["id"].to_i, "Wrong message id")
              assert_equal("Channel deleted", response["text"], "Wrong message text")

              stats = EventMachine::HttpRequest.new(nginx_address + '/channels-stats').get :head => {'accept' => 'application/json'}, :timeout => 30
              stats.callback {
                assert_equal(200, stats.response_header.status, "Don't get channels statistics")
                assert_not_equal(0, stats.response_header.content_length, "Don't received channels statistics")
                response = JSON.parse(stats.response)
                assert_equal(0, response["subscribers"].to_i, "Subscriber was not deleted")
                assert_equal(0, response["channels"].to_i, "Channel was not deleted")
                EventMachine.stop
              }
            end
          rescue JSON::ParserError
            EventMachine.stop
            fail("Didn't receive a valid response")
          end
        end
      }
      add_test_timeout
    }
  end

  def config_test_delete_channels_whith_subscribers
    @header_template = nil
    @footer_template = "FOOTER"
    @ping_message_interval = nil
    @memory_cleanup_timeout = nil
    @message_template = '{\"id\":\"~id~\", \"channel\":\"~channel~\", \"text\":\"~text~\"}'
  end

  def test_delete_channels_whith_subscribers
    headers = {'accept' => 'application/json'}
    body = 'published message'
    channel_1 = 'test_delete_channels_whith_subscribers_1'
    channel_2 = 'test_delete_channels_whith_subscribers_2'


    EventMachine.run {
      resp_1 = ""
      sub_1 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel_1.to_s).get :head => headers, :timeout => 30
      sub_1.stream { |chunk|
        resp_1 += chunk
      }
      sub_1.callback {
        assert_equal("{\"id\":\"-2\", \"channel\":\"test_delete_channels_whith_subscribers_1\", \"text\":\"Channel deleted\"}\r\nFOOTER\r\n", resp_1, "Subscriber was not created")
      }

      resp_2 = ""
      sub_2 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel_2.to_s).get :head => headers, :timeout => 30
      sub_2.stream { |chunk|
        resp_2 += chunk
      }
      sub_2.callback {
        assert_equal("{\"id\":\"-2\", \"channel\":\"test_delete_channels_whith_subscribers_2\", \"text\":\"Channel deleted\"}\r\nFOOTER\r\n", resp_2, "Subscriber was not created")
      }

      stats = EventMachine::HttpRequest.new(nginx_address + '/channels-stats').get :head => {'accept' => 'application/json'}, :timeout => 30
      stats.callback {
        assert_equal(200, stats.response_header.status, "Don't get channels statistics")
        assert_not_equal(0, stats.response_header.content_length, "Don't received channels statistics")
        begin
          response = JSON.parse(stats.response)
          assert_equal(2, response["subscribers"].to_i, "Subscriber was not created")
          assert_equal(2, response["channels"].to_i, "Channel was not created")
        rescue JSON::ParserError
          fail("Didn't receive a valid response")
        end
      }

      pub_1 = EventMachine::HttpRequest.new(nginx_address + '/pub?id=' + channel_1.to_s).delete :head => headers, :timeout => 30
      pub_1.callback {
        assert_equal(200, pub_1.response_header.status, "Request was not received")
        assert_equal(0, pub_1.response_header.content_length, "Should response only with headers")
        assert_equal("Channel deleted.", pub_1.response_header['X_NGINX_PUSHSTREAM_EXPLAIN'], "Didn't receive the right error message")
      }

      pub_2 = EventMachine::HttpRequest.new(nginx_address + '/pub?id=' + channel_2.to_s).delete :head => headers, :timeout => 30
      pub_2.callback {
        assert_equal(200, pub_2.response_header.status, "Request was not received")
        assert_equal(0, pub_2.response_header.content_length, "Should response only with headers")
        assert_equal("Channel deleted.", pub_2.response_header['X_NGINX_PUSHSTREAM_EXPLAIN'], "Didn't receive the right error message")
      }

      EM.add_timer(5) {
        stats_2 = EventMachine::HttpRequest.new(nginx_address + '/channels-stats').get :head => {'accept' => 'application/json'}, :timeout => 30
        stats_2.callback {
          assert_equal(200, stats_2.response_header.status, "Don't get channels statistics")
          assert_not_equal(0, stats_2.response_header.content_length, "Don't received channels statistics")
          begin
            response = JSON.parse(stats_2.response)
            assert_equal(0, response["subscribers"].to_i, "Subscriber was not created")
            assert_equal(0, response["channels"].to_i, "Channel was not created")
          rescue JSON::ParserError
            fail("Didn't receive a valid response")
          end
          EventMachine.stop
        }
      }
      add_test_timeout(10)
    }
  end

  def config_test_receive_footer_template_when_channel_is_deleted
    @header_template = "HEADER_TEMPLATE"
    @footer_template = "FOOTER_TEMPLATE"
    @ping_message_interval = nil
    @message_template = '~text~'
  end

  def test_receive_footer_template_when_channel_is_deleted
    headers = {'accept' => 'application/json'}
    body = 'published message'
    channel = 'ch_test_receive_footer_template_when_channel_is_deleted'

    resp = ""
    EventMachine.run {
      sub_1 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => headers, :timeout => 30
      sub_1.stream { |chunk|

        resp = resp + chunk
        if resp == "#{@header_template}\r\n"
          pub = EventMachine::HttpRequest.new(nginx_address + '/pub?id=' + channel.to_s).delete :head => headers, :timeout => 30
          pub.callback {
            assert_equal(200, pub.response_header.status, "Request was not received")
            assert_equal(0, pub.response_header.content_length, "Should response only with headers")
            assert_equal("Channel deleted.", pub.response_header['X_NGINX_PUSHSTREAM_EXPLAIN'], "Didn't receive the right error message")
          }
        end
      }
      sub_1.callback {
        assert_equal("#{@header_template}\r\nChannel deleted\r\n#{@footer_template}\r\n", resp, "Didn't receive complete message")
        EventMachine.stop
      }

      add_test_timeout
    }
  end

  def config_test_different_header_and_footer_template_by_location
    @header_template = "HEADER_TEMPLATE"
    @footer_template = "FOOTER_TEMPLATE"
    @header_template2 = "<html><body>"
    @footer_template2 = "</body></html>"
    @ping_message_interval = nil
    @message_template = '~text~'
    @extra_location = %{
            location ~ /sub2/(.*)? {
                # activate subscriber mode for this location
                push_stream_subscriber;

                # positional channel path
                set $push_stream_channels_path          $1;
                push_stream_header_template "#{@header_template2}";
                push_stream_footer_template "#{@footer_template2}";
                push_stream_message_template "|~text~|";
            }
    }
  end

  def test_different_header_and_footer_template_by_location
    headers = {'accept' => 'application/json'}
    body = 'published message'
    channel = 'ch_test_different_header_and_footer_template_by_location'

    resp = ""
    resp2 = ""

    EventMachine.run {
      sub_1 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => headers, :timeout => 30
      sub_1.stream { |chunk|
        resp = resp + chunk
      }

      sub_2 = EventMachine::HttpRequest.new(nginx_address + '/sub2/' + channel.to_s).get :head => headers, :timeout => 30
      sub_2.stream { |chunk|
        resp2 = resp2 + chunk
      }

      EM.add_timer(1) do
        pub = EventMachine::HttpRequest.new(nginx_address + '/pub?id=' + channel.to_s).delete :head => headers, :timeout => 30
        pub.callback {
          assert_equal(200, pub.response_header.status, "Request was not received")
          assert_equal(0, pub.response_header.content_length, "Should response only with headers")
          assert_equal("Channel deleted.", pub.response_header['X_NGINX_PUSHSTREAM_EXPLAIN'], "Didn't receive the right error message")
        }
      end

      EM.add_timer(2) do
        assert_equal("#{@header_template}\r\nChannel deleted\r\n#{@footer_template}\r\n", resp, "Didn't receive complete message")
        assert_equal("#{@header_template2}\r\n|Channel deleted|\r\n#{@footer_template2}\r\n", resp2, "Didn't receive complete message")
        EventMachine.stop
      end

      add_test_timeout
    }
  end

  def config_test_custom_channel_deleted_message_text
    @channel_deleted_message_text = "Channel has gone away."
    @header_template = " " # send a space as header to has a chunk received
    @footer_template = nil
    @ping_message_interval = nil
    @message_template = '{\"id\":\"~id~\", \"channel\":\"~channel~\", \"text\":\"~text~\"}'
  end

  def test_custom_channel_deleted_message_text
    headers = {'accept' => 'application/json'}
    body = 'published message'
    channel = 'test_custom_channel_deleted_message_text'

    resp = ""
    EventMachine.run {
      sub_1 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => headers, :timeout => 30
      sub_1.stream { |chunk|

        resp = resp + chunk
        if resp.strip.empty?
          pub = EventMachine::HttpRequest.new(nginx_address + '/pub?id=' + channel.to_s).delete :head => headers, :timeout => 30
          pub.callback {
            assert_equal(200, pub.response_header.status, "Request was not received")
            assert_equal(0, pub.response_header.content_length, "Should response only with headers")
            assert_equal("Channel deleted.", pub.response_header['X_NGINX_PUSHSTREAM_EXPLAIN'], "Didn't receive the right error message")
          }
        else
          begin
            response = JSON.parse(resp)
            assert_equal(channel, response["channel"], "Wrong channel")
            assert_equal(-2, response["id"].to_i, "Wrong message id")
            assert_equal(@channel_deleted_message_text, response["text"], "Wrong message text")
          rescue JSON::ParserError
            fail("Didn't receive a valid response")
          end
          EventMachine.stop
        end
      }

      add_test_timeout
    }
  end

end
