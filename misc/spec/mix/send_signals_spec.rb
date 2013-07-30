require 'spec_helper'

describe "Send Signals" do
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

  let(:config) do
    {
      :master_process => 'on',
      :daemon => 'on',
      :header_template => 'HEADER',
      :message_ttl => '60s',
      :subscriber_connection_ttl => '65s'
    }
  end

  it "should reload normaly when receives HUP signal" do
    channel = 'ch_test_send_hup_signal'
    body = 'body'
    response = response2 = ''
    pid = pid2 = 0

    nginx_run_server(config, :timeout => 60) do |conf|
      EventMachine.run do
        # create subscriber
        sub_1 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => headers
        sub_1.stream do |chunk|
          response = response + chunk
          if response.strip == conf.header_template
            # check statistics
            pub_1 = EventMachine::HttpRequest.new(nginx_address + '/channels-stats').get :head => headers
            pub_1.callback do
              pub_1.should be_http_status(200).with_body
              resp_1 = JSON.parse(pub_1.response)
              resp_1.has_key?("channels").should be_true
              resp_1["channels"].to_i.should eql(1)
              resp_1["by_worker"].count.should eql(1)
              pid = resp_1["by_worker"][0]['pid'].to_i

              # send reload signal
              `#{ nginx_executable } -c #{ conf.configuration_filename } -s reload > /dev/null 2>&1`
            end
          end
        end

        conectted_after_reloaded = false
        i = 0
        # check if first worker die
        EM.add_periodic_timer(0.5) do

          # check statistics again
          pub_4 = EventMachine::HttpRequest.new(nginx_address + '/channels-stats').get :head => headers
          pub_4.callback do
            resp_3 = JSON.parse(pub_4.response)
            resp_3.has_key?("by_worker").should be_true

            if resp_3["by_worker"].count == 2 && !conectted_after_reloaded
              conectted_after_reloaded = true

              # publish a message
              pub_2 = EventMachine::HttpRequest.new(nginx_address + '/pub?id=' + channel.to_s).post :head => headers, :body => body
              pub_2.callback do
                # add new subscriber
                sub_2 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s + '.b1').get :head => headers
                sub_2.stream do |chunk|
                  response2 = response2 + chunk
                  if response2.strip == conf.header_template
                    # check statistics again
                    pub_3 = EventMachine::HttpRequest.new(nginx_address + '/channels-stats').get :head => headers
                    pub_3.callback do

                      resp_2 = JSON.parse(pub_3.response)
                      resp_2.has_key?("channels").should be_true
                      resp_2["channels"].to_i.should eql(1)
                      resp_2["published_messages"].to_i.should eql(1)
                      resp_2["subscribers"].to_i.should eql(2)
                      resp_2["by_worker"].count.should eql(2)
                    end
                  end
                end
              end
            end

            if resp_3["by_worker"].count == 1 && conectted_after_reloaded
              resp_3["channels"].to_i.should eql(1)
              resp_3["published_messages"].to_i.should eql(1)
              resp_3["subscribers"].to_i.should eql(1)
              resp_3["by_worker"].count.should eql(1)
              pid2 = resp_3["by_worker"][0]['pid'].to_i

              pid.should_not eql(pid2)
              EventMachine.stop
            end

            i = i + 1
            if i == 120
              fail("Worker didn't die in 60 seconds")
              EventMachine.stop
            end
          end
        end
      end
    end
  end

  it "should ignore changes on shared memory size when doing a reload" do
    channel = 'ch_test_reload_with_different_shared_memory_size'
    body = 'body'
    response = response2 = ''
    pid = pid2 = 0

    nginx_run_server(config, :timeout => 10) do |conf|
      EventMachine.run do
        publish_message_inline(channel, {}, body)
        # check statistics
        pub_1 = EventMachine::HttpRequest.new(nginx_address + '/channels-stats').get :head => headers
        pub_1.callback do
          pub_1.should be_http_status(200).with_body
          resp_1 = JSON.parse(pub_1.response)
          resp_1.has_key?("channels").should be_true
          resp_1["channels"].to_i.should eql(1)
          resp_1["published_messages"].to_i.should eql(1)

          conf.configuration[:shared_memory_size] = '20m'
          conf.create_configuration_file

          # send reload signal
          `#{ nginx_executable } -c #{ conf.configuration_filename } -s reload > /dev/null 2>&1`

          sleep 5

          pub_2 = EventMachine::HttpRequest.new(nginx_address + '/channels-stats').get :head => headers
          pub_2.callback do
            pub_2.should be_http_status(200).with_body
            resp_2 = JSON.parse(pub_2.response)
            resp_2.has_key?("channels").should be_true
            resp_2["channels"].to_i.should eql(1)
            resp_2["published_messages"].to_i.should eql(1)

            error_log = File.read(conf.error_log)
            error_log.should include("Cannot change memory area size without restart, ignoring change")

            EventMachine.stop
          end
        end
      end
    end
  end
end
