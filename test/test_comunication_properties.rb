require File.expand_path('base_test_case', File.dirname(__FILE__))

class TestComunicationProperties < Test::Unit::TestCase
  include BaseTestCase

  def config_test_all_authorized
    @authorized_channels_only = "off"
    @header_template = "connected"
  end

  def test_all_authorized
    channel = 'ch_test_all_authorized'
    headers = {'accept' => 'text/html'}

    EventMachine.run {
      sub = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => headers, :timeout => 60
      sub.stream { |chunk|
        assert_equal("#{@header_template}\r\n", chunk, "Didn't received header template")
        EventMachine.stop
      }
    }
  end

  def config_test_only_authorized
    @authorized_channels_only = "on"
    @header_template = "connected"
  end

  def test_only_authorized
    channel = 'ch_test_only_authorized'
    headers = {'accept' => 'text/html'}
    body = 'message to create a channel'

    EventMachine.run {
      sub_1 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => headers, :timeout => 60
      sub_1.callback { |chunk|
        assert_equal(403, sub_1.response_header.status, "Subscriber was not forbidden")
        assert_equal(0, sub_1.response_header.content_length, "Should response only with headers")
        assert_equal("Subscriber could not create channels.", sub_1.response_header['X_NGINX_PUSHSTREAM_EXPLAIN'], "Didn't receive the right error message")

        pub = EventMachine::HttpRequest.new(nginx_address + '/pub?id=' + channel.to_s ).post :head => headers, :body => body, :timeout => 30
        pub.callback {
          sub_2 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => headers, :timeout => 60
          sub_2.stream { |chunk2|
            assert_equal("#{@header_template}\r\n", chunk2, "Didn't received header template")
            EventMachine.stop
          }
        }
      }
    }
  end

  def config_test_message_buffer_timeout
    @authorized_channels_only = "off"
    @header_template = "connected"
    @message_template = "~text~"
    @min_message_buffer_timeout = "12s"
  end

  def test_message_buffer_timeout
    channel = 'ch_test_message_buffer_timeout'
    headers = {'accept' => 'text/html'}
    body = 'message to test buffer timeout '
    response_1 = response_2 = response_3 = ""
    sub_1 = sub_2 = sub_3 = nil

    EventMachine.run {
      pub = EventMachine::HttpRequest.new(nginx_address + '/pub?id=' + channel.to_s ).post :head => headers, :body => body, :timeout => 30
      EM.add_timer(2) do
        sub_1 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s + '.b1').get :head => headers, :timeout => 60
        sub_1.stream { |chunk|
          response_1 += chunk
          sub_1.close if response_1.include?(body)
        }
      end

      EM.add_timer(6) do
        sub_2 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s + '.b1').get :head => headers, :timeout => 60
        sub_2.stream { |chunk|
          response_2 += chunk
          sub_2.close if response_2.include?(body)
        }
      end

      #message will be certainly expired at 15 seconds
      EM.add_timer(16) do
        sub_3 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s + '.b1').get :head => headers, :timeout => 60
        sub_3.stream { |chunk|
          response_3 += chunk
          sub_3.close if response_3.include?(body)
        }
      end

      EM.add_timer(17) do
        assert_equal("#{@header_template}\r\n#{body}\r\n\r\n", response_1, "Didn't received header and message")
        assert_equal("#{@header_template}\r\n#{body}\r\n\r\n", response_2, "Didn't received header and message")
        assert_equal("#{@header_template}\r\n", response_3, "Didn't received header")
        EventMachine.stop
      end
    }
  end

  def config_test_message_template
    @authorized_channels_only = "off"
    @header_template = "header"
    @message_template = '{\"duplicated\":\"~channel~\", \"channel\":\"~channel~\", \"message\":\"~text~\", \"message_id\":\"~id~\"}'
    @ping_message_interval = "1s"
  end

  def test_message_template
    channel = 'ch_test_message_template'
    headers = {'accept' => 'text/html'}
    body = 'message to create a channel'

    publish_message(channel, headers, body)

    response = ""
    EventMachine.run {
      chunksReceived = 0
      sub = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s + '.b1').get :head => headers, :timeout => 60
      sub.stream { |chunk|
        response += chunk

        lines = response.split("\r\n")

        if lines.length >= 3
          assert_equal("#{@header_template}", lines[0], "Didn't received header template")
          assert_equal("{\"duplicated\":\"#{channel}\", \"channel\":\"#{channel}\", \"message\":\"#{body}\", \"message_id\":\"1\"}", lines[1], "Didn't received message formatted: #{lines[1]}")
          assert_equal("{\"duplicated\":\"\", \"channel\":\"\", \"message\":\"\", \"message_id\":\"-1\"}", lines[2], "Didn't received ping message: #{lines[2]}")
          EventMachine.stop
        end
      }
      add_test_timeout(20)
    }
  end

  def config_test_message_and_channel_with_same_pattern_of_the_template
    @authorized_channels_only = "off"
    @header_template = "header"
    @message_template = '{\"channel\":\"~channel~\", \"message\":\"~text~\", \"message_id\":\"~id~\"}'
    @ping_message_interval = "1s"
  end

  def test_message_and_channel_with_same_pattern_of_the_template
    channel = 'ch_test_message_and_channel_with_same_pattern_of_the_template~channel~~channel~~channel~~text~~text~~text~'
    headers = {'accept' => 'text/html'}
    body = '~channel~~channel~~channel~~text~~text~~text~'

    publish_message(channel, headers, body)

    response = ""
    EventMachine.run {
      chunksReceived = 0
      sub = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s + '.b1').get :head => headers, :timeout => 60
      sub.stream { |chunk|
        response += chunk

        lines = response.split("\r\n")

        if lines.length >= 3
          assert_equal("#{@header_template}", lines[0], "Didn't received header template")
          assert_equal("{\"channel\":\"ch_test_message_and_channel_with_same_pattern_of_the_template~channel~~channel~~channel~~channel~~channel~~channel~~text~~text~~text~~channel~~channel~~channel~~text~~text~~text~~channel~~channel~~channel~~text~~text~~text~\", \"message\":\"~channel~~channel~~channel~~text~~text~~text~\", \"message_id\":\"1\"}", lines[1], "Didn't received message formatted: #{lines[1]}")
          assert_equal("{\"channel\":\"\", \"message\":\"\", \"message_id\":\"-1\"}", lines[2], "Didn't received ping message: #{lines[2]}")
          EventMachine.stop
        end
      }
      add_test_timeout(20)
    }
  end
end
