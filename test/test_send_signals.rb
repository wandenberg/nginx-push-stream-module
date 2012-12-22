require File.expand_path('base_test_case', File.dirname(__FILE__))

class TestSendSignals < Test::Unit::TestCase
  include BaseTestCase

  def global_configuration
    ENV['NGINX_WORKERS'] = '1'
    @memory_cleanup_timeout = '40s'
    @min_message_buffer_timeout = '60s'
    @subscriber_connection_timeout = '65s'
    @master_process = 'on'
    @daemon = 'on'
    @header_template = 'HEADER'
    @disable_ignore_childs = true
  end

  def test_send_hup_signal
    headers = {'accept' => 'application/json'}
    channel = 'ch_test_send_hup_signal'
    body = 'body'
    response = ''
    response2 = ''
    pid = 0
    pid2 = 0

    EventMachine.run {
      # create subscriber
      sub_1 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => headers, :timeout => 30
      sub_1.stream { |chunk|
        response = response + chunk
        if response.strip == @header_template
          # check statistics
          pub_1 = EventMachine::HttpRequest.new(nginx_address + '/channels-stats').get :head => headers, :timeout => 30
          pub_1.callback {
            assert_equal(200, pub_1.response_header.status, "Don't get channels statistics")
            assert_not_equal(0, pub_1.response_header.content_length, "Don't received channels statistics")
            resp_1 = JSON.parse(pub_1.response)
            assert(resp_1.has_key?("channels"), "Didn't received the correct answer with channels info")
            assert_equal(1, resp_1["channels"].to_i, "Didn't create channel")
            assert_equal(1, resp_1["by_worker"].count, "Didn't return infos by_worker")
            pid = resp_1["by_worker"][0]['pid'].to_i

            # send reload signal
            `#{ nginx_executable } -c #{ config_filename } -s reload > /dev/null 2>&1`
          }
        end
      }


      conectted_after_reloaded = false
      i = 0
      # check if first worker die
      EM.add_periodic_timer(0.5) do

        # check statistics again
        pub_4 = EventMachine::HttpRequest.new(nginx_address + '/channels-stats').get :head => headers, :timeout => 30
        pub_4.callback {
          resp_3 = JSON.parse(pub_4.response)
          assert(resp_3.has_key?("by_worker"), "Didn't received the correct answer with channels info")

          if resp_3["by_worker"].count == 2 && !conectted_after_reloaded
            conectted_after_reloaded = true

            # publish a message
            pub_2 = EventMachine::HttpRequest.new(nginx_address + '/pub?id=' + channel.to_s).post :head => headers, :body => body, :timeout => 30
            pub_2.callback {
              # add new subscriber
              sub_2 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s + '.b1').get :head => headers, :timeout => 30
              sub_2.stream { |chunk|
                response2 = response2 + chunk
                if response2.strip == @header_template
                  # check statistics again
                  pub_3 = EventMachine::HttpRequest.new(nginx_address + '/channels-stats').get :head => headers, :timeout => 30
                  pub_3.callback {

                    resp_2 = JSON.parse(pub_3.response)
                    assert(resp_2.has_key?("channels"), "Didn't received the correct answer with channels info")
                    assert_equal(1, resp_2["channels"].to_i, "Didn't create channel")
                    assert_equal(1, resp_2["published_messages"].to_i, "Didn't create messages")
                    assert_equal(2, resp_2["subscribers"].to_i, "Didn't create subscribers")
                    assert_equal(2, resp_2["by_worker"].count, "Didn't return infos by_worker")
                  }
                end
              }
            }
          end

          if resp_3["by_worker"].count == 1 && conectted_after_reloaded
            assert_equal(1, resp_3["channels"].to_i, "Didn't create channel")
            assert_equal(1, resp_3["published_messages"].to_i, "Didn't create messages")
            assert_equal(1, resp_3["subscribers"].to_i, "Didn't create subscribers")
            assert_equal(1, resp_3["by_worker"].count, "Didn't return infos by_worker")
            pid2 = resp_3["by_worker"][0]['pid'].to_i

            assert_not_equal(pid, pid2, "Didn't recreate worker")
            EventMachine.stop
          end

          i = i + 1
          if i == 120
            fail("Worker didn't die in 60 seconds")
            EventMachine.stop
          end
        }
      end

      add_test_timeout(60)
    }
  end

  def test_reload_with_different_shared_memory_size
    headers = {'accept' => 'application/json'}
    channel = 'ch_test_reload_with_different_shared_memory_size'
    body = 'body'

    EventMachine.run do
      publish_message_inline(channel, headers, body)
      # check statistics
      pub_1 = EventMachine::HttpRequest.new(nginx_address + '/channels-stats').get :head => headers, :timeout => 30
      pub_1.callback do
        assert_equal(200, pub_1.response_header.status, "Don't get channels statistics")
        assert_not_equal(0, pub_1.response_header.content_length, "Don't received channels statistics")
        resp_1 = JSON.parse(pub_1.response)
        assert(resp_1.has_key?("channels"), "Didn't received the correct answer with channels info")
        assert_equal(1, resp_1["channels"].to_i, "Didn't create channel")
        assert_equal(1, resp_1["published_messages"].to_i, "Didn't create messages")

        @max_reserved_memory = '20m'
        create_config_file

        # send reload signal
        `#{ nginx_executable } -c #{ config_filename } -s reload > /dev/null 2>&1`

        sleep 5

        pub_2 = EventMachine::HttpRequest.new(nginx_address + '/channels-stats').get :head => headers, :timeout => 30
        pub_2.callback do
          assert_equal(200, pub_2.response_header.status, "Don't get channels statistics")
          assert_not_equal(0, pub_2.response_header.content_length, "Don't received channels statistics")
          resp_2 = JSON.parse(pub_2.response)
          assert(resp_2.has_key?("channels"), "Didn't received the correct answer with channels info")
          assert_equal(1, resp_2["channels"].to_i, "Didn't create channel")
          assert_equal(1, resp_2["published_messages"].to_i, "Didn't create messages")

          error_log = File.read(@main_error_log)
          assert(error_log.include?("Cannot change memory area size without restart, ignoring change"), "Didn't log error message")

          EventMachine.stop
        end
      end

      add_test_timeout(10)
    end
  end

end
