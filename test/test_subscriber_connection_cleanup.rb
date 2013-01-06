require File.expand_path('base_test_case', File.dirname(__FILE__))

class TestSubscriberConnectionCleanup < Test::Unit::TestCase
  include BaseTestCase

  def config_test_subscriber_connection_timeout
    @subscriber_connection_timeout = "37s"
    @header_template = "HEADER_TEMPLATE"
    @footer_template = "FOOTER_TEMPLATE"
    @ping_message_interval = nil
  end

  def test_subscriber_connection_timeout
    channel = 'ch_test_subscriber_connection_timeout'
    headers = {'accept' => 'text/html'}

    start = Time.now
    response = ''

    EventMachine.run {
      sub = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s, :inactivity_timeout => 60).get :head => headers, :timeout => 60
      sub.stream { |chunk|
        response += chunk
        assert(response.include?(@header_template), "Didn't received header template")
      }
      sub.callback {
        stop = Time.now
        elapsed = time_diff_sec(start, stop)
        assert(elapsed >= 37 && elapsed <= 37.5, "Disconnect was in #{elapsed} seconds")
        assert(response.include?(@footer_template), "Didn't received footer template")
        EventMachine.stop
      }

      add_test_timeout(50)
    }
  end

  def config_test_subscriber_connection_timeout_with_ping_message
    @subscriber_connection_timeout = "37s"
    @ping_message_interval = "5s"
    @header_template = nil
    @footer_template = nil
  end

  def test_subscriber_connection_timeout_with_ping_message
    channel = 'ch_test_subscriber_connection_timeout_with_ping_message'
    headers = {'accept' => 'text/html'}

    start = Time.now
    chunksReceived = 0

    EventMachine.run {
      sub = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s, :inactivity_timeout => 15).get :head => headers, :timeout => 60
      sub.stream { |chunk|
        chunksReceived += 1;
      }
      sub.callback {
        stop = Time.now
        elapsed = time_diff_sec(start, stop)
        assert(elapsed >= 37 && elapsed <= 37.5, "Disconnect was in #{elapsed} seconds")
        assert_equal(7, chunksReceived, "Received #{chunksReceived} chunks")
        EventMachine.stop
      }

      add_test_timeout(50)
    }
  end


  def config_test_multiple_subscribers_connection_timeout
    @subscriber_connection_timeout = "5s"
    @header_template = "HEADER_TEMPLATE"
    @footer_template = "FOOTER_TEMPLATE"
    @ping_message_interval = nil
  end

  def test_multiple_subscribers_connection_timeout
    channel = 'ch_test_multiple_subscribers_connection_timeout'
    headers = {'accept' => 'text/html'}


    EventMachine.run {
      response_1 = ''
      sub_1 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => headers, :timeout => 60
      sub_1.stream { |chunk|
        response_1 += chunk
        assert(response_1.include?(@header_template), "Didn't received header template")
      }
      sub_1.callback {
        assert(response_1.include?(@footer_template), "Didn't received footer template")
      }

      sleep(2)

      response_2 = ''
      sub_2 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => headers, :timeout => 60
      sub_2.stream { |chunk|
        response_2 += chunk
        assert(response_2.include?(@header_template), "Didn't received header template")
      }
      sub_2.callback {
        assert(response_2.include?(@footer_template), "Didn't received footer template")

        response_4 = ''
        sub_4 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => headers, :timeout => 60
        sub_4.stream { |chunk|
          response_4 += chunk
          assert(response_4.include?(@header_template), "Didn't received header template")
        }
        sub_4.callback {
          assert(response_4.include?(@footer_template), "Didn't received footer template")
          EventMachine.stop
        }
      }

      sleep(6)

      response_3 = ''
      sub_3 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => headers, :timeout => 60
      sub_3.stream { |chunk|
        response_3 += chunk
        assert(response_3.include?(@header_template), "Didn't received header template")
      }
      sub_3.callback {
        assert(response_3.include?(@footer_template), "Didn't received footer template")
      }

      add_test_timeout(15)
    }
  end
end
