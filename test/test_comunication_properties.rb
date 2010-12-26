require 'rubygems'
require 'em-http'
require 'test/unit'
require File.expand_path('base_test_case', File.dirname(__FILE__))

class TestComunicationProperties < Test::Unit::TestCase
  include BaseTestCase

  def config_test_all_authorized
    @test_config_file = "test_all_authorized.conf"
    @authorized_channels_only = "off"
    @header_template = "connected"
  end

  def test_all_authorized
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

  def config_test_only_authorized
    @test_config_file = "test_only_authorized.conf"
    @authorized_channels_only = "on"
    @header_template = "connected"
  end

  def test_only_authorized
    channel = 'ch2'
    headers = {'accept' => 'text/html'}
    body = 'message to create a channel'

    EventMachine.run {
      sub_1 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => headers, :timeout => 60
      sub_1.callback { |chunk|
        assert_equal(403, sub_1.response_header.status, "Subscriber was not forbidden")

        pub = EventMachine::HttpRequest.new(nginx_address + '/pub?id=' + channel.to_s ).post :head => headers, :body => body, :timeout => 30
        pub.callback {
          sub_2 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => headers, :timeout => 60
          sub_2.stream { |chunk|
            assert_equal("#{@header_template}\r\n", chunk, "Didn't received header template")
            EventMachine.stop
          }
          fail_if_connecttion_error(sub_2)
        }
        fail_if_connecttion_error(pub)
      }
      fail_if_connecttion_error(sub_1)
    }
  end

  def config_test_message_buffer_timeout
    @test_config_file = "test_message_buffer_timeout.conf"
    @authorized_channels_only = "off"
    @header_template = "connected"
    @message_template = "~text~"
    @min_message_buffer_timeout = "12s"
  end

  def test_message_buffer_timeout
    channel = 'ch3'
    headers = {'accept' => 'text/html'}
    body = 'message to test buffer timeout '

    EventMachine.run {
      pub = EventMachine::HttpRequest.new(nginx_address + '/pub?id=' + channel.to_s ).post :head => headers, :body => body, :timeout => 30
      fail_if_connecttion_error(pub)
      EM.add_timer(2) do
        sub_1 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s + '.b1').get :head => headers, :timeout => 60
        sub_1.stream { |chunk|
          assert_equal("#{@header_template}\r\n#{body}\r\n", chunk, "Didn't received header and message")
          sub_1.close_connection
        }
        fail_if_connecttion_error(sub_1)
      end

      EM.add_timer(6) do
        sub_2 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s + '.b1').get :head => headers, :timeout => 60
        sub_2.stream { |chunk|
          assert_equal("#{@header_template}\r\n#{body}\r\n", chunk, "Didn't received header and message")
          sub_2.close_connection
        }
        fail_if_connecttion_error(sub_2)
      end

      EM.add_timer(13) do
        sub_3 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s + '.b1').get :head => headers, :timeout => 60
        sub_3.stream { |chunk|
          assert_equal("#{@header_template}\r\n", chunk, "Didn't received header")
          sub_3.close_connection
          EventMachine.stop
        }
        fail_if_connecttion_error(sub_3)
      end
    }
  end

  def config_test_message_template
    @test_config_file = "test_message_template.conf"
    @authorized_channels_only = "off"
    @header_template = "header"
    @message_template = '{\"duplicated\":\"~channel~\", \"channel\":\"~channel~\", \"message\":\"~text~\", \"message_id\":\"~id~\"}'
    @ping_message_interval = "1s"
  end

  def test_message_template
    channel = 'ch4'
    headers = {'accept' => 'text/html'}
    body = 'message to create a channel'

    EventMachine.run {
      chunksReceived = 0
      sub = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s + '.b1').get :head => headers, :timeout => 60
      sub.stream { |chunk|
        chunksReceived += 1
        if chunksReceived == 1
          assert_equal("#{@header_template}\r\n", chunk, "Didn't received header template")
          pub = EventMachine::HttpRequest.new(nginx_address + '/pub?id=' + channel.to_s ).post :head => headers, :body => body, :timeout => 30
          fail_if_connecttion_error(pub)
        end
        if chunksReceived == 2
          assert_equal("{\"duplicated\":\"ch4\", \"channel\":\"#{channel}\", \"message\":\"#{body}\", \"message_id\":\"1\"}\r\n", chunk, "Didn't received message formatted: #{chunk}")
        end
        if chunksReceived == 3
          assert_equal("{\"duplicated\":\"\", \"channel\":\"\", \"message\":\"\", \"message_id\":\"-1\"}\r\n", chunk, "Didn't received ping message: #{chunk}")
          EventMachine.stop
        end
      }
      fail_if_connecttion_error(sub)
    }
  end
end
