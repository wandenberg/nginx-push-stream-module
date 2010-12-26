require 'rubygems'
require 'em-http'
require 'test/unit'
require File.expand_path('base_test_case', File.dirname(__FILE__))

class TestSubscriberProperties < Test::Unit::TestCase
  include BaseTestCase

  def config_test_header_template
    @test_config_file = "test_header_template.conf"
    @header_template = "HEADER\r\nTEMPLATE\r\n1234\r\n"
    @authorized_channels_only = "off"
  end

  def test_header_template
    channel = 'ch1'
    headers = {'accept' => 'text/html'}

    EventMachine.run {
      sub = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => headers, :timeout => 60
      sub.stream { |chunk|
        assert_equal("#{@header_template}\r\n", chunk, "Didn't received header template")
        EventMachine.stop
      }
      fail_if_connecttion_error(sub)
    }
  end

  def config_test_content_type
    @test_config_file = "test_content_type.conf"
    @content_type = "custom content type"
    @authorized_channels_only = "off"
  end

  def test_content_type
    channel = 'ch2'
    headers = {'accept' => 'text/html'}

    EventMachine.run {
      sub = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => headers, :timeout => 60
      sub.stream { |chunk|
        assert_equal(@content_type, sub.response_header['CONTENT_TYPE'], "Didn't received correct content type")
        EventMachine.stop
      }
      fail_if_connecttion_error(sub)
    }
  end

  def config_test_ping_message_interval
    @test_config_file = "test_ping_message_interval.conf"
    @subscriber_connection_timeout = "0s"
    @ping_message_interval = "2s"
  end

  def test_ping_message_interval
    channel = 'ch3'
    headers = {'accept' => 'text/html'}

    step1 = step2 = step3 = step4 = nil

    chuncksReceived = 0
    EventMachine.run {
      sub = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => headers, :timeout => 60
      sub.stream { |chunk|
        chuncksReceived += 1;
        step1 = Time.now if chuncksReceived == 1
        step2 = Time.now if chuncksReceived == 2
        step3 = Time.now if chuncksReceived == 3
        step4 = Time.now if chuncksReceived == 4
        if chuncksReceived == 4
          EventMachine.stop
        end
      }
      sub.callback {
        interval1 = time_diff_sec(step2, step1).round
        interval2 = time_diff_sec(step4, step3).round
        assert_equal(interval1, interval2, "Wrong #{interval1}, #{interval2} intervals")
      }
      fail_if_connecttion_error(sub)
    }
  end

end
