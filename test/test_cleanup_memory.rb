require File.expand_path('base_test_case', File.dirname(__FILE__))

class TestCreateManyChannels < Test::Unit::TestCase
  include BaseTestCase

  def config_test_message_cleanup
    @min_message_buffer_timeout = '10s'
    @max_reserved_memory = "129k"
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

      EM.add_periodic_timer(0.001) do
        pub_1 = EventMachine::HttpRequest.new(nginx_address + '/pub?id=' + channel.to_s).post :head => headers, :body => body, :timeout => 60
        pub_1.callback {
          EventMachine.stop if pub_1.response_header.status == 500
        }
      end
    }

    EventMachine.run {
      # ensure channel will not be cleaned up
      sub_1 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => headers, :timeout => 60

      stored_messages_setp_1 = 0
      stored_messages_setp_2 = 0
      pub_2 = EventMachine::HttpRequest.new(nginx_address + '/channels-stats?id=' + channel.to_s).get :head => headers, :timeout => 60
      pub_2.callback {
        assert_equal(200, pub_2.response_header.status, "Don't get channels statistics")
        assert_not_equal(0, pub_2.response_header.content_length, "Don't received channels statistics")
        stored_messages_setp_1 = JSON.parse(pub_2.response)["stored_messages"].to_i
        fail("Don't create any message") if stored_messages_setp_1 == 0

        i = 0
        EM.add_periodic_timer(1) do
          pub_3 = EventMachine::HttpRequest.new(nginx_address + '/channels-stats?id=' + channel.to_s).get :head => headers, :timeout => 60
          pub_3.callback {
            assert_equal(200, pub_3.response_header.status, "Don't get channels statistics")
            assert_not_equal(0, pub_3.response_header.content_length, "Don't received channels statistics")
            stored_messages_setp_2 = JSON.parse(pub_3.response)["stored_messages"].to_i

            if (stored_messages_setp_1 > stored_messages_setp_2)
              pub_4 = EventMachine::HttpRequest.new(nginx_address + '/pub?id=' + channel.to_s).post :head => headers, :body => body, :timeout => 60
              pub_4.callback {
                EventMachine.stop if (pub_4.response_header.status == 200)
              }
            end

            fail("Don't free the memory in 60 seconds") if (i == 60)
            i += 1
          }
        end
      }
      add_test_timeout(65)
    }
  end

  def config_test_message_cleanup_without_set_message_ttl
    @store_messages = 'on'
    @min_message_buffer_timeout = nil
    @max_message_buffer_length = 5
    @memory_cleanup_timeout = '30s'
  end

  def test_message_cleanup_without_set_message_ttl
    channel = 'ch_test_message_cleanup_without_set_message_ttl'
    headers = {'accept' => 'text/html'}
    body = 'message to create a channel'

    EventMachine.run {
      10.times do |i|
        publish_message_inline(channel, headers, body)
        if i == 9
          pub_1 = EventMachine::HttpRequest.new(nginx_address + '/channels-stats?id=' + channel.to_s).get :head => headers, :timeout => 60
          pub_1.callback {
            assert_equal(200, pub_1.response_header.status, "Don't get channels statistics")
            assert_not_equal(0, pub_1.response_header.content_length, "Don't received channels statistics")
            assert_equal(@max_message_buffer_length, JSON.parse(pub_1.response)["stored_messages"].to_i, "Don't store messages")

            sleep(15) # wait cleanup timer to be executed one time

            pub_2 = EventMachine::HttpRequest.new(nginx_address + '/channels-stats?id=' + channel.to_s).get :head => headers, :timeout => 60
            pub_2.callback {
              assert_equal(200, pub_2.response_header.status, "Don't get channels statistics")
              assert_not_equal(0, pub_2.response_header.content_length, "Don't received channels statistics")
              assert_equal(@max_message_buffer_length, JSON.parse(pub_2.response)["stored_messages"].to_i, "Don't store messages")

              EventMachine.stop
            }
          }
        end
      end

      add_test_timeout(20)
    }
  end

  def config_test_message_cleanup_without_max_messages_stored_per_channel
    @store_messages = 'on'
    @min_message_buffer_timeout = '10s'
    @max_message_buffer_length = nil
    @memory_cleanup_timeout = '30s'
  end

  def test_message_cleanup_without_max_messages_stored_per_channel
    channel = 'ch_test_message_cleanup_without_max_messages_stored_per_channel'
    headers = {'accept' => 'text/html'}
    body = 'message to create a channel'
    messages_to_publish = 10

    count = 0
    stored_messages_setp_1 = 0

    EventMachine.run {
      EM.add_periodic_timer(messages_to_publish / 12.to_f) do # publish messages before cleanup timer be executed
        if (count < messages_to_publish)
          publish_message_inline(channel, headers, body)
        elsif (count == messages_to_publish)
          pub_1 = EventMachine::HttpRequest.new(nginx_address + '/channels-stats?id=' + channel.to_s).get :head => headers, :timeout => 60
          pub_1.callback {
            assert_equal(200, pub_1.response_header.status, "Don't get channels statistics")
            assert_not_equal(0, pub_1.response_header.content_length, "Don't received channels statistics")
            stored_messages_setp_1 = JSON.parse(pub_1.response)["stored_messages"].to_i
            assert_equal(messages_to_publish, stored_messages_setp_1, "Don't store messages")
          }
        end
        count += 1
      end

      EM.add_timer(15) do # wait cleanup timer to be executed one time
        pub_2 = EventMachine::HttpRequest.new(nginx_address + '/channels-stats?id=' + channel.to_s).get :head => headers, :timeout => 60
        pub_2.callback {
          assert_equal(200, pub_2.response_header.status, "Don't get channels statistics")
          assert_not_equal(0, pub_2.response_header.content_length, "Don't received channels statistics")
          stored_messages_setp_2 = JSON.parse(pub_2.response)["stored_messages"].to_i
          assert(stored_messages_setp_1 > stored_messages_setp_2, "Don't clear messages")
          assert(stored_messages_setp_2 >= (messages_to_publish / 2), "Cleared all messages")

          EventMachine.stop
        }
      end

      add_test_timeout(20)
    }
  end

  def config_test_channel_cleanup
    @min_message_buffer_timeout = '10s'
    @max_reserved_memory = "129k"
    @memory_cleanup_timeout = '30s'
  end

  def test_channel_cleanup
    channel = 'ch_test_channel_cleanup_'
    headers = {'accept' => 'text/html'}
    body = 'message to create a channel'

    EventMachine.run {
      i = 0
      EM.add_periodic_timer(0.001) do
        pub_1 = EventMachine::HttpRequest.new(nginx_address + '/pub?id=' + channel.to_s + i.to_s).post :head => headers, :body => body, :timeout => 60
        pub_1.callback {
          EventMachine.stop if pub_1.response_header.status == 500
          i += 1
        }
      end
    }

    EventMachine.run {
      channels_setp_1 = 0
      channels_setp_2 = 0
      pub_2 = EventMachine::HttpRequest.new(nginx_address + '/channels-stats').get :head => headers, :timeout => 60
      pub_2.callback {
        assert_equal(200, pub_2.response_header.status, "Don't get channels statistics")
        assert_not_equal(0, pub_2.response_header.content_length, "Don't received channels statistics")
        channels_setp_1 = JSON.parse(pub_2.response)["channels"].to_i
        fail("Don't create any channel") if channels_setp_1 == 0

        i = 0
        EM.add_periodic_timer(1) do
          pub_3 = EventMachine::HttpRequest.new(nginx_address + '/channels-stats').get :head => headers, :timeout => 60
          pub_3.callback {
            assert_equal(200, pub_3.response_header.status, "Don't get channels statistics")
            assert_not_equal(0, pub_3.response_header.content_length, "Don't received channels statistics")
            channels_setp_2 = JSON.parse(pub_3.response)["channels"].to_i

            if (channels_setp_1 > channels_setp_2)
              pub_4 = EventMachine::HttpRequest.new(nginx_address + '/pub?id=' + channel.to_s + (i + 1).to_s).post :head => headers, :body => body, :timeout => 60
              pub_4.callback {
                EventMachine.stop if (pub_4.response_header.status == 200)
              }
            end

            fail("Don't free the memory in 60 seconds") if (i == 60)
            i += 1
          }
        end
      }
      add_test_timeout(65)
    }
  end

  def config_test_message_cleanup_with_store_off_with_subscriber
    @max_reserved_memory = "129k"
    @store_messages = 'off'
    @memory_cleanup_timeout = '30s'
  end

  def test_message_cleanup_with_store_off_with_subscriber
    channel = 'ch_test_message_cleanup_with_store_off_with_subscriber'
    headers = {'accept' => 'text/html'}
    body = 'message to create a channel'

    EventMachine.run {
      # ensure space for a subscriber after memory was full
      sub_1 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => headers, :timeout => 60

      EM.add_periodic_timer(0.001) do
        pub_1 = EventMachine::HttpRequest.new(nginx_address + '/pub?id=' + channel.to_s).post :head => headers, :body => body, :timeout => 60
        pub_1.callback {
          EventMachine.stop if (pub_1.response_header.status == 500)
        }
      end
    }

    i = 0
    EventMachine.run {
      EM.add_periodic_timer(1) do
        pub_2 = EventMachine::HttpRequest.new(nginx_address + '/pub?id=' + channel.to_s).post :head => headers, :body => body, :timeout => 60
        pub_2.callback {
          fail("Don't free the memory in 60 seconds") if (i == 60)
          EventMachine.stop if (pub_2.response_header.status == 200)
          i += 1
        }
      end
      add_test_timeout(65)
    }
  end

  def config_test_message_cleanup_with_store_off_without_subscriber
    @max_reserved_memory = "129k"
    @store_messages = 'off'
    @memory_cleanup_timeout = '30s'
  end

  def test_message_cleanup_with_store_off_without_subscriber
    channel = 'ch_test_message_cleanup_with_store_off_without_subscriber'
    headers = {'accept' => 'text/html'}
    body = 'message to create a channel'

    EventMachine.run {
      EM.add_periodic_timer(0.001) do
        pub_1 = EventMachine::HttpRequest.new(nginx_address + '/pub?id=' + channel.to_s).post :head => headers, :body => body, :timeout => 60
        pub_1.callback {
          EventMachine.stop if (pub_1.response_header.status == 500)
        }
      end
    }

    i = 0
    EventMachine.run {
      EM.add_periodic_timer(1) do
        pub_2 = EventMachine::HttpRequest.new(nginx_address + '/pub?id=' + channel.to_s).post :head => headers, :body => body, :timeout => 60
        pub_2.callback {
          fail("Don't free the memory in 60 seconds") if (i == 60)
          EventMachine.stop if (pub_2.response_header.status == 200)
          i += 1
        }
      end
      add_test_timeout(65)
    }
  end

end
