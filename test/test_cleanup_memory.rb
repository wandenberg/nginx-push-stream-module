require File.expand_path('base_test_case', File.dirname(__FILE__))

class TestCleanupMemory < Test::Unit::TestCase
  include BaseTestCase

  def global_configuration
    ENV['NGINX_WORKERS'] = '1'
    @disable_ignore_childs = true
    @master_process = 'on'
    @daemon = 'on'
  end

  def config_test_channel_cleanup_after_delete
    @memory_cleanup_timeout = '30s'
    @max_reserved_memory = "129k"
    @min_message_buffer_timeout = '10s'
    @max_message_buffer_length = nil
    @publisher_mode = 'admin'
  end

  def test_channel_cleanup_after_delete
    channel = 'ch_test_channel_cleanup'
    headers = {'accept' => 'text/html'}
    body = 'message to create a channel'

    published_messages_setp_1 = 0

    EventMachine.run do
      i = 0
      fill_memory_timer = EventMachine::PeriodicTimer.new(0.001) do
        pub_1 = EventMachine::HttpRequest.new(nginx_address + '/pub?id=' + channel.to_s + i.to_s).post :body => body, :head => headers, :timeout => 30
        pub_1.callback do
          if pub_1.response_header.status == 500
            fill_memory_timer.cancel
            i.times do |j|
              pub = EventMachine::HttpRequest.new(nginx_address + '/pub?id=' + channel.to_s + j.to_s).delete :head => headers, :timeout => 30
            end
            pub_2 = EventMachine::HttpRequest.new(nginx_address + '/channels-stats').get :head => headers, :timeout => 60
            pub_2.callback do
              fail("Don't received the stats") if (pub_2.response_header.status != 200) || (pub_2.response_header.content_length == 0)
              result = JSON.parse(pub_2.response)
              published_messages_setp_1 = result["published_messages"].to_i
              fail("Don't create any message") if published_messages_setp_1 == 0

              EM.add_timer(35) do
                i = 0
                fill_memory_timer = EventMachine::PeriodicTimer.new(0.001) do
                  pub_1 = EventMachine::HttpRequest.new(nginx_address + '/pub?id=' + channel.to_s + i.to_s).post :body => body, :head => headers, :timeout => 30
                  pub_1.callback do
                    if pub_1.response_header.status == 500
                      fill_memory_timer.cancel
                      pub_2 = EventMachine::HttpRequest.new(nginx_address + '/channels-stats').get :head => headers, :timeout => 60
                      pub_2.callback do
                        fail("Don't received the stats") if (pub_2.response_header.status != 200) || (pub_2.response_header.content_length == 0)
                        result = JSON.parse(pub_2.response)
                        assert_equal(published_messages_setp_1, result["published_messages"].to_i / 2, "Don't cleaned all memory")
                        EventMachine.stop
                      end
                    end
                  end
                  i += 1
                end
              end

            end
          end
        end
        i += 1
      end
      add_test_timeout(45)
    end
  end

  def create_and_delete_channel(channel, body, headers, &block)
    pub_1 = EventMachine::HttpRequest.new(nginx_address + '/pub?id=' + channel.to_s).post :body => body, :head => headers, :timeout => 30
    pub_1.callback do
      if pub_1.response_header.status == 200
        pub = EventMachine::HttpRequest.new(nginx_address + '/pub?id=' + channel.to_s).delete :head => headers, :timeout => 30
        pub.callback do
          block.call((pub.response_header.status == 200) ? :success : :error)
        end
      else
        block.call(:error)
      end
    end
  end

  def create_and_delete_channel_in_loop(channel, body, headers, &block)
    create_and_delete_channel(channel, body, headers) do |status|
      if status == :success
        create_and_delete_channel_in_loop(channel, body, headers) do
          yield
        end
      else
        block.call unless block.nil?
      end
    end
  end

  def config_test_channel_cleanup_after_delete_same_id
    @memory_cleanup_timeout = '30s'
    @max_reserved_memory = "129k"
    @min_message_buffer_timeout = '10s'
    @max_message_buffer_length = nil
    @publisher_mode = 'admin'
  end

  def test_channel_cleanup_after_delete_same_id
    channel = 'ch_test_channel_cleanup'
    headers = {'accept' => 'text/html'}
    body = 'message to create a channel'

    published_messages_setp_1 = 0

    EventMachine.run do
      create_and_delete_channel_in_loop(channel, body, headers) do
        pub_2 = EventMachine::HttpRequest.new(nginx_address + '/channels-stats').get :head => headers, :timeout => 60
        pub_2.callback do
          fail("Don't received the stats") if (pub_2.response_header.status != 200) || (pub_2.response_header.content_length == 0)
          result = JSON.parse(pub_2.response)
          published_messages_setp_1 = result["published_messages"].to_i
          fail("Don't create any message") if published_messages_setp_1 == 0

          EM.add_timer(40) do
            create_and_delete_channel_in_loop(channel, body, headers) do
              pub_2 = EventMachine::HttpRequest.new(nginx_address + '/channels-stats').get :head => headers, :timeout => 60
              pub_2.callback do
                fail("Don't received the stats") if (pub_2.response_header.status != 200) || (pub_2.response_header.content_length == 0)
                result = JSON.parse(pub_2.response)
                assert_equal(published_messages_setp_1, result["published_messages"].to_i / 2, "Don't cleaned all memory")
                EventMachine.stop
              end
            end
          end
        end
      end
      add_test_timeout(50)
    end
  end

  def config_test_message_cleanup
    @memory_cleanup_timeout = '30s'
    @max_reserved_memory = "129k"
    @min_message_buffer_timeout = '10s'
    @max_message_buffer_length = 100
  end

  def test_message_cleanup
    channel = 'ch_test_message_cleanup'
    headers = {'accept' => 'text/html'}
    body = 'message to create a channel'

    stored_messages_setp_1 = 0
    published_messages_setp_1 = 0

    EventMachine.run do
      # ensure channel will not be cleaned up
      sub_1 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => headers, :timeout => 60

      fill_memory_timer = EventMachine::PeriodicTimer.new(0.001) do
        publish_message_inline_with_callbacks(channel, headers, body, {
          :error => Proc.new do |status, content|
            fill_memory_timer.cancel
            pub_2 = EventMachine::HttpRequest.new(nginx_address + '/channels-stats?id=' + channel.to_s).get :head => headers, :timeout => 60
            pub_2.callback do
              fail("Don't received the stats") if (pub_2.response_header.status != 200) || (pub_2.response_header.content_length == 0)
              result = JSON.parse(pub_2.response)
              stored_messages_setp_1 = result["stored_messages"].to_i
              published_messages_setp_1 = result["published_messages"].to_i
              assert_equal(@max_message_buffer_length, stored_messages_setp_1, "Don't limit stored messages")
              fail("Don't reached the limit of stored messages") if result["published_messages"].to_i <= @max_message_buffer_length
              fail("Don't create any message") if stored_messages_setp_1 == 0
              EM.add_timer(45) do
                pub_3 = EventMachine::HttpRequest.new(nginx_address + '/channels-stats?id=' + channel.to_s).get :head => headers, :timeout => 60
                pub_3.callback do
                  fail("Don't received the stats") if (pub_3.response_header.status != 200) || (pub_3.response_header.content_length == 0)
                  assert_equal(0, JSON.parse(pub_3.response)["stored_messages"].to_i, "Don't cleaned all messages")

                  fill_memory_timer = EventMachine::PeriodicTimer.new(0.001) do
                    publish_message_inline_with_callbacks(channel, headers, body, {
                      :error => Proc.new do |status2, content2|
                        pub_4 = EventMachine::HttpRequest.new(nginx_address + '/channels-stats?id=' + channel.to_s).get :head => headers, :timeout => 60
                        pub_4.callback do
                          fail("Don't received the stats") if (pub_4.response_header.status != 200) || (pub_4.response_header.content_length == 0)
                          result = JSON.parse(pub_4.response)
                          assert_equal(stored_messages_setp_1, result["stored_messages"].to_i, "Don't cleaned all messages")
                          assert_equal(published_messages_setp_1, result["published_messages"].to_i / 2, "Don't cleaned all memory")
                          EventMachine.stop
                        end
                      end
                    })
                  end
                end
              end
            end
          end
        })
      end
      add_test_timeout(55)
    end
  end

  def config_test_discard_old_messages
    @memory_cleanup_timeout = '30s'
    @max_reserved_memory = "129k"
    @min_message_buffer_timeout = '10s'
    @max_message_buffer_length = nil
  end

  def test_discard_old_messages
    channel = 'ch_test_discard_old_messages'
    headers = {'accept' => 'text/html'}
    body = 'message to create a channel'
    messages_to_publish = 10

    count = 0
    stored_messages_setp_1 = 0

    EventMachine.run do
      fill_memory_timer = EventMachine::PeriodicTimer.new(messages_to_publish / 12.to_f) do # publish messages before cleanup timer be executed
        if (count < messages_to_publish)
          publish_message_inline(channel, headers, body)
        elsif (count == messages_to_publish)
          pub_1 = EventMachine::HttpRequest.new(nginx_address + '/channels-stats?id=' + channel.to_s).get :head => headers, :timeout => 60
          pub_1.callback do
            fill_memory_timer.cancel
            fail("Don't received the stats") if (pub_1.response_header.status != 200) || (pub_1.response_header.content_length == 0)
            stored_messages_setp_1 = JSON.parse(pub_1.response)["stored_messages"].to_i
            assert_equal(messages_to_publish, stored_messages_setp_1, "Don't store messages")

            EM.add_timer(5) do # wait cleanup timer to be executed one time
              pub_2 = EventMachine::HttpRequest.new(nginx_address + '/channels-stats?id=' + channel.to_s).get :head => headers, :timeout => 60
              pub_2.callback do
                fail("Don't received the stats") if (pub_2.response_header.status != 200) || (pub_2.response_header.content_length == 0)
                stored_messages_setp_2 = JSON.parse(pub_2.response)["stored_messages"].to_i
                assert(stored_messages_setp_1 > stored_messages_setp_2, "Don't clear messages")
                assert(stored_messages_setp_2 > 0, "Cleared all messages")

                EventMachine.stop
              end
            end
          end
        end
        count += 1
      end
      add_test_timeout(20)
    end
  end

  def config_test_message_cleanup_without_max_messages_stored_per_channel
    @memory_cleanup_timeout = '30s'
    @max_reserved_memory = "129k"
    @min_message_buffer_timeout = '10s'
    @max_message_buffer_length = nil
  end

  def test_message_cleanup_without_max_messages_stored_per_channel
    channel = 'ch_test_message_cleanup_without_max_messages_stored_per_chann'
    headers = {'accept' => 'text/html'}
    body = 'message to create a channel'

    stored_messages_setp_1 = 0
    published_messages_setp_1 = 0

    EventMachine.run do
      # ensure channel will not be cleaned up
      sub_1 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => headers, :timeout => 60

      fill_memory_timer = EventMachine::PeriodicTimer.new(0.001) do
        publish_message_inline_with_callbacks(channel, headers, body, {
          :error => Proc.new do |status, content|
            fill_memory_timer.cancel
            pub_2 = EventMachine::HttpRequest.new(nginx_address + '/channels-stats?id=' + channel.to_s).get :head => headers, :timeout => 60
            pub_2.callback do
              fail("Don't received the stats") if (pub_2.response_header.status != 200) || (pub_2.response_header.content_length == 0)
              result = JSON.parse(pub_2.response)
              stored_messages_setp_1 = result["stored_messages"].to_i
              published_messages_setp_1 = result["published_messages"].to_i
              fail("Limited the number of stored messages") if stored_messages_setp_1 <= 100
              fail("Don't create any message") if stored_messages_setp_1 == 0

              EM.add_timer(45) do
                pub_3 = EventMachine::HttpRequest.new(nginx_address + '/channels-stats?id=' + channel.to_s).get :head => headers, :timeout => 60
                pub_3.callback do
                  fail("Don't received the stats") if (pub_3.response_header.status != 200) || (pub_3.response_header.content_length == 0)
                  assert_equal(0, JSON.parse(pub_3.response)["stored_messages"].to_i, "Don't cleaned all messages")

                  fill_memory_timer = EventMachine::PeriodicTimer.new(0.001) do
                    publish_message_inline_with_callbacks(channel, headers, body, {
                      :error => Proc.new do |status2, content2|
                        pub_4 = EventMachine::HttpRequest.new(nginx_address + '/channels-stats?id=' + channel.to_s).get :head => headers, :timeout => 60
                        pub_4.callback do
                          fail("Don't received the stats") if (pub_4.response_header.status != 200) || (pub_4.response_header.content_length == 0)
                          result = JSON.parse(pub_4.response)
                          assert_equal(stored_messages_setp_1, result["stored_messages"].to_i, "Don't cleaned all messages")
                          assert_equal(published_messages_setp_1, result["published_messages"].to_i / 2, "Don't cleaned all memory")
                          EventMachine.stop
                        end
                      end
                    })
                  end
                end
              end
            end
          end
        })
      end

      add_test_timeout(55)
    end
  end

  def config_test_channel_cleanup
    @memory_cleanup_timeout = '30s'
    @max_reserved_memory = "129k"
    @min_message_buffer_timeout = '2s'
    @max_message_buffer_length = nil
  end

  def test_channel_cleanup
    channel = 'ch_test_channel_cleanup_'
    headers = {'accept' => 'text/html'}
    body = 'message to create a channel'

    channels_setp_1 = 0
    channels_setp_2 = 0

    EventMachine.run do
      i = 0
      fill_memory_timer = EventMachine::PeriodicTimer.new(0.001) do
        publish_message_inline_with_callbacks(channel + i.to_s, headers, body, {
          :error => Proc.new do |status, content|
            fill_memory_timer.cancel
            pub_2 = EventMachine::HttpRequest.new(nginx_address + '/channels-stats').get :head => headers, :timeout => 60
            pub_2.callback do
              fail("Don't received the stats") if (pub_2.response_header.status != 200) || (pub_2.response_header.content_length == 0)
              channels_setp_1 = JSON.parse(pub_2.response)["channels"].to_i
              fail("Don't create any channel") if channels_setp_1 == 0

              EM.add_timer(40) do
                pub_3 = EventMachine::HttpRequest.new(nginx_address + '/channels-stats').get :head => headers, :timeout => 60
                pub_3.callback do
                  fail("Don't received the stats") if (pub_3.response_header.status != 200) || (pub_3.response_header.content_length == 0)
                  channels = JSON.parse(pub_3.response)["channels"].to_i

                  assert_equal(0, channels, "Don't removed all channels")
                  EventMachine.stop unless (channels == 0)

                  EM.add_timer(35) do
                    i = 0
                    fill_memory_timer = EventMachine::PeriodicTimer.new(0.001) do
                      publish_message_inline_with_callbacks(channel + i.to_s, headers, body, {
                        :error => Proc.new do |status2, content2|
                          fill_memory_timer.cancel
                          pub_4 = EventMachine::HttpRequest.new(nginx_address + '/channels-stats').get :head => headers, :timeout => 60
                          pub_4.callback do
                            fail("Don't received the stats") if (pub_4.response_header.status != 200) || (pub_4.response_header.content_length == 0)
                            channels_setp_2 = JSON.parse(pub_4.response)["channels"].to_i

                            assert_equal(channels_setp_1, channels_setp_2, "Don't released all memory")
                            EventMachine.stop
                          end
                        end
                      })
                      i += 1
                    end
                  end
                end
              end
            end
          end
        })
        i += 1
      end

      add_test_timeout(85)
    end
  end

  def config_test_message_cleanup_with_store_off_with_subscriber
    @store_messages = 'off'
    @memory_cleanup_timeout = '30s'
    @max_reserved_memory = "129k"
    @min_message_buffer_timeout = nil
    @max_message_buffer_length = nil
  end

  def test_message_cleanup_with_store_off_with_subscriber
    channel = 'ch_test_message_cleanup_with_store_off_with_subscriber'
    headers = {'accept' => 'text/html'}
    body = 'message to create a channel'

    published_messages_setp_1 = 0

    EventMachine.run do
      # ensure channel will not be cleaned up
      sub_1 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => headers, :timeout => 60

      fill_memory_timer = EventMachine::PeriodicTimer.new(0.001) do
        publish_message_inline_with_callbacks(channel, headers, body, {
          :error => Proc.new do |status, content|
            fill_memory_timer.cancel
            pub_2 = EventMachine::HttpRequest.new(nginx_address + '/channels-stats?id=' + channel.to_s).get :head => headers, :timeout => 60
            pub_2.callback do
              fail("Don't received the stats") if (pub_2.response_header.status != 200) || (pub_2.response_header.content_length == 0)
              result = JSON.parse(pub_2.response)
              published_messages_setp_1 = result["published_messages"].to_i

              EM.add_timer(40) do
                pub_3 = EventMachine::HttpRequest.new(nginx_address + '/channels-stats?id=' + channel.to_s).get :head => headers, :timeout => 60
                pub_3.callback do
                  fail("Don't received the stats") if (pub_3.response_header.status != 200) || (pub_3.response_header.content_length == 0)
                  assert_equal(0, JSON.parse(pub_3.response)["channels"].to_i, "Don't cleaned all messages/channels")

                  fill_memory_timer = EventMachine::PeriodicTimer.new(0.001) do
                    publish_message_inline_with_callbacks(channel, headers, body, {
                      :error => Proc.new do |status2, content2|
                        pub_4 = EventMachine::HttpRequest.new(nginx_address + '/channels-stats?id=' + channel.to_s).get :head => headers, :timeout => 60
                        pub_4.callback do
                          fail("Don't received the stats") if (pub_4.response_header.status != 200) || (pub_4.response_header.content_length == 0)
                          result = JSON.parse(pub_4.response)
                          assert_equal(published_messages_setp_1, result["published_messages"].to_i / 2, "Don't cleaned all memory")
                          EventMachine.stop
                        end
                      end
                    })
                  end
                end
              end
            end
          end
        })
      end

      add_test_timeout(45)
    end
  end

  def config_test_message_cleanup_with_store_off_without_subscriber
    @store_messages = 'off'
    @memory_cleanup_timeout = '30s'
    @max_reserved_memory = "129k"
    @min_message_buffer_timeout = nil
    @max_message_buffer_length = nil
  end

  def test_message_cleanup_with_store_off_without_subscriber
    channel = 'ch_test_message_cleanup_with_store_off_without_subscriber'
    headers = {'accept' => 'text/html'}
    body = 'message to create a channel'

    published_messages_setp_1 = 0

    EventMachine.run do
      i = 0
      fill_memory_timer = EventMachine::PeriodicTimer.new(0.001) do
        publish_message_inline_with_callbacks(channel + i.to_s, headers, body, {
          :error => Proc.new do |status, content|
            fill_memory_timer.cancel
            pub_2 = EventMachine::HttpRequest.new(nginx_address + '/channels-stats').get :head => headers, :timeout => 60
            pub_2.callback do
              fail("Don't received the stats") if (pub_2.response_header.status != 200) || (pub_2.response_header.content_length == 0)
              result = JSON.parse(pub_2.response)
              published_messages_setp_1 = result["published_messages"].to_i

              EM.add_timer(35) do
                pub_3 = EventMachine::HttpRequest.new(nginx_address + '/channels-stats').get :head => headers, :timeout => 60
                pub_3.callback do
                  fail("Don't received the stats") if (pub_3.response_header.status != 200) || (pub_3.response_header.content_length == 0)
                  assert_equal(0, JSON.parse(pub_3.response)["channels"].to_i, "Don't cleaned all messages/channels")

                  EM.add_timer(35) do
                    fill_memory_timer = EventMachine::PeriodicTimer.new(0.001) do
                      publish_message_inline_with_callbacks(channel + i.to_s, headers, body, {
                        :error => Proc.new do |status2, content2|
                          pub_4 = EventMachine::HttpRequest.new(nginx_address + '/channels-stats').get :head => headers, :timeout => 60
                          pub_4.callback do
                            fail("Don't received the stats") if (pub_4.response_header.status != 200) || (pub_4.response_header.content_length == 0)
                            result = JSON.parse(pub_4.response)
                            assert_equal(published_messages_setp_1, result["published_messages"].to_i / 2, "Don't cleaned all memory")
                            EventMachine.stop
                          end
                        end
                      })
                      i += 1
                    end
                  end
                end
              end
            end
          end
        })
        i += 1
      end
      add_test_timeout(85)
    end
  end

end
