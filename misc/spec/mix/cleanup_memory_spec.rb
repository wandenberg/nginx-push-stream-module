require 'spec_helper'

describe "Cleanup Memory" do
  old_cld_trap = nil
  before do
    old_cld_trap = Signal.trap("CLD", "IGNORE")
  end

  after do
    Signal.trap("CLD", old_cld_trap)
  end

  shared_examples_for "executing on normal conditions" do

    it "should cleanup memory used for published message", :cleanup => true do
      channel = 'ch_test_message_cleanup'
      body = 'message to create a channel'
      expected_time_for_clear = 25

      nginx_run_server(config.merge(:max_messages_stored_per_channel => 100), :timeout => test_timeout) do |conf|
        stored_messages_setp_1 = 0
        published_messages_setp_1 = 0
        published_messages_setp_2 = 0

        EventMachine.run do
          # ensure channel will not be cleaned up
          sub_1 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => headers

          publish_messages_until_fill_the_memory(channel, body) do |status, content|

            start = Time.now
            pub_2 = EventMachine::HttpRequest.new(nginx_address + '/channels-stats').get :head => headers
            pub_2.callback do
              expect(pub_2).to be_http_status(200).with_body
              result = JSON.parse(pub_2.response)
              stored_messages_setp_1 = result["stored_messages"].to_i
              published_messages_setp_1 = result["published_messages"].to_i
              messages_in_trash = result["messages_in_trash"].to_i

              expect(stored_messages_setp_1).to eql(conf.max_messages_stored_per_channel)
              expect(published_messages_setp_1).to be > (conf.max_messages_stored_per_channel)
              expect(stored_messages_setp_1).not_to eql(0)
              expect(published_messages_setp_1).to eql(stored_messages_setp_1 + messages_in_trash)

              wait_until_trash_is_empty(start, expected_time_for_clear, {:check_stored_messages => true}) do
                execute_changes_on_environment(conf) do
                  # connect a subscriber on new worker
                  sub_1 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => headers

                  publish_messages_until_fill_the_memory(channel, body) do |status2, content2|
                    start = Time.now
                    pub_2 = EventMachine::HttpRequest.new(nginx_address + '/channels-stats').get :head => headers
                    pub_2.callback do
                      expect(pub_2).to be_http_status(200).with_body
                      published_messages_setp_2 = JSON.parse(pub_2.response)["published_messages"].to_i
                      fail("Don't publish more messages") if published_messages_setp_1 == published_messages_setp_2

                      wait_until_trash_is_empty(start, expected_time_for_clear, {:check_stored_messages => true}) do
                        publish_messages_until_fill_the_memory(channel, body) do |status3, content3|
                          pub_4 = EventMachine::HttpRequest.new(nginx_address + '/channels-stats').get :head => headers
                          pub_4.callback do
                            expect(pub_4).to be_http_status(200).with_body
                            result = JSON.parse(pub_4.response)
                            expect(result["stored_messages"].to_i).to eql(stored_messages_setp_1)
                            expect(result["published_messages"].to_i - published_messages_setp_2).to eql(published_messages_setp_1)

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
      end
    end

    it "should discard old messages", :cleanup => true do
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
              pub_1 = EventMachine::HttpRequest.new(nginx_address + '/channels-stats?id=' + channel.to_s).get :head => headers
              pub_1.callback do
                fill_memory_timer.cancel
                expect(pub_1).to be_http_status(200).with_body
                stored_messages_setp_1 = JSON.parse(pub_1.response)["stored_messages"].to_i
                expect(stored_messages_setp_1).to eql(messages_to_publish)

                execute_changes_on_environment(conf) do
                  EM.add_timer(3) do # wait cleanup timer to be executed one time
                    pub_2 = EventMachine::HttpRequest.new(nginx_address + '/channels-stats?id=' + channel.to_s).get :head => headers
                    pub_2.callback do
                      expect(pub_2).to be_http_status(200).with_body
                      stored_messages_setp_2 = JSON.parse(pub_2.response)["stored_messages"].to_i
                      expect(stored_messages_setp_2).to be <= stored_messages_setp_1
                      expect(stored_messages_setp_2).to be > 0

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

    it "should cleanup message memory without max messages stored per channel", :cleanup => true do
      channel = 'ch_test_message_cleanup_without_max_messages_stored_per_channel'
      body = 'message to create a channel'
      expected_time_for_clear = 25

      nginx_run_server(config, :timeout => test_timeout) do |conf|
        stored_messages_setp_1 = 0
        published_messages_setp_1 = 0
        published_messages_setp_2 = 0

        EventMachine.run do
          # ensure channel will not be cleaned up
          sub_1 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => headers

          publish_messages_until_fill_the_memory(channel, body) do |status, content|
            start = Time.now
            pub_2 = EventMachine::HttpRequest.new(nginx_address + '/channels-stats').get :head => headers
            pub_2.callback do
              expect(pub_2).to be_http_status(200).with_body
              result = JSON.parse(pub_2.response)
              stored_messages_setp_1 = result["stored_messages"].to_i
              published_messages_setp_1 = result["published_messages"].to_i
              fail("Limited the number of stored messages") if stored_messages_setp_1 <= 100
              fail("Don't create any message") if stored_messages_setp_1 == 0

              execute_changes_on_environment(conf) do
                # connect a subscriber on new worker
                sub_1 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => headers

                wait_until_trash_is_empty(start, expected_time_for_clear, {:check_stored_messages => true}) do
                  publish_messages_until_fill_the_memory(channel, body) do |status2, content2|
                    start = Time.now
                    pub_2 = EventMachine::HttpRequest.new(nginx_address + '/channels-stats?id=' + channel.to_s).get :head => headers
                    pub_2.callback do
                      expect(pub_2).to be_http_status(200).with_body
                      published_messages_setp_2 = JSON.parse(pub_2.response)["published_messages"].to_i
                      fail("Don't publish more messages") if published_messages_setp_1 == published_messages_setp_2

                      wait_until_trash_is_empty(start, expected_time_for_clear, {:check_stored_messages => true}) do
                        publish_messages_until_fill_the_memory(channel, body) do |status3, content3|
                          pub_4 = EventMachine::HttpRequest.new(nginx_address + '/channels-stats?id=' + channel.to_s).get :head => headers
                          pub_4.callback do
                            expect(pub_4).to be_http_status(200).with_body
                            result = JSON.parse(pub_4.response)
                            expect(result["stored_messages"].to_i).to eql(stored_messages_setp_1)
                            expect(result["published_messages"].to_i - published_messages_setp_2).to eql(published_messages_setp_1)
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
      end
    end

    it "should cleanup memory used for create channels", :cleanup => true do
      channel = 'ch_test_channel_cleanup_%d'
      body = 'message to create a channel'

      nginx_run_server(config.merge(:message_ttl => '2s'), :timeout => test_timeout) do |conf|
        channels_setp_1 = 0
        channels_setp_2 = 0
        published_messages_setp_1 = 0
        expected_time_for_clear = 45

        EventMachine.run do
          publish_messages_until_fill_the_memory(channel, body) do |status, content|
            start = Time.now
            pub_2 = EventMachine::HttpRequest.new(nginx_address + '/channels-stats').get :head => headers
            pub_2.callback do
              expect(pub_2).to be_http_status(200).with_body
              channels_setp_1 = JSON.parse(pub_2.response)["channels"].to_i
              fail("Don't create any channel") if channels_setp_1 == 0

              execute_changes_on_environment(conf) do
                wait_until_trash_is_empty(start, expected_time_for_clear, {:check_stored_messages => true, :check_channels => true}) do
                  publish_messages_until_fill_the_memory(channel, body) do |status2, content2|
                    start = Time.now
                    pub_2 = EventMachine::HttpRequest.new(nginx_address + '/channels-stats').get :head => headers
                    pub_2.callback do
                      expect(pub_2).to be_http_status(200).with_body
                      fail("Don't create more channel") if published_messages_setp_1 == JSON.parse(pub_2.response)["published_messages"].to_i

                      wait_until_trash_is_empty(start, expected_time_for_clear, {:check_stored_messages => true, :check_channels => true}) do
                        publish_messages_until_fill_the_memory(channel, body) do |status3, content3|
                          pub_4 = EventMachine::HttpRequest.new(nginx_address + '/channels-stats').get :head => headers
                          pub_4.callback do
                            expect(pub_4).to be_http_status(200).with_body
                            channels_setp_2 = JSON.parse(pub_4.response)["channels"].to_i

                            expect(channels_setp_2).to eql(channels_setp_1)
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
      end
    end

    it "should cleanup memory used for publish messages with store 'off' and with subscriber", :cleanup => true do
      channel = 'ch_test_message_cleanup_with_store_off_with_subscriber'
      body = 'message to create a channel'
      expected_time_for_clear = 15

      nginx_run_server(config.merge(:store_messages => 'off'), :timeout => test_timeout) do |conf|
        published_messages_setp_1 = 0
        published_messages_setp_2 = 0

        EventMachine.run do
          # ensure channel will not be cleaned up
          sub_1 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => headers

          publish_messages_until_fill_the_memory(channel, body) do |status, content|
            start = Time.now
            pub_2 = EventMachine::HttpRequest.new(nginx_address + '/channels-stats?id=' + channel.to_s).get :head => headers
            pub_2.callback do
              expect(pub_2).to be_http_status(200).with_body
              result = JSON.parse(pub_2.response)
              published_messages_setp_1 = result["published_messages"].to_i

              execute_changes_on_environment(conf) do
                # connect a subscriber on new worker
                sub_1 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => headers

                wait_until_trash_is_empty(start, expected_time_for_clear, {:check_stored_messages => true}) do

                  publish_messages_until_fill_the_memory(channel, body) do |status2, content2|
                    start = Time.now
                    pub_2 = EventMachine::HttpRequest.new(nginx_address + '/channels-stats?id=' + channel.to_s).get :head => headers
                    pub_2.callback do
                      expect(pub_2).to be_http_status(200).with_body
                      published_messages_setp_2 = JSON.parse(pub_2.response)["published_messages"].to_i
                      expect(published_messages_setp_2).not_to eql(published_messages_setp_1)

                      wait_until_trash_is_empty(start, expected_time_for_clear, {:check_stored_messages => true}) do

                        publish_messages_until_fill_the_memory(channel, body) do |status3, content3|
                          pub_4 = EventMachine::HttpRequest.new(nginx_address + '/channels-stats?id=' + channel.to_s).get :head => headers
                          pub_4.callback do
                            expect(pub_4).to be_http_status(200).with_body
                            result = JSON.parse(pub_4.response)
                            expect(result["published_messages"].to_i - published_messages_setp_2).to eql(published_messages_setp_1)
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
      end
    end

    it "should cleanup memory used for publish messages with store 'off' and without subscriber", :cleanup => true do
      channel = 'ch_test_message_cleanup_with_store_off_without_subscriber %d'
      body = 'message to create a channel'
      expected_time_for_clear = 45

      nginx_run_server(config.merge(:store_messages => 'off'), :timeout => test_timeout) do |conf|
        published_messages_setp_1 = 0
        published_messages_setp_2 = 0

        EventMachine.run do
          publish_messages_until_fill_the_memory(channel, body) do |status, content|

            start = Time.now
            pub_2 = EventMachine::HttpRequest.new(nginx_address + '/channels-stats').get :head => headers
            pub_2.callback do
              expect(pub_2).to be_http_status(200).with_body
              result = JSON.parse(pub_2.response)
              published_messages_setp_1 = result["published_messages"].to_i

              execute_changes_on_environment(conf) do
                wait_until_trash_is_empty(start, expected_time_for_clear, {:check_stored_messages => true, :check_channels => true}) do
                  publish_messages_until_fill_the_memory(channel, body) do |status2, content2|
                    start = Time.now
                    pub_2 = EventMachine::HttpRequest.new(nginx_address + '/channels-stats').get :head => headers
                    pub_2.callback do
                      expect(pub_2).to be_http_status(200).with_body
                      published_messages_setp_2 = JSON.parse(pub_2.response)["published_messages"].to_i
                      fail("Don't create more channel") if published_messages_setp_1 == published_messages_setp_2

                      wait_until_trash_is_empty(start, expected_time_for_clear, {:check_stored_messages => true, :check_channels => true}) do
                        publish_messages_until_fill_the_memory(channel, body) do |status3, content3|
                          pub_4 = EventMachine::HttpRequest.new(nginx_address + '/channels-stats').get :head => headers
                          pub_4.callback do
                            expect(pub_4).to be_http_status(200).with_body
                            result = JSON.parse(pub_4.response)
                            expect(result["published_messages"].to_i - published_messages_setp_2).to eql(published_messages_setp_1)
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
      end
    end

    it "should cleanup memory used after delete created channels", :cleanup => true do
      channel = 'ch_test_channel_cleanup_after_delete'
      body = 'message to create a channel'
      expected_time_for_clear = 15

      nginx_run_server(config.merge(:publisher_mode => 'admin'), :timeout => test_timeout) do |conf|
        published_messages_setp_1 = 0

        EventMachine.run do
          pub_placeholder = EventMachine::HttpRequest.new(nginx_address + '/pub?id=pub_placeholder').post :body => body * 200, :head => headers
          pub_placeholder.callback do

            EM.add_timer(3) do
              i = 0
              fill_memory_timer = EventMachine::PeriodicTimer.new(0.001) do
                pub_1 = EventMachine::HttpRequest.new(nginx_address + '/pub?id=' + channel.to_s + i.to_s).post :body => body, :head => headers
                pub_1.callback do
                  if pub_1.response_header.status == 500
                    fill_memory_timer.cancel
                    pub_2 = EventMachine::HttpRequest.new(nginx_address + '/channels-stats?id=ALL').get :head => headers
                    pub_2.callback do
                      delete_channels(JSON.parse(pub_2.response)["infos"].map {|info| info["channel"] }, headers) do
                        start = Time.now

                        pub_2 = EventMachine::HttpRequest.new(nginx_address + '/channels-stats').get :head => headers
                        pub_2.callback do
                          expect(pub_2).to be_http_status(200).with_body
                          result = JSON.parse(pub_2.response)
                          published_messages_setp_1 = result["published_messages"].to_i
                          fail("Don't create any message") if published_messages_setp_1 == 0
                          fail("Some channel left") if result["channels"].to_i != 0
                          fail("Don't deleted any channel") if result["channels_in_delete"].to_i == 0
                          fail("Already sent channels to trash") if result["channels_in_trash"].to_i != 0

                          execute_changes_on_environment(conf) do
                            wait_until_trash_is_empty(start, expected_time_for_clear, {:check_stored_messages => true, :check_channels => true}) do
                              i = 0
                              pub_placeholder = EventMachine::HttpRequest.new(nginx_address + '/pub?id=pub_placeholder').post :body => body * 200, :head => headers

                              fill_memory_timer = EventMachine::PeriodicTimer.new(0.001) do
                                pub_1 = EventMachine::HttpRequest.new(nginx_address + '/pub?id=' + channel.to_s + i.to_s).post :body => body, :head => headers
                                pub_1.callback do
                                  if pub_1.response_header.status == 500
                                    fill_memory_timer.cancel
                                    pub_2 = EventMachine::HttpRequest.new(nginx_address + '/channels-stats').get :head => headers
                                    pub_2.callback do
                                      expect(pub_2).to be_http_status(200).with_body
                                      result = JSON.parse(pub_2.response)
                                      expect(result["published_messages"].to_i / 2).to eql(published_messages_setp_1)
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
                  end
                end
                i += 1
              end
            end
          end
        end
      end
    end

    it "should cleanup memory used after delete created channels with same id", :cleanup => true do
      channel = 'ch_test_channel_cleanup_after_delete_same_id'
      body = 'message to create a channel'
      expected_time_for_clear = 15

      nginx_run_server(config.merge(:publisher_mode => 'admin'), :timeout => test_timeout) do |conf|
        published_messages_setp_1 = 0

        EventMachine.run do
          create_and_delete_channel(channel, body, headers) do
            pub_2 = EventMachine::HttpRequest.new(nginx_address + '/channels-stats').get :head => headers
            pub_2.callback do
              expect(pub_2).to be_http_status(200).with_body
              result = JSON.parse(pub_2.response)
              published_messages_setp_1 = result["published_messages"].to_i
              fail("Don't create any message") if published_messages_setp_1 == 0
              fail("Don't deleted any channel") if result["channels_in_delete"].to_i == 0
              fail("Already sent channels to trash") if result["channels_in_trash"].to_i != 0

              execute_changes_on_environment(conf) do
                wait_until_trash_is_empty(Time.now, expected_time_for_clear, {:check_stored_messages => true, :check_channels => true}) do
                  create_and_delete_channel(channel, body, headers) do
                    pub_2 = EventMachine::HttpRequest.new(nginx_address + '/channels-stats').get :head => headers
                    pub_2.callback do
                      expect(pub_2).to be_http_status(200).with_body
                      result = JSON.parse(pub_2.response)
                      expect(result["published_messages"].to_i / 2).to eql(published_messages_setp_1)
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
    pub_1 = EventMachine::HttpRequest.new(nginx_address + '/pub?id=' + channel.to_s).post :body => body, :head => headers
    pub_1.callback do
      pub = EventMachine::HttpRequest.new(nginx_address + '/pub?id=' + channel.to_s).delete :head => headers
      pub.callback do
        if pub_1.response_header.status == 200 && pub.response_header.status == 200
          EM.add_timer(0.001) { create_and_delete_channel(channel, body, headers, &block) }
        else
          delete_channels([channel.to_s], headers) do
            block.call unless block.nil?
          end
        end
      end
    end
  end

  def delete_channels(channels, headers, &block)
    if channels.length == 0
      block.call
      return
    end

    pub = EventMachine::HttpRequest.new(nginx_address + '/pub?id=' + channels[0]).delete :head => headers
    pub.callback do
      if pub.response_header.status == 500
        EM.add_timer(1) { delete_channels(channels, headers, &block) }
      else
        channels.shift
        EM.add_timer(0.005) { delete_channels(channels, headers, &block) }
      end
    end
    pub.errback do
      EM.add_timer(1) { delete_channels(channels, headers, &block) }
    end
  end

  def wait_until_trash_is_empty(start_time, expected_time_for_clear, options={}, &block)
    check_timer = EventMachine::PeriodicTimer.new(1) do
      stats = EventMachine::HttpRequest.new("#{nginx_address}/channels-stats").get :head => headers
      stats.callback do
        expect(stats).to be_http_status(200).with_body
        result = JSON.parse(stats.response)
        if (result["messages_in_trash"].to_i == 0) && (result["channels_in_trash"].to_i == 0)
          if (!options[:check_stored_messages] || (result["stored_messages"].to_i == 0)) && (!options[:check_channels] || ((result["channels"].to_i == 0) && (result["channels_in_delete"].to_i == 0)))
            check_timer.cancel
            stop = Time.now
            expect(stop - start_time).to be_within(5).of(expected_time_for_clear)

            block.call
          end
        end
      end
    end
  end

  let(:test_timeout) { 260 }

  let(:config) do
    {
      :master_process => 'on',
      :daemon => 'on',
      :shared_memory_size => "129k",
      :message_ttl => '10s',
      :max_messages_stored_per_channel => nil,
      :keepalive_requests => 200
    }
  end

  let(:headers) do
    {'accept' => 'text/html'}
  end

  context "when moving inactive channels to trash" do
    it "should wait 30s by default" do
      channel = 'ch_move_inactive_channels'
      body = 'body'

      nginx_run_server(config.merge(:store_messages => "off"), :timeout => 40) do |conf|
        EventMachine.run do
          pub_1 = EventMachine::HttpRequest.new(nginx_address + '/pub?id=' + channel.to_s).post :head => headers, :body => body
          pub_1.callback do
            expect(pub_1).to be_http_status(200).with_body

            start = Time.now
            timer = EventMachine::PeriodicTimer.new(1) do
              stats = EventMachine::HttpRequest.new(nginx_address + '/channels-stats').get :head => headers
              stats.callback do
                expect(stats).to be_http_status(200).with_body
                response = JSON.parse(stats.response)

                if response["channels"].to_i != 1
                  stop = Time.now
                  expect(time_diff_sec(start, stop)).to be_within(5).of(30)
                  expect(response["channels_in_trash"].to_i).to eql(1)
                  expect(response["channels"].to_i).to eql(0)
                  EventMachine.stop
                end
              end
            end
          end
        end
      end
    end

    it "should be possible change the default value" do
      channel = 'ch_move_inactive_channels_with_custom_value'
      body = 'body'

      nginx_run_server(config.merge(:store_messages => "off", :channel_inactivity_time => "5s"), :timeout => 10) do |conf|
        EventMachine.run do
          pub_1 = EventMachine::HttpRequest.new(nginx_address + '/pub?id=' + channel.to_s).post :head => headers, :body => body
          pub_1.callback do
            expect(pub_1).to be_http_status(200).with_body

            start = Time.now
            timer = EventMachine::PeriodicTimer.new(1) do
              stats = EventMachine::HttpRequest.new(nginx_address + '/channels-stats').get :head => headers
              stats.callback do
                expect(stats).to be_http_status(200).with_body
                response = JSON.parse(stats.response)

                if response["channels"].to_i != 1
                  stop = Time.now
                  expect(time_diff_sec(start, stop)).to be_within(3).of(5)
                  expect(response["channels_in_trash"].to_i).to eql(1)
                  expect(response["channels"].to_i).to eql(0)
                  EventMachine.stop
                end
              end
            end
          end
        end
      end
    end
    #after the last published message
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
        expect(pub).to be_http_status(200).with_body
        resp_1 = JSON.parse(pub.response)
        expect(resp_1["by_worker"].count).to eql(conf.workers)
        pids = resp_1["by_worker"].map{ |info| info['pid'].to_i }

        # send kill signal
        pids.each{ |pid| `kill -9 #{ pid } > /dev/null 2>&1` }

        while pids.all?{ |pid| `ps -p #{ pid } > /dev/null 2>&1; echo $?`.to_i == 0 }
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
        expect(pub).to be_http_status(200).with_body
        resp_1 = JSON.parse(pub.response)
        expect(resp_1["by_worker"].count).to eql(conf.workers)
        pids = resp_1["by_worker"].map{ |info| info['pid'].to_i }

        # send reload signal
        pids.each{ |pid| `#{ nginx_executable } -c #{ conf.configuration_filename } -s reload > /dev/null 2>&1` }

        while pids.all?{ |pid| `ps -p #{ pid } > /dev/null 2>&1; echo $?`.to_i == 0 }
          sleep(0.1)
        end

        block.call unless block.nil?
      end

    end

    it_should_behave_like "executing on normal conditions"
  end
end
