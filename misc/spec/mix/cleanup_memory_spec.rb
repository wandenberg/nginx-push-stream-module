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
              pub_2.should be_http_status(200).with_body
              result = JSON.parse(pub_2.response)
              stored_messages_setp_1 = result["stored_messages"].to_i
              published_messages_setp_1 = result["published_messages"].to_i
              messages_in_trash = result["messages_in_trash"].to_i

              stored_messages_setp_1.should eql(conf.max_messages_stored_per_channel)
              published_messages_setp_1.should be > (conf.max_messages_stored_per_channel)
              stored_messages_setp_1.should_not eql(0)
              published_messages_setp_1.should eql(stored_messages_setp_1 + messages_in_trash)

              wait_until_trash_is_empty(start, expected_time_for_clear, {:check_stored_messages => true}) do
                execute_changes_on_environment(conf) do
                  # connect a subscriber on new worker
                  sub_1 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => headers

                  publish_messages_until_fill_the_memory(channel, body) do |status2, content2|
                    start = Time.now
                    pub_2 = EventMachine::HttpRequest.new(nginx_address + '/channels-stats').get :head => headers
                    pub_2.callback do
                      pub_2.should be_http_status(200).with_body
                      published_messages_setp_2 = JSON.parse(pub_2.response)["published_messages"].to_i
                      fail("Don't publish more messages") if published_messages_setp_1 == published_messages_setp_2

                      wait_until_trash_is_empty(start, expected_time_for_clear, {:check_stored_messages => true}) do
                        publish_messages_until_fill_the_memory(channel, body) do |status3, content3|
                          pub_4 = EventMachine::HttpRequest.new(nginx_address + '/channels-stats').get :head => headers
                          pub_4.callback do
                            pub_4.should be_http_status(200).with_body
                            result = JSON.parse(pub_4.response)
                            result["stored_messages"].to_i.should eql(stored_messages_setp_1)
                            (result["published_messages"].to_i - published_messages_setp_2).should eql(published_messages_setp_1)

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
                pub_1.should be_http_status(200).with_body
                stored_messages_setp_1 = JSON.parse(pub_1.response)["stored_messages"].to_i
                stored_messages_setp_1.should eql(messages_to_publish)

                execute_changes_on_environment(conf) do
                  EM.add_timer(3) do # wait cleanup timer to be executed one time
                    pub_2 = EventMachine::HttpRequest.new(nginx_address + '/channels-stats?id=' + channel.to_s).get :head => headers
                    pub_2.callback do
                      pub_2.should be_http_status(200).with_body
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
              pub_2.should be_http_status(200).with_body
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
                      pub_2.should be_http_status(200).with_body
                      published_messages_setp_2 = JSON.parse(pub_2.response)["published_messages"].to_i
                      fail("Don't publish more messages") if published_messages_setp_1 == published_messages_setp_2

                      wait_until_trash_is_empty(start, expected_time_for_clear, {:check_stored_messages => true}) do
                        publish_messages_until_fill_the_memory(channel, body) do |status3, content3|
                          pub_4 = EventMachine::HttpRequest.new(nginx_address + '/channels-stats?id=' + channel.to_s).get :head => headers
                          pub_4.callback do
                            pub_4.should be_http_status(200).with_body
                            result = JSON.parse(pub_4.response)
                            result["stored_messages"].to_i.should eql(stored_messages_setp_1)
                            (result["published_messages"].to_i - published_messages_setp_2).should eql(published_messages_setp_1)
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
              pub_2.should be_http_status(200).with_body
              channels_setp_1 = JSON.parse(pub_2.response)["channels"].to_i
              fail("Don't create any channel") if channels_setp_1 == 0

              execute_changes_on_environment(conf) do
                wait_until_trash_is_empty(start, expected_time_for_clear, {:check_stored_messages => true, :check_channels => true}) do
                  publish_messages_until_fill_the_memory(channel, body) do |status2, content2|
                    start = Time.now
                    pub_2 = EventMachine::HttpRequest.new(nginx_address + '/channels-stats').get :head => headers
                    pub_2.callback do
                      pub_2.should be_http_status(200).with_body
                      fail("Don't create more channel") if published_messages_setp_1 == JSON.parse(pub_2.response)["published_messages"].to_i

                      wait_until_trash_is_empty(start, expected_time_for_clear, {:check_stored_messages => true, :check_channels => true}) do
                        publish_messages_until_fill_the_memory(channel, body) do |status3, content3|
                          pub_4 = EventMachine::HttpRequest.new(nginx_address + '/channels-stats').get :head => headers
                          pub_4.callback do
                            pub_4.should be_http_status(200).with_body
                            channels_setp_2 = JSON.parse(pub_4.response)["channels"].to_i

                            channels_setp_2.should eql(channels_setp_1)
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
              pub_2.should be_http_status(200).with_body
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
                      pub_2.should be_http_status(200).with_body
                      published_messages_setp_2 = JSON.parse(pub_2.response)["published_messages"].to_i
                      published_messages_setp_2.should_not eql(published_messages_setp_1)

                      wait_until_trash_is_empty(start, expected_time_for_clear, {:check_stored_messages => true}) do

                        publish_messages_until_fill_the_memory(channel, body) do |status3, content3|
                          pub_4 = EventMachine::HttpRequest.new(nginx_address + '/channels-stats?id=' + channel.to_s).get :head => headers
                          pub_4.callback do
                            pub_4.should be_http_status(200).with_body
                            result = JSON.parse(pub_4.response)
                            (result["published_messages"].to_i - published_messages_setp_2).should eql(published_messages_setp_1)
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
              pub_2.should be_http_status(200).with_body
              result = JSON.parse(pub_2.response)
              published_messages_setp_1 = result["published_messages"].to_i

              execute_changes_on_environment(conf) do
                wait_until_trash_is_empty(start, expected_time_for_clear, {:check_stored_messages => true, :check_channels => true}) do
                  publish_messages_until_fill_the_memory(channel, body) do |status2, content2|
                    start = Time.now
                    pub_2 = EventMachine::HttpRequest.new(nginx_address + '/channels-stats').get :head => headers
                    pub_2.callback do
                      pub_2.should be_http_status(200).with_body
                      published_messages_setp_2 = JSON.parse(pub_2.response)["published_messages"].to_i
                      fail("Don't create more channel") if published_messages_setp_1 == published_messages_setp_2

                      wait_until_trash_is_empty(start, expected_time_for_clear, {:check_stored_messages => true, :check_channels => true}) do
                        publish_messages_until_fill_the_memory(channel, body) do |status3, content3|
                          pub_4 = EventMachine::HttpRequest.new(nginx_address + '/channels-stats').get :head => headers
                          pub_4.callback do
                            pub_4.should be_http_status(200).with_body
                            result = JSON.parse(pub_4.response)
                            (result["published_messages"].to_i - published_messages_setp_2).should eql(published_messages_setp_1)
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
          i = 0
          fill_memory_timer = EventMachine::PeriodicTimer.new(0.001) do
            pub_1 = EventMachine::HttpRequest.new(nginx_address + '/pub?id=' + channel.to_s + i.to_s).post :body => body, :head => headers
            pub_1.callback do
              if pub_1.response_header.status == 500
                fill_memory_timer.cancel
                start = Time.now
                i.times do |j|
                  pub = EventMachine::HttpRequest.new(nginx_address + '/pub?id=' + channel.to_s + j.to_s).delete :head => headers
                end
                pub_2 = EventMachine::HttpRequest.new(nginx_address + '/channels-stats').get :head => headers
                pub_2.callback do
                  pub_2.should be_http_status(200).with_body
                  result = JSON.parse(pub_2.response)
                  published_messages_setp_1 = result["published_messages"].to_i
                  fail("Don't create any message") if published_messages_setp_1 == 0

                  execute_changes_on_environment(conf) do
                    wait_until_trash_is_empty(start, expected_time_for_clear, {:check_stored_messages => true, :check_channels => true}) do
                      i = 0
                      fill_memory_timer = EventMachine::PeriodicTimer.new(0.001) do
                        pub_1 = EventMachine::HttpRequest.new(nginx_address + '/pub?id=' + channel.to_s + i.to_s).post :body => body, :head => headers
                        pub_1.callback do
                          if pub_1.response_header.status == 500
                            fill_memory_timer.cancel
                            pub_2 = EventMachine::HttpRequest.new(nginx_address + '/channels-stats').get :head => headers
                            pub_2.callback do
                              pub_2.should be_http_status(200).with_body
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

    it "should cleanup memory used after delete created channels with same id", :cleanup => true do
      channel = 'ch_test_channel_cleanup_after_delete_same_id'
      body = 'message to create a channel'
      expected_time_for_clear = 15

      nginx_run_server(config.merge(:publisher_mode => 'admin'), :timeout => test_timeout) do |conf|
        published_messages_setp_1 = 0

        EventMachine.run do
          create_and_delete_channel_in_loop(channel, body, headers) do
            pub_2 = EventMachine::HttpRequest.new(nginx_address + '/channels-stats').get :head => headers
            pub_2.callback do
              pub_2.should be_http_status(200).with_body
              result = JSON.parse(pub_2.response)
              published_messages_setp_1 = result["published_messages"].to_i
              fail("Don't create any message") if published_messages_setp_1 == 0

              execute_changes_on_environment(conf) do
                wait_until_trash_is_empty(Time.now, expected_time_for_clear, {:check_stored_messages => true, :check_channels => true}) do
                  create_and_delete_channel_in_loop(channel, body, headers) do
                    pub_2 = EventMachine::HttpRequest.new(nginx_address + '/channels-stats').get :head => headers
                    pub_2.callback do
                      pub_2.should be_http_status(200).with_body
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
    pub_1 = EventMachine::HttpRequest.new(nginx_address + '/pub?id=' + channel.to_s).post :body => body, :head => headers
    pub_1.callback do
      pub = EventMachine::HttpRequest.new(nginx_address + '/pub?id=' + channel.to_s).delete :head => headers
      pub.callback do
        if pub_1.response_header.status == 200
          block.call((pub.response_header.status == 200) ? :success : :error)
        else
          block.call(:error)
        end
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

  def wait_until_trash_is_empty(start_time, expected_time_for_clear, options={}, &block)
    check_timer = EventMachine::PeriodicTimer.new(1) do
      stats = EventMachine::HttpRequest.new("#{nginx_address}/channels-stats").get :head => headers
      stats.callback do
        stats.should be_http_status(200).with_body
        result = JSON.parse(stats.response)
        if (result["messages_in_trash"].to_i == 0) && (result["channels_in_trash"].to_i == 0)
          if (!options[:check_stored_messages] || (result["stored_messages"].to_i == 0)) && (!options[:check_channels] || (result["channels"].to_i == 0))
            check_timer.cancel
            stop = Time.now
            (stop - start_time).should be_within(5).of(expected_time_for_clear)

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
            pub_1.should be_http_status(200).with_body

            start = Time.now
            timer = EventMachine::PeriodicTimer.new(1) do
              stats = EventMachine::HttpRequest.new(nginx_address + '/channels-stats').get :head => headers
              stats.callback do
                stats.should be_http_status(200).with_body
                response = JSON.parse(stats.response)

                if response["channels"].to_i != 1
                  stop = Time.now
                  time_diff_sec(start, stop).should be_within(5).of(30)
                  response["channels_in_trash"].to_i.should eql(1)
                  response["channels"].to_i.should eql(0)
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
            pub_1.should be_http_status(200).with_body

            start = Time.now
            timer = EventMachine::PeriodicTimer.new(1) do
              stats = EventMachine::HttpRequest.new(nginx_address + '/channels-stats').get :head => headers
              stats.callback do
                stats.should be_http_status(200).with_body
                response = JSON.parse(stats.response)

                if response["channels"].to_i != 1
                  stop = Time.now
                  time_diff_sec(start, stop).should be_within(3).of(5)
                  response["channels_in_trash"].to_i.should eql(1)
                  response["channels"].to_i.should eql(0)
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
        pub.should be_http_status(200).with_body
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
        pub.should be_http_status(200).with_body
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
