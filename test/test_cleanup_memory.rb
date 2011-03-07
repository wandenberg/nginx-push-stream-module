require File.expand_path('base_test_case', File.dirname(__FILE__))

class TestCreateManyChannels < Test::Unit::TestCase
  include BaseTestCase

  def config_test_message_cleanup
    @min_message_buffer_timeout = '10s'
    @max_reserved_memory = "32k"
    @max_message_buffer_length = 100
    @memory_cleanup_timeout = '30s'
  end

  def test_message_cleanup
    channel = 'ch_test_message_cleanup'
    headers = {'accept' => 'text/html'}
    body = 'message to create a channel'

    EventMachine.run {
      # ensure space for a subscriber after memory was full
      sub_1 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => headers, :timeout => 60
      fail_if_connecttion_error(sub_1)

      i = 0
      EM.add_periodic_timer(0.05) do
        i += 1
        pub_1 = EventMachine::HttpRequest.new(nginx_address + '/pub?id=' + channel.to_s).post :head => headers, :body => body, :timeout => 60
        pub_1.callback {
          if pub_1.response_header.status == 500
            EventMachine.stop
          end
        }
        fail_if_connecttion_error(pub_1)

      end
    }

    EventMachine.run {
      # ensure channel will not be cleaned up
      sub_1 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => headers, :timeout => 60
      fail_if_connecttion_error(sub_1)

      stored_messages_setp_1 = 0
      stored_messages_setp_2 = 0
      pub_2 = EventMachine::HttpRequest.new(nginx_address + '/channels_stats?id=' + channel.to_s).get :head => headers, :timeout => 60
      pub_2.callback {
        assert_equal(200, pub_2.response_header.status, "Don't get channels statistics")
        assert_not_equal(0, pub_2.response_header.content_length, "Don't received channels statistics")
        stored_messages_setp_1 = JSON.parse(pub_2.response)["stored_messages"].to_i

        sleep(40) #wait for message timeout and for cleanup timer

        pub_3 = EventMachine::HttpRequest.new(nginx_address + '/channels_stats?id=' + channel.to_s).get :head => headers, :timeout => 60
        pub_3.callback {
          assert_equal(200, pub_3.response_header.status, "Don't get channels statistics")
          assert_not_equal(0, pub_3.response_header.content_length, "Don't received channels statistics")
          stored_messages_setp_2 = JSON.parse(pub_3.response)["stored_messages"].to_i

          assert(stored_messages_setp_1 > stored_messages_setp_2, "Messages weren't clean up: #{stored_messages_setp_1} <= #{stored_messages_setp_2}")

          pub_4 = EventMachine::HttpRequest.new(nginx_address + '/pub?id=' + channel.to_s).post :head => headers, :body => body, :timeout => 60
          pub_4.callback {
            assert_equal(200, pub_4.response_header.status, "Don't get channels statistics")
            assert_equal(stored_messages_setp_2 + 1, JSON.parse(pub_4.response)["stored_messages"].to_i, "Don't get channels statistics")
            EventMachine.stop
          }
          fail_if_connecttion_error(pub_4)
        }
        fail_if_connecttion_error(pub_3)
      }
      fail_if_connecttion_error(pub_2)
    }
  end

  def config_test_channel_cleanup
    @min_message_buffer_timeout = '10s'
    @max_reserved_memory = "32k"
    @memory_cleanup_timeout = '30s'
  end

  def test_channel_cleanup
    channel = 'ch_test_channel_cleanup_'
    headers = {'accept' => 'text/html'}
    body = 'message to create a channel'

    i = 0
    EventMachine.run {
      EM.add_periodic_timer(0.05) do
        i += 1
        pub_1 = EventMachine::HttpRequest.new(nginx_address + '/pub?id=' + channel.to_s + i.to_s).post :head => headers, :body => body, :timeout => 60
        pub_1.callback {
          if pub_1.response_header.status == 500
            EventMachine.stop
          end
        }
        fail_if_connecttion_error(pub_1)
      end
    }

    EventMachine.run {
      channels_setp_1 = 0
      channels_setp_2 = 0
      pub_2 = EventMachine::HttpRequest.new(nginx_address + '/channels_stats').get :head => headers, :timeout => 60
      pub_2.callback {
        assert_equal(200, pub_2.response_header.status, "Don't get channels statistics")
        assert_not_equal(0, pub_2.response_header.content_length, "Don't received channels statistics")
        channels_setp_1 = JSON.parse(pub_2.response)["channels"].to_i
        assert_equal(i, channels_setp_1, "Channels were not here anymore")

        sleep(45) #wait for message timeout and for cleanup timer

        pub_3 = EventMachine::HttpRequest.new(nginx_address + '/channels_stats').get :head => headers, :timeout => 60
        pub_3.callback {
          assert_equal(200, pub_3.response_header.status, "Don't get channels statistics")
          assert_not_equal(0, pub_3.response_header.content_length, "Don't received channels statistics")
          channels_setp_2 = JSON.parse(pub_3.response)["channels"].to_i

          assert(channels_setp_1 > channels_setp_2, "Channels weren't clean up: #{channels_setp_1} <= #{channels_setp_2}")

          pub_4 = EventMachine::HttpRequest.new(nginx_address + '/pub?id=' + channel.to_s + (i + 1).to_s).post :head => headers, :body => body, :timeout => 60
          pub_4.callback {
            assert_equal(200, pub_4.response_header.status, "Don't get channels statistics")

            pub_5 = EventMachine::HttpRequest.new(nginx_address + '/channels_stats').get :head => headers, :timeout => 60
            pub_5.callback {
              assert_equal(200, pub_5.response_header.status, "Don't get channels statistics")
              assert_not_equal(0, pub_5.response_header.content_length, "Don't received channels statistics")
              assert_equal(channels_setp_2 + 1, JSON.parse(pub_5.response)["channels"].to_i, "Don't get channels statistics")
              EventMachine.stop
            }
            fail_if_connecttion_error(pub_5)
          }
          fail_if_connecttion_error(pub_4)
        }
        fail_if_connecttion_error(pub_3)
      }
      fail_if_connecttion_error(pub_2)
    }
  end

end
