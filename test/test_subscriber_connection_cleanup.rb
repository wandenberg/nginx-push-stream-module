require 'rubygems'
require 'em-http'
require 'test/unit'
require File.expand_path('base_test_case', File.dirname(__FILE__))

class TestSubscriberConnectionCleanup < Test::Unit::TestCase
  include BaseTestCase

  def config_test_subscriber_connection_timeout
    @test_config_file = "test_subscriber_connection_timeout.conf"
    @subscriber_connection_timeout = "37s"
    @subscriber_disconnect_interval = "1s"
    @header_template = "HEADER_TEMPLATE"
    @ping_message_interval = "0s"
  end

  def test_subscriber_connection_timeout
    channel = 'ch1'
    headers = {'accept' => 'text/html'}

    start = Time.now
    receivedHeaderTemplate = false
    EventMachine.run {
      sub = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => headers, :timeout => 60
      sub.stream { |chunk|
        assert(chunk.include?(@header_template), "Didn't received header template")
      }
      sub.callback {
        stop = Time.now
        elapsed = time_diff_sec(start, stop)
        assert(elapsed >= 37 && elapsed <= 38.5, "Disconnect was in #{elapsed} seconds")
        EventMachine.stop
      }
      fail_if_connecttion_error(sub)
    }
  end

  def config_test_subscriber_disconnect_interval
    @test_config_file = "test_subscriber_disconnect_interval.conf"
    @subscriber_connection_timeout = "37s"
    @ping_message_interval = "5s"
    @subscriber_disconnect_interval = "5s"
  end

  def test_subscriber_disconnect_interval
    channel = 'ch2'
    headers = {'accept' => 'text/html'}

    start = Time.now
    chuncksReceived = 0
    EventMachine.run {
      sub = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => headers, :timeout => 60
      sub.stream { |chunk|
        chuncksReceived += 1;
      }
      sub.callback {
        stop = Time.now
        elapsed = time_diff_sec(start, stop)
        assert(elapsed >= 40 && elapsed <= 40.5, "Disconnect was in #{elapsed} seconds")
        assert_equal(9, chuncksReceived, "Received #{chuncksReceived} chuncks")
        EventMachine.stop
      }
      fail_if_connecttion_error(sub)
    }
  end
end
