require 'spec_helper'

describe "Cleanup Memory" do
  workers = 1
  old_cld_trap = nil
  before do
    workers = ENV['NGINX_WORKERS']
    ENV['NGINX_WORKERS'] = '1'
    old_cld_trap = Signal.trap("CLD", "IGNORE")
  end

  after do
    ENV['NGINX_WORKERS'] = workers
    Signal.trap("CLD", old_cld_trap)
  end

  shared_examples_for "executing on normal conditions" do

    it "should cleanup memory used for published message" do
      channel = 'ch_test_message_cleanup'
      body = 'message to create a channel'

      nginx_run_server(config.merge(:max_messages_stored_per_channel => 100), :timeout => test_timeout) do |conf|
        stored_messages_setp_1 = 0
        published_messages_setp_1 = 0
        published_messages_setp_2 = 0

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

                  stored_messages_setp_1.should eql(conf.max_messages_stored_per_channel)
                  published_messages_setp_1.should be > (conf.max_messages_stored_per_channel)
                  stored_messages_setp_1.should_not eql(0)
                  EM.add_timer(45) do
                    pub_3 = EventMachine::HttpRequest.new(nginx_address + '/channels-stats?id=' + channel.to_s).get :head => headers, :timeout => 60
                    pub_3.callback do
                      fail("Don't received the stats") if (pub_3.response_header.status != 200) || (pub_3.response_header.content_length == 0)
                      JSON.parse(pub_3.response)["stored_messages"].to_i.should eql(0)

                      execute_changes_on_environment(conf) do
                        # connect a subscriber on new worker
                        sub_1 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => headers, :timeout => 60

                        EM.add_timer(35) do
                          fill_memory_timer = EventMachine::PeriodicTimer.new(0.001) do
                            publish_message_inline_with_callbacks(channel, headers, body, {
                              :error => Proc.new do |status2, content2|
                                fill_memory_timer.cancel
                                pub_2 = EventMachine::HttpRequest.new(nginx_address + '/channels-stats?id=' + channel.to_s).get :head => headers, :timeout => 60
                                pub_2.callback do
                                  fail("Don't received the stats") if (pub_2.response_header.status != 200) || (pub_2.response_header.content_length == 0)
                                  published_messages_setp_2 = JSON.parse(pub_2.response)["published_messages"].to_i
                                  fail("Don't publish more messages") if published_messages_setp_1 == published_messages_setp_2

                                  EM.add_timer(50) do
                                    pub_3 = EventMachine::HttpRequest.new(nginx_address + '/channels-stats?id=' + channel.to_s).get :head => headers, :timeout => 60
                                    pub_3.callback do
                                      fail("Don't received the stats") if (pub_3.response_header.status != 200) || (pub_3.response_header.content_length == 0)
                                      JSON.parse(pub_3.response)["stored_messages"].to_i.should eql(0)

                                      fill_memory_timer = EventMachine::PeriodicTimer.new(0.001) do
                                        publish_message_inline_with_callbacks(channel, headers, body, {
                                          :error => Proc.new do |status3, content3|
                                            fill_memory_timer.cancel
                                            pub_4 = EventMachine::HttpRequest.new(nginx_address + '/channels-stats?id=' + channel.to_s).get :head => headers, :timeout => 60
                                            pub_4.callback do
                                              fail("Don't received the stats") if (pub_4.response_header.status != 200) || (pub_4.response_header.content_length == 0)
                                              result = JSON.parse(pub_4.response)
                                              result["stored_messages"].to_i.should eql(stored_messages_setp_1)
                                              (result["published_messages"].to_i - published_messages_setp_2).should eql(published_messages_setp_1)
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
                        end
                      end
                    end
                  end
                end
              end
            })
          end
        end
      end
    end

    it "should discard old messages" do
      channel = 'ch_test_discard_old_messages'
      body = 'message to create a channel'
      messages_to_publish = 10

      count = 0
      stored_messages_setp_1 = 0
      nginx_run_server(config, :timeout => test_timeout) do |conf|
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
                stored_messages_setp_1.should eql(messages_to_publish)

                execute_changes_on_environment(conf) do
                  EM.add_timer(5) do # wait cleanup timer to be executed one time
                    pub_2 = EventMachine::HttpRequest.new(nginx_address + '/channels-stats?id=' + channel.to_s).get :head => headers, :timeout => 60
                    pub_2.callback do
                      fail("Don't received the stats") if (pub_2.response_header.status != 200) || (pub_2.response_header.content_length == 0)
                      stored_messages_setp_2 = JSON.parse(pub_2.response)["stored_messages"].to_i
                      stored_messages_setp_2.should be <= stored_messages_setp_1
                      stored_messages_setp_2.should be > 0

                      EventMachine.stop
                    end
                  end
                end
              end
            end
            count += 1
          end
        end
      end
    end

    it "should cleanup message memory without max messages stored per channelXXX" do
      channel = 'ch_test_message_cleanup_without_max_messages_stored_per_chann'
      body = 'message to create a channel'

      nginx_run_server(config, :timeout => test_timeout) do |conf|
        stored_messages_setp_1 = 0
        published_messages_setp_1 = 0
        published_messages_setp_2 = 0

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

                  execute_changes_on_environment(conf) do
                    # connect a subscriber on new worker
                    sub_1 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => headers, :timeout => 60

                    EM.add_timer(50) do
                      fill_memory_timer = EventMachine::PeriodicTimer.new(0.001) do
                        publish_message_inline_with_callbacks(channel, headers, body, {
                          :error => Proc.new do |status2, content2|
                            fill_memory_timer.cancel
                            pub_2 = EventMachine::HttpRequest.new(nginx_address + '/channels-stats?id=' + channel.to_s).get :head => headers, :timeout => 60
                            pub_2.callback do
                              fail("Don't received the stats") if (pub_2.response_header.status != 200) || (pub_2.response_header.content_length == 0)
                              published_messages_setp_2 = JSON.parse(pub_2.response)["published_messages"].to_i
                              fail("Don't publish more messages") if published_messages_setp_1 == published_messages_setp_2

                              EM.add_timer(60) do
                                pub_3 = EventMachine::HttpRequest.new(nginx_address + '/channels-stats?id=' + channel.to_s).get :head => headers, :timeout => 60
                                pub_3.callback do
                                  fail("Don't received the stats") if (pub_3.response_header.status != 200) || (pub_3.response_header.content_length == 0)
                                  JSON.parse(pub_3.response)["stored_messages"].to_i.should eql(0)

                                  fill_memory_timer = EventMachine::PeriodicTimer.new(0.001) do
                                    publish_message_inline_with_callbacks(channel, headers, body, {
                                      :error => Proc.new do |status3, content3|
                                        pub_4 = EventMachine::HttpRequest.new(nginx_address + '/channels-stats?id=' + channel.to_s).get :head => headers, :timeout => 60
                                        pub_4.callback do
                                          fail("Don't received the stats") if (pub_4.response_header.status != 200) || (pub_4.response_header.content_length == 0)
                                          result = JSON.parse(pub_4.response)
                                          result["stored_messages"].to_i.should eql(stored_messages_setp_1)
                                          (result["published_messages"].to_i - published_messages_setp_2).should eql(published_messages_setp_1)
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
                    end
                  end
                end
              end
            })
          end
        end
      end
    end

    it "should cleanup memory used for create channels" do
      channel = 'ch_test_channel_cleanup_'
      body = 'message to create a channel'

      nginx_run_server(config.merge(:message_ttl => '2s'), :timeout => test_timeout) do |conf|
        channels_setp_1 = 0
        channels_setp_2 = 0
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
                  channels_setp_1 = JSON.parse(pub_2.response)["channels"].to_i
                  fail("Don't create any channel") if channels_setp_1 == 0

                  execute_changes_on_environment(conf) do
                    EM.add_timer(35) do
                      j = 0
                      fill_memory_timer = EventMachine::PeriodicTimer.new(0.001) do
                        publish_message_inline_with_callbacks(channel + j.to_s, headers, body, {
                          :error => Proc.new do |status2, content2|
                            fill_memory_timer.cancel
                            pub_2 = EventMachine::HttpRequest.new(nginx_address + '/channels-stats').get :head => headers, :timeout => 60
                            pub_2.callback do
                              fail("Don't received the stats") if (pub_2.response_header.status != 200) || (pub_2.response_header.content_length == 0)
                              fail("Don't create more channel") if published_messages_setp_1 == JSON.parse(pub_2.response)["published_messages"].to_i

                              EM.add_timer(40) do
                                pub_3 = EventMachine::HttpRequest.new(nginx_address + '/channels-stats').get :head => headers, :timeout => 60
                                pub_3.callback do
                                  fail("Don't received the stats") if (pub_3.response_header.status != 200) || (pub_3.response_header.content_length == 0)
                                  channels = JSON.parse(pub_3.response)["channels"].to_i

                                  channels.should eql(0)

                                  EM.add_timer(35) do
                                    i = 0
                                    fill_memory_timer = EventMachine::PeriodicTimer.new(0.001) do
                                      publish_message_inline_with_callbacks(channel + i.to_s, headers, body, {
                                        :error => Proc.new do |status3, content3|
                                          fill_memory_timer.cancel
                                          pub_4 = EventMachine::HttpRequest.new(nginx_address + '/channels-stats').get :head => headers, :timeout => 60
                                          pub_4.callback do
                                            fail("Don't received the stats") if (pub_4.response_header.status != 200) || (pub_4.response_header.content_length == 0)
                                            channels_setp_2 = JSON.parse(pub_4.response)["channels"].to_i

                                            channels_setp_2.should eql(channels_setp_1)
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
                        j += 1
                      end
                    end
                  end
                end
              end
            })
            i += 1
          end
        end
      end
    end

    it "should cleanup memory used for publish messages with store 'off' and with subscriber" do
      channel = 'ch_test_message_cleanup_with_store_off_with_subscriber'
      body = 'message to create a channel'

      nginx_run_server(config.merge(:store_messages => 'off'), :timeout => test_timeout) do |conf|
        published_messages_setp_1 = 0
        published_messages_setp_2 = 0

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

                  execute_changes_on_environment(conf) do
                    # connect a subscriber on new worker
                    sub_1 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => headers, :timeout => 60

                    EM.add_timer(35) do
                      fill_memory_timer = EventMachine::PeriodicTimer.new(0.001) do
                        publish_message_inline_with_callbacks(channel, headers, body, {
                          :error => Proc.new do |status2, content2|
                            fill_memory_timer.cancel
                            pub_2 = EventMachine::HttpRequest.new(nginx_address + '/channels-stats?id=' + channel.to_s).get :head => headers, :timeout => 60
                            pub_2.callback do
                              fail("Don't received the stats") if (pub_2.response_header.status != 200) || (pub_2.response_header.content_length == 0)
                              published_messages_setp_2 = JSON.parse(pub_2.response)["published_messages"].to_i
                              published_messages_setp_2.should_not eql(published_messages_setp_1)

                              EM.add_timer(35) do
                                pub_3 = EventMachine::HttpRequest.new(nginx_address + '/channels-stats?id=' + channel.to_s).get :head => headers, :timeout => 60
                                pub_3.callback do
                                  fail("Don't received the stats") if (pub_3.response_header.status != 200) || (pub_3.response_header.content_length == 0)
                                  JSON.parse(pub_3.response)["channels"].to_i.should eql(0)

                                  fill_memory_timer = EventMachine::PeriodicTimer.new(0.001) do
                                    publish_message_inline_with_callbacks(channel, headers, body, {
                                      :error => Proc.new do |status3, content3|
                                        pub_4 = EventMachine::HttpRequest.new(nginx_address + '/channels-stats?id=' + channel.to_s).get :head => headers, :timeout => 60
                                        pub_4.callback do
                                          fail("Don't received the stats") if (pub_4.response_header.status != 200) || (pub_4.response_header.content_length == 0)
                                          result = JSON.parse(pub_4.response)
                                          (result["published_messages"].to_i - published_messages_setp_2).should eql(published_messages_setp_1)
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
                    end
                  end
                end
              end
            })
          end
        end
      end
    end

    it "should cleanup memory used for publish messages with store 'off' and without subscriber" do
      channel = 'ch_test_message_cleanup_with_store_off_without_subscriber'
      body = 'message to create a channel'

      nginx_run_server(config.merge(:store_messages => 'off'), :timeout => test_timeout) do |conf|
        published_messages_setp_1 = 0
        published_messages_setp_2 = 0

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

                  execute_changes_on_environment(conf) do
                    EM.add_timer(35) do
                      j = 0
                      fill_memory_timer = EventMachine::PeriodicTimer.new(0.001) do
                        publish_message_inline_with_callbacks(channel + j.to_s, headers, body, {
                          :error => Proc.new do |status2, content2|
                            fill_memory_timer.cancel
                            pub_2 = EventMachine::HttpRequest.new(nginx_address + '/channels-stats').get :head => headers, :timeout => 60
                            pub_2.callback do
                              fail("Don't received the stats") if (pub_2.response_header.status != 200) || (pub_2.response_header.content_length == 0)
                              published_messages_setp_2 = JSON.parse(pub_2.response)["published_messages"].to_i
                              fail("Don't create more channel") if published_messages_setp_1 == published_messages_setp_2

                              EM.add_timer(35) do
                                pub_3 = EventMachine::HttpRequest.new(nginx_address + '/channels-stats').get :head => headers, :timeout => 60
                                pub_3.callback do
                                  fail("Don't received the stats") if (pub_3.response_header.status != 200) || (pub_3.response_header.content_length == 0)
                                  JSON.parse(pub_3.response)["channels"].to_i.should eql(0)

                                  EM.add_timer(35) do
                                    fill_memory_timer = EventMachine::PeriodicTimer.new(0.001) do
                                      publish_message_inline_with_callbacks(channel + i.to_s, headers, body, {
                                        :error => Proc.new do |status3, content3|
                                          pub_4 = EventMachine::HttpRequest.new(nginx_address + '/channels-stats').get :head => headers, :timeout => 60
                                          pub_4.callback do
                                            fail("Don't received the stats") if (pub_4.response_header.status != 200) || (pub_4.response_header.content_length == 0)
                                            result = JSON.parse(pub_4.response)
                                            (result["published_messages"].to_i - published_messages_setp_2).should eql(published_messages_setp_1)
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
                        j += 1
                      end
                    end
                  end
                end
              end
            })
            i += 1
          end
        end
      end
    end

    it "should cleanup memory used after delete created channels" do
      channel = 'ch_test_channel_cleanup_after_delete'
      body = 'message to create a channel'

      nginx_run_server(config.merge(:publisher_mode => 'admin'), :timeout => test_timeout) do |conf|
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

                  execute_changes_on_environment(conf) do
                    EM.add_timer(45) do
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
                              (result["published_messages"].to_i / 2).should eql(published_messages_setp_1)
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
            end
            i += 1
          end
        end
      end
    end

    it "should cleanup memory used after delete created channels with same id" do
      channel = 'ch_test_channel_cleanup_after_delete_same_id'
      body = 'message to create a channel'

      nginx_run_server(config.merge(:publisher_mode => 'admin'), :timeout => test_timeout) do |conf|
        published_messages_setp_1 = 0

        EventMachine.run do
          create_and_delete_channel_in_loop(channel, body, headers) do
            pub_2 = EventMachine::HttpRequest.new(nginx_address + '/channels-stats').get :head => headers, :timeout => 60
            pub_2.callback do
              fail("Don't received the stats") if (pub_2.response_header.status != 200) || (pub_2.response_header.content_length == 0)
              result = JSON.parse(pub_2.response)
              published_messages_setp_1 = result["published_messages"].to_i
              fail("Don't create any message") if published_messages_setp_1 == 0

              execute_changes_on_environment(conf) do
                EM.add_timer(40) do
                  create_and_delete_channel_in_loop(channel, body, headers) do
                    pub_2 = EventMachine::HttpRequest.new(nginx_address + '/channels-stats').get :head => headers, :timeout => 60
                    pub_2.callback do
                      fail("Don't received the stats") if (pub_2.response_header.status != 200) || (pub_2.response_header.content_length == 0)
                      result = JSON.parse(pub_2.response)
                      (result["published_messages"].to_i / 2).should eql(published_messages_setp_1)
                      EventMachine.stop
                    end
                  end
                end
              end
            end
          end
        end
      end
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

  let(:test_timeout) { 260 }

  let(:config) do
    {
      :master_process => 'on',
      :daemon => 'on',
      :shared_memory_cleanup_objects_ttl => '30s',
      :shared_memory_size => "129k",
      :message_ttl => '10s',
      :max_messages_stored_per_channel => nil
    }
  end

  let(:headers) do
    {'accept' => 'text/html'}
  end

  context "when nothing strange occur" do
    def execute_changes_on_environment(conf, &block)
      #nothing strange happens
      block.call
    end

    it_should_behave_like "executing on normal conditions"
  end

  context "when a worker is killed" do
    def execute_changes_on_environment(conf, &block)
      pub = EventMachine::HttpRequest.new(nginx_address + '/channels-stats').get :timeout => 30
      pub.callback do
        fail("Don't received the stats") if (pub.response_header.status != 200) || (pub.response_header.content_length == 0)
        resp_1 = JSON.parse(pub.response)
        resp_1["by_worker"].count.should eql(1)
        pid = resp_1["by_worker"][0]['pid'].to_i

        # send kill signal
        `kill -9 #{ pid } > /dev/null 2>&1`

        while `ps -p #{ pid } > /dev/null 2>&1; echo $?`.to_i == 0
          sleep(0.1)
        end

        block.call unless block.nil?
      end

    end

    it_should_behave_like "executing on normal conditions"
  end

  context "when the server is reloaded" do
    def execute_changes_on_environment(conf, &block)
      pub = EventMachine::HttpRequest.new(nginx_address + '/channels-stats').get :timeout => 30
      pub.callback do
        fail("Don't received the stats") if (pub.response_header.status != 200) || (pub.response_header.content_length == 0)
        resp_1 = JSON.parse(pub.response)
        resp_1["by_worker"].count.should eql(1)
        pid = resp_1["by_worker"][0]['pid'].to_i

        # send reload signal
        `#{ nginx_executable } -c #{ conf.configuration_filename } -s reload > /dev/null 2>&1`

        while `ps -p #{ pid } > /dev/null 2>&1; echo $?`.to_i == 0
          sleep(0.1)
        end

        block.call unless block.nil?
      end

    end

    it_should_behave_like "executing on normal conditions"
  end
end
