require 'spec_helper'

describe "Send Signals" do
  old_cld_trap = nil
  before do
    old_cld_trap = Signal.trap("CLD", "IGNORE")
  end

  after do
    Signal.trap("CLD", old_cld_trap)
  end

  let(:config) do
    {
      :master_process => 'on',
      :daemon => 'on',
      :workers => 1,
      :header_template => 'HEADER',
      :footer_template => 'FOOTER',
      :message_ttl => '60s',
      :subscriber_connection_ttl => '65s'
    }
  end

  it "should disconnect subscribers when receives TERM signal" do
    channel = 'ch_test_send_term_signal'
    body = 'body'
    response = ''

    nginx_run_server(config, :timeout => 5) do |conf|
      EventMachine.run do
        sub_1 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => headers.merge('X-Nginx-PushStream-Mode' => 'long-polling')
        sub_1.callback do
          expect(sub_1).to be_http_status(304).without_body
          expect(Time.parse(sub_1.response_header['LAST_MODIFIED'].to_s).utc.to_i).to be_in_the_interval(Time.now.utc.to_i-1, Time.now.utc.to_i)
          expect(sub_1.response_header['ETAG'].to_s).to eql("W/0")
        end

        sub_2 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => headers
        sub_2.stream do |chunk|
          # send stop signal
          `#{ nginx_executable } -c #{ conf.configuration_filename } -s stop > /dev/null 2>&1`
          response += chunk
        end
        sub_2.callback do
          expect(response).to include("FOOTER")
          EventMachine.stop
        end
      end
    end
  end


  it "should reload normaly when receives HUP signal" do
    channel = 'ch_test_send_hup_signal'
    body = 'body'
    response = response2 = ''
    pid = pid2 = 0
    open_sockets_1 = 0
    socket = nil

    nginx_run_server(config, :timeout => 60) do |conf|
      error_log_pre = File.readlines(conf.error_log)

      EventMachine.run do
        # create subscriber
        sub_1 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => headers
        sub_1.stream do |chunk|
          response = response + chunk
          if response.strip == conf.header_template
            # check statistics
            pub_1 = EventMachine::HttpRequest.new(nginx_address + '/channels-stats').get :head => headers
            pub_1.callback do
              expect(pub_1).to be_http_status(200).with_body
              resp_1 = JSON.parse(pub_1.response)
              expect(resp_1.has_key?("channels")).to be_truthy
              expect(resp_1["channels"].to_i).to eql(1)
              expect(resp_1["by_worker"].count).to eql(1)
              pid = resp_1["by_worker"][0]['pid'].to_i

              open_sockets_1 = `lsof -p #{Process.getpgid pid} | grep socket | wc -l`.strip

              socket = open_socket(nginx_host, nginx_port)
              socket.print "GET /sub/#{channel} HTTP/1.1\r\nHost: test\r\nX-Nginx-PushStream-Mode: long-polling\r\n\r\n"

              # send reload signal
              `#{ nginx_executable } -c #{ conf.configuration_filename } -s reload > /dev/null 2>&1`
            end
          end
        end

        # check if first worker die
        timer = EM.add_periodic_timer(0.5) do

          # check statistics again
          pub_4 = EventMachine::HttpRequest.new(nginx_address + '/channels-stats').get :head => headers
          pub_4.callback do
            resp_3 = JSON.parse(pub_4.response)
            expect(resp_3.has_key?("by_worker")).to be_truthy

            old_process_running = Process.getpgid(pid) rescue false
            if !old_process_running && (resp_3["by_worker"].count == 1) && (pid != resp_3["by_worker"][0]['pid'].to_i)
              timer.cancel

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
                      expect(resp_2.has_key?("channels")).to be_truthy
                      expect(resp_2["channels"].to_i).to eql(1)
                      expect(resp_2["published_messages"].to_i).to eql(1)
                      expect(resp_2["subscribers"].to_i).to eql(1)

                      open_sockets_2 = `lsof -p #{Process.getpgid resp_3["by_worker"][0]['pid'].to_i} | grep socket | wc -l`.strip
                      expect(open_sockets_2).to eql(open_sockets_1)

                      EventMachine.stop

                      # send stop signal
                      `#{ nginx_executable } -c #{ conf.configuration_filename } -s stop > /dev/null 2>&1`

                      error_log_pos = File.readlines(conf.error_log)
                      expect((error_log_pos - error_log_pre).join).not_to include("open socket")
                      socket.close unless socket.nil?
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

  shared_examples_for "reload server" do
    it "should reload fast" do
      channel = 'ch_test_send_hup_signal'
      pid = pid2 = 0

      nginx_run_server(config.merge(custom_config), :timeout => 5) do |conf|
        EventMachine.run do
          # create subscriber
          sub_1 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => headers
          sub_1.stream do |chunk|
          end

          EM.add_timer(1) do
            # check statistics
            pub_1 = EventMachine::HttpRequest.new(nginx_address + '/channels-stats').get :head => headers
            pub_1.callback do
              expect(pub_1).to be_http_status(200).with_body
              resp_1 = JSON.parse(pub_1.response)
              expect(resp_1["subscribers"].to_i).to eql(1)
              expect(resp_1["channels"].to_i).to eql(1)
              expect(resp_1["by_worker"].count).to eql(1)
              pid = resp_1["by_worker"][0]['pid'].to_i

              # send reload signal
              `#{ nginx_executable } -c #{ conf.configuration_filename } -s reload > /dev/null 2>&1`

              # check if first worker die
              EM.add_periodic_timer(1) do

                # check statistics
                pub_4 = EventMachine::HttpRequest.new(nginx_address + '/channels-stats').get :head => headers
                pub_4.callback do
                  resp_3 = JSON.parse(pub_4.response)
                  expect(resp_3.has_key?("by_worker")).to be_truthy

                  if resp_3["by_worker"].count == 1
                    expect(resp_3["subscribers"].to_i).to eql(0)
                    expect(resp_3["channels"].to_i).to eql(1)
                    pid2 = resp_3["by_worker"][0]['pid'].to_i

                    expect(pid).not_to eql(pid2)
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

  context "with a big ping message interval" do
    let(:custom_config) do
      {
        :ping_message_interval => "10m",
        :subscriber_connection_ttl => '10s'
      }
    end

    it_should_behave_like "reload server"
  end

  context "with a big subscriber connection ttl" do
    let(:custom_config) do
      {
        :ping_message_interval => "1s",
        :subscriber_connection_ttl => '10m'
      }
    end

    it_should_behave_like "reload server"
  end

  it "should ignore changes on shared memory size when doing a reload" do
    channel = 'ch_test_reload_with_different_shared_memory_size'
    body = 'body'
    response = response2 = ''
    pid = pid2 = 0

    nginx_run_server(config, :timeout => 10) do |conf|
      EventMachine.run do
        publish_message(channel, {}, body)
        # check statistics
        pub_1 = EventMachine::HttpRequest.new(nginx_address + '/channels-stats').get :head => headers
        pub_1.callback do
          expect(pub_1).to be_http_status(200).with_body
          resp_1 = JSON.parse(pub_1.response)
          expect(resp_1.has_key?("channels")).to be_truthy
          expect(resp_1["channels"].to_i).to eql(1)
          expect(resp_1["published_messages"].to_i).to eql(1)

          conf.configuration[:shared_memory_size] = '20m'
          conf.create_configuration_file

          # send reload signal
          `#{ nginx_executable } -c #{ conf.configuration_filename } -s reload > /dev/null 2>&1`

          sleep 5

          pub_2 = EventMachine::HttpRequest.new(nginx_address + '/channels-stats').get :head => headers
          pub_2.callback do
            expect(pub_2).to be_http_status(200).with_body
            resp_2 = JSON.parse(pub_2.response)
            expect(resp_2.has_key?("channels")).to be_truthy
            expect(resp_2["channels"].to_i).to eql(1)
            expect(resp_2["published_messages"].to_i).to eql(1)

            error_log = File.read(conf.error_log)
            expect(error_log).to include("Cannot change memory area size without restart, ignoring change")

            EventMachine.stop
          end
        end
      end
    end
  end
end
