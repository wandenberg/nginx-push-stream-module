require File.expand_path('base_test_case', File.dirname(__FILE__))

class TestSubscriberProperties < Test::Unit::TestCase
  include BaseTestCase

  def config_test_header_template
    @header_template = "HEADER\r\nTEMPLATE\r\n1234\r\n"
    @authorized_channels_only = "off"
  end

  def test_header_template
    channel = 'ch_test_header_template'
    headers = {'accept' => 'text/html'}

    EventMachine.run {
      sub = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => headers, :timeout => 60
      sub.stream { |chunk|
        assert_equal("#{@header_template}\r\n", chunk, "Didn't received header template")
        EventMachine.stop
      }
    }
  end

  def config_test_content_type
    @content_type = "custom content type"
    @authorized_channels_only = "off"
  end

  def test_content_type
    channel = 'ch_test_content_type'
    headers = {'accept' => 'text/html'}

    EventMachine.run {
      sub = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => headers, :timeout => 60
      sub.stream { |chunk|
        assert_equal(@content_type, sub.response_header['CONTENT_TYPE'], "Didn't received correct content type")
        EventMachine.stop
      }
    }
  end

  def config_test_ping_message_interval
    @subscriber_connection_timeout = nil
    @ping_message_interval = "2s"
  end

  def test_ping_message_interval
    channel = 'ch_test_ping_message_interval'
    headers = {'accept' => 'text/html'}

    step1 = step2 = step3 = step4 = nil

    chunksReceived = 0
    EventMachine.run {
      sub = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => headers, :timeout => 60
      sub.stream { |chunk|
        chunksReceived += 1;
        step1 = Time.now if chunksReceived == 1
        step2 = Time.now if chunksReceived == 2
        step3 = Time.now if chunksReceived == 3
        step4 = Time.now if chunksReceived == 4
        EventMachine.stop if chunksReceived == 4
      }
      sub.callback {
        assert_equal(4, chunksReceived, "Didn't received expected messages")
        interval1 = time_diff_sec(step2, step1).round
        interval2 = time_diff_sec(step4, step3).round
        assert_equal(interval1, interval2, "Wrong #{interval1}, #{interval2} intervals")
      }
    }
  end

end
