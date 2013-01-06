require File.expand_path('base_test_case', File.dirname(__FILE__))

class TestSubscriberLongPolling < Test::Unit::TestCase
  include BaseTestCase

  def global_configuration
    @ping_message_interval = nil
    @header_template = nil
    @footer_template = nil
    @message_template = nil
    @subscriber_mode = 'long-polling'
  end

  def test_disconnect_after_receive_a_message_when_longpolling_is_on
    headers = {'accept' => 'application/json'}
    channel = 'ch_test_disconnect_after_receive_a_message_when_longpolling_is_on'
    body = 'body'
    response = ""

    EventMachine.run {
      sub_1 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => headers, :timeout => 30
      sub_1.stream { |chunk|
        response += chunk
      }
      sub_1.callback { |chunk|
        assert_equal("#{body}\r\n", response, "Wrong message")

        headers.merge!({'If-Modified-Since' => sub_1.response_header['LAST_MODIFIED'], 'If-None-Match' => sub_1.response_header['ETAG']})
        response = ""
        sub_2 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => headers, :timeout => 30
        sub_2.stream { |chunk|
          response += chunk
        }
        sub_2.callback { |chunk|
          assert_equal("#{body} 1\r\n", response, "Wrong message")
          EventMachine.stop
        }

        publish_message_inline(channel, {'accept' => 'text/html'}, body + " 1")
      }

      publish_message_inline(channel, {'accept' => 'text/html'}, body)

      add_test_timeout
    }
  end

  def test_disconnect_after_receive_old_messages_by_backtrack_when_longpolling_is_on
    channel = 'ch_test_disconnect_after_receive_old_messages_by_backtrack_when_longpolling_is_on'

    response = ''

    EventMachine.run {
      publish_message_inline(channel, {'accept' => 'text/html'}, 'msg 1')
      publish_message_inline(channel, {'accept' => 'text/html'}, 'msg 2')
      publish_message_inline(channel, {'accept' => 'text/html'}, 'msg 3')
      publish_message_inline(channel, {'accept' => 'text/html'}, 'msg 4')

      sub = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s + '.b2').get
      sub.stream { | chunk |
        response += chunk
      }
      sub.callback { |chunk|
        assert_equal("msg 3\r\nmsg 4\r\n", response, "The published message was not received correctly")

        response = ''
        sub_1 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => {'If-Modified-Since' => sub.response_header['LAST_MODIFIED'], 'If-None-Match' => sub.response_header['ETAG']}
        sub_1.stream { | chunk |
          response += chunk
        }
        sub_1.callback { |chunk|
          assert_equal("msg 5\r\n", response, "The published message was not received correctly")

          EventMachine.stop
        }

        publish_message_inline(channel, {'accept' => 'text/html'}, 'msg 5')
      }

      add_test_timeout
    }
  end

  def test_disconnect_after_receive_old_messages_by_last_event_id_when_longpolling_is_on
    channel = 'ch_test_disconnect_after_receive_old_messages_by_last_event_id_when_longpolling_is_on'

    response = ''

    EventMachine.run {
      publish_message_inline(channel, {'accept' => 'text/html', 'Event-Id' => 'event 1' }, 'msg 1')
      publish_message_inline(channel, {'accept' => 'text/html', 'Event-Id' => 'event 2' }, 'msg 2')
      publish_message_inline(channel, {'accept' => 'text/html' }, 'msg 3')
      publish_message_inline(channel, {'accept' => 'text/html', 'Event-Id' => 'event 3' }, 'msg 4')

      sub = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => {'Last-Event-Id' => 'event 2' }
      sub.stream { | chunk |
        response += chunk
      }
      sub.callback { |chunk|
        assert_equal("msg 3\r\nmsg 4\r\n", response, "The published message was not received correctly")
        EventMachine.stop
      }

      add_test_timeout
    }
  end

  def test_receive_old_messages_from_different_channels
    headers = {'accept' => 'application/json'}
    channel_1 = 'ch_test_receive_old_messages_from_different_channels_1'
    channel_2 = 'ch_test_receive_old_messages_from_different_channels_2'
    body = 'body'
    response = ''

    EventMachine.run {
      publish_message_inline(channel_1, {'accept' => 'text/html'}, body + "_1")
      publish_message_inline(channel_2, {'accept' => 'text/html'}, body + "_2")

      sub_1 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel_2.to_s + '/' + channel_1.to_s).get :head => headers, :timeout => 30
      sub_1.callback {
        assert_equal(200, sub_1.response_header.status, "Wrong status")
        assert_not_equal("", sub_1.response_header['LAST_MODIFIED'].to_s, "Wrong header")
        assert_not_equal("", sub_1.response_header['ETAG'].to_s, "Wrong header")
        assert_equal("#{body}_2\r\n#{body}_1\r\n", sub_1.response, "The published message was not received correctly")

        headers.merge!({'If-Modified-Since' => sub_1.response_header['LAST_MODIFIED'], 'If-None-Match' => sub_1.response_header['ETAG']})
        sub_2 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel_2.to_s + '/' + channel_1.to_s).get :head => headers, :timeout => 30
        sub_2.callback {
          assert_equal(200, sub_2.response_header.status, "Wrong status")
          assert_not_equal(sub_1.response_header['LAST_MODIFIED'], sub_2.response_header['LAST_MODIFIED'].to_s, "Wrong header")
          assert_equal("0", sub_2.response_header['ETAG'].to_s, "Wrong header")
          assert_equal("#{body}1_1\r\n", sub_2.response, "The published message was not received correctly")

          headers.merge!({'If-Modified-Since' => sub_2.response_header['LAST_MODIFIED'], 'If-None-Match' => sub_2.response_header['ETAG']})
          sub_3 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel_2.to_s + '/' + channel_1.to_s).get :head => headers, :timeout => 30
          sub_3.callback {
            assert_equal(200, sub_3.response_header.status, "Wrong status")
            assert_not_equal(sub_2.response_header['LAST_MODIFIED'], sub_3.response_header['LAST_MODIFIED'].to_s, "Wrong header")
            assert_equal("0", sub_3.response_header['ETAG'].to_s, "Wrong header")
            assert_equal("#{body}1_2\r\n", sub_3.response, "The published message was not received correctly")

            EventMachine.stop
          }

          sleep(1) # to publish the second message in a different second from the first
          publish_message_inline(channel_2, {'accept' => 'text/html'}, body + "1_2")
        }

        sleep(1) # to publish the second message in a different second from the first
        publish_message_inline(channel_1, {'accept' => 'text/html'}, body + "1_1")
      }

      add_test_timeout
    }
  end

  def config_test_disconnect_after_receive_a_message_when_has_header_mode_longpolling
    @subscriber_mode = nil
  end

  def test_disconnect_after_receive_a_message_when_has_header_mode_longpolling
    headers = {'accept' => 'application/json', 'X-Nginx-PushStream-Mode' => 'long-polling'}
    channel = 'ch_test_disconnect_after_receive_a_message_when_has_header_mode_longpolling'
    body = 'body'
    response = ""

    EventMachine.run {
      sub_1 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => headers, :timeout => 30
      sub_1.stream { |chunk|
        response += chunk
      }
      sub_1.callback { |chunk|
        assert_equal("#{body}\r\n", response, "Wrong message")

        headers.merge!({'If-Modified-Since' => sub_1.response_header['LAST_MODIFIED'], 'If-None-Match' => sub_1.response_header['ETAG']})
        response = ""
        sub_2 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => headers, :timeout => 30
        sub_2.stream { |chunk|
          response += chunk
        }
        sub_2.callback { |chunk|
          assert_equal("#{body} 1\r\n", response, "Wrong message")
          EventMachine.stop
        }

        publish_message_inline(channel, {'accept' => 'text/html'}, body + " 1")
      }

      publish_message_inline(channel, {'accept' => 'text/html'}, body)

      add_test_timeout
    }
  end

  def config_test_disconnect_after_receive_old_messages_by_backtrack_when_has_header_mode_longpolling
    @subscriber_mode = nil
  end

  def test_disconnect_after_receive_old_messages_by_backtrack_when_has_header_mode_longpolling
    headers = {'accept' => 'application/json', 'X-Nginx-PushStream-Mode' => 'long-polling'}
    channel = 'ch_test_disconnect_after_receive_old_messages_by_backtrack_when_has_header_mode_longpolling'

    response = ''

    EventMachine.run {
      publish_message_inline(channel, {'accept' => 'text/html'}, 'msg 1')
      publish_message_inline(channel, {'accept' => 'text/html'}, 'msg 2')
      publish_message_inline(channel, {'accept' => 'text/html'}, 'msg 3')
      publish_message_inline(channel, {'accept' => 'text/html'}, 'msg 4')

      sub = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s + '.b2').get :head => headers
      sub.stream { | chunk |
        response += chunk
      }
      sub.callback { |chunk|
        assert_equal("msg 3\r\nmsg 4\r\n", response, "The published message was not received correctly")

        headers.merge!({'If-Modified-Since' => sub.response_header['LAST_MODIFIED'], 'If-None-Match' => sub.response_header['ETAG']})
        response = ''
        sub_1 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => headers
        sub_1.stream { | chunk |
          response += chunk
        }
        sub_1.callback { |chunk|
          assert_equal("msg 5\r\n", response, "The published message was not received correctly")

          EventMachine.stop
        }

        publish_message_inline(channel, {'accept' => 'text/html'}, 'msg 5')
      }

      add_test_timeout
    }
  end

  def config_test_disconnect_after_receive_old_messages_by_last_event_id_when_has_header_mode_longpolling
    @subscriber_mode = nil
  end

  def test_disconnect_after_receive_old_messages_by_last_event_id_when_has_header_mode_longpolling
    channel = 'ch_test_disconnect_after_receive_old_messages_by_last_event_id_when_has_header_mode_longpolling'

    response = ''

    EventMachine.run {
      publish_message_inline(channel, {'accept' => 'text/html', 'Event-Id' => 'event 1' }, 'msg 1')
      publish_message_inline(channel, {'accept' => 'text/html', 'Event-Id' => 'event 2' }, 'msg 2')
      publish_message_inline(channel, {'accept' => 'text/html' }, 'msg 3')
      publish_message_inline(channel, {'accept' => 'text/html', 'Event-Id' => 'event 3' }, 'msg 4')

      sub = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => {'Last-Event-Id' => 'event 2', 'X-Nginx-PushStream-Mode' => 'long-polling' }
      sub.stream { | chunk |
        response += chunk
      }
      sub.callback { |chunk|
        assert_equal("msg 3\r\nmsg 4\r\n", response, "The published message was not received correctly")
        EventMachine.stop
      }

      add_test_timeout
    }
  end

  def config_test_receive_old_messages_from_different_channels_when_has_header_mode_longpolling
    @subscriber_mode = nil
  end

  def test_receive_old_messages_from_different_channels_when_has_header_mode_longpolling
    headers = {'accept' => 'application/json', 'X-Nginx-PushStream-Mode' => 'long-polling'}
    channel_1 = 'ch_test_receive_old_messages_from_different_channels_when_has_header_mode_longpolling_1'
    channel_2 = 'ch_test_receive_old_messages_from_different_channels_when_has_header_mode_longpolling_2'
    body = 'body'
    response = ''

    EventMachine.run {
      publish_message_inline(channel_1, {'accept' => 'text/html'}, body + "_1")
      publish_message_inline(channel_2, {'accept' => 'text/html'}, body + "_2")

      sub_1 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel_2.to_s + '/' + channel_1.to_s).get :head => headers, :timeout => 30
      sub_1.callback {
        assert_equal(200, sub_1.response_header.status, "Wrong status")
        assert_not_equal("", sub_1.response_header['LAST_MODIFIED'].to_s, "Wrong header")
        assert_not_equal("", sub_1.response_header['ETAG'].to_s, "Wrong header")
        assert_equal("#{body}_2\r\n#{body}_1\r\n", sub_1.response, "The published message was not received correctly")

        headers.merge!({'If-Modified-Since' => sub_1.response_header['LAST_MODIFIED'], 'If-None-Match' => sub_1.response_header['ETAG']})
        sub_2 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel_2.to_s + '/' + channel_1.to_s).get :head => headers, :timeout => 30
        sub_2.callback {
          assert_equal(200, sub_2.response_header.status, "Wrong status")
          assert_not_equal(sub_1.response_header['LAST_MODIFIED'], sub_2.response_header['LAST_MODIFIED'].to_s, "Wrong header")
          assert_equal("0", sub_2.response_header['ETAG'].to_s, "Wrong header")
          assert_equal("#{body}1_1\r\n", sub_2.response, "The published message was not received correctly")

          headers.merge!({'If-Modified-Since' => sub_2.response_header['LAST_MODIFIED'], 'If-None-Match' => sub_2.response_header['ETAG']})
          sub_3 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel_2.to_s + '/' + channel_1.to_s).get :head => headers, :timeout => 30
          sub_3.callback {
            assert_equal(200, sub_3.response_header.status, "Wrong status")
            assert_not_equal(sub_2.response_header['LAST_MODIFIED'], sub_3.response_header['LAST_MODIFIED'].to_s, "Wrong header")
            assert_equal("0", sub_3.response_header['ETAG'].to_s, "Wrong header")
            assert_equal("#{body}1_2\r\n", sub_3.response, "The published message was not received correctly")

            EventMachine.stop
          }

          sleep(1) # to publish the second message in a different second from the first
          publish_message_inline(channel_2, {'accept' => 'text/html'}, body + "1_2")
        }

        sleep(1) # to publish the second message in a different second from the first
        publish_message_inline(channel_1, {'accept' => 'text/html'}, body + "1_1")
      }

      add_test_timeout
    }
  end

  def config_test_disconnect_long_polling_subscriber_when_disconnect_timeout_is_set
    @subscriber_connection_timeout = "15s"
  end

  def test_disconnect_long_polling_subscriber_when_disconnect_timeout_is_set
    channel = 'ch_test_disconnect_long_polling_subscriber_when_disconnect_timeout_is_set'

    start = Time.now

    EventMachine.run {
      sub = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s, :inactivity_timeout => 30).get :timeout => 30
      sub.callback {
        stop = Time.now
        elapsed = time_diff_sec(start, stop)
        assert(elapsed >= 15 && elapsed <= 15.5, "Disconnect was in #{elapsed} seconds")
        assert_equal(304, sub.response_header.status, "Wrong status")
        assert_equal(Time.now.utc.strftime("%a, %d %b %Y %T %Z"), sub.response_header['LAST_MODIFIED'].to_s, "Wrong header")
        assert_equal("0", sub.response_header['ETAG'].to_s, "Wrong header")
        assert_equal(0, sub.response_header.content_length, "Wrong response")
        EventMachine.stop
      }

      add_test_timeout(20)
    }
  end

  def config_test_disconnect_long_polling_subscriber_when_longpolling_timeout_is_set
    @subscriber_connection_timeout = "15s"
    @longpolling_connection_ttl = "5s"
  end

  def test_disconnect_long_polling_subscriber_when_longpolling_timeout_is_set
    channel = 'ch_test_disconnect_long_polling_subscriber_when_longpolling_timeout_is_set'

    start = Time.now

    EventMachine.run {
      sub = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :timeout => 30
      sub.callback {
        stop = Time.now
        elapsed = time_diff_sec(start, stop)
        assert(elapsed >= 5 && elapsed <= 5.5, "Disconnect was in #{elapsed} seconds")
        assert_equal(304, sub.response_header.status, "Wrong status")
        assert_equal(Time.now.utc.strftime("%a, %d %b %Y %T %Z"), sub.response_header['LAST_MODIFIED'].to_s, "Wrong header")
        assert_equal("0", sub.response_header['ETAG'].to_s, "Wrong header")
        assert_equal(0, sub.response_header.content_length, "Wrong response")
        EventMachine.stop
      }

      add_test_timeout(20)
    }
  end

  def config_test_disconnect_long_polling_subscriber_when_only_longpolling_timeout_is_set
    @longpolling_connection_ttl = "3s"
  end

  def test_disconnect_long_polling_subscriber_when_only_longpolling_timeout_is_set
    channel = 'ch_test_disconnect_long_polling_subscriber_when_only_longpolling_timeout_is_set'

    start = Time.now

    EventMachine.run {
      sub = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :timeout => 30
      sub.callback {
        stop = Time.now
        elapsed = time_diff_sec(start, stop)
        assert(elapsed >= 3 && elapsed <= 3.5, "Disconnect was in #{elapsed} seconds")
        assert_equal(304, sub.response_header.status, "Wrong status")
        assert_equal(Time.now.utc.strftime("%a, %d %b %Y %T %Z"), sub.response_header['LAST_MODIFIED'].to_s, "Wrong header")
        assert_equal("0", sub.response_header['ETAG'].to_s, "Wrong header")
        assert_equal(0, sub.response_header.content_length, "Wrong response")
        EventMachine.stop
      }

      add_test_timeout(20)
    }
  end

  def config_test_not_receive_ping_message
    @subscriber_connection_timeout = "5s"
    @ping_message_interval = "1s"
  end

  def test_not_receive_ping_message
    channel = 'ch_test_not_receive_ping_message'

    EventMachine.run {
      sub = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :timeout => 30
      sub.callback {
        assert_equal(304, sub.response_header.status, "Wrong status")
        assert_equal(0, sub.response_header.content_length, "Wrong response")
        EventMachine.stop
      }

      add_test_timeout(10)
    }
  end

  def config_test_receiving_messages_with_etag_greather_than_recent_message
    @store_messages = "on"
    @message_template = '{\"id\":\"~id~\", \"message\":\"~text~\"}'
  end

  def test_receiving_messages_with_etag_greather_than_recent_message
    headers = {'accept' => 'application/json'}
    body_prefix = 'published message '
    channel = 'ch_test_receiving_messages_with_etag_greather_than_recent_message'
    messagens_to_publish = 10

    EventMachine.run {

      i = 0
      stored_messages = 0
      EM.add_periodic_timer(0.001) do
        if i < messagens_to_publish
          i += 1
          publish_message_inline(channel.to_s, headers, body_prefix + i.to_s)
        else
        end
      end

      EM.add_timer(1) do
        pub = EventMachine::HttpRequest.new(nginx_address + '/pub?id=' + channel.to_s ).post :head => headers, :body => body_prefix + i.to_s, :timeout => 30
        pub.callback {
          response = JSON.parse(pub.response)
          stored_messages = response["stored_messages"].to_i
        }
      end

      EM.add_timer(2) do
        sub = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => {'If-Modified-Since' => 'Thu, 1 Jan 1970 00:00:00 GMT', 'If-None-Match' => 0},  :timeout => 30
        sub.callback {
          assert_equal(200, sub.response_header.status, "Wrong status")
          assert(stored_messages == (messagens_to_publish + 1), "Do not stored all published messages")
          messages = sub.response.split("\r\n")
          assert_equal((messagens_to_publish + 1), messages.count, "Wrong header")
          messages.each_with_index do |content, index|
            message = JSON.parse(content)
            assert_equal((index + 1), message["id"].to_i, "Wrong message order")
          end
          EventMachine.stop
        }
      end

      add_test_timeout
    }
  end

  def config_test_receiving_messages_when_connected_in_more_then_one_channel
    @store_messages = "on"
    @message_template = '{\"id\":\"~id~\", \"message\":\"~text~\", \"channel\":\"~channel~\"}'
  end

  def test_receiving_messages_when_connected_in_more_then_one_channel
    headers = {'accept' => 'application/json'}
    body = 'published message'
    channel_1 = 'ch_test_receiving_messages_when_connected_in_more_then_one_channel_1'
    channel_2 = 'ch_test_receiving_messages_when_connected_in_more_then_one_channel_2'

    EventMachine.run {

      sub_1 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel_1.to_s + '/' + channel_2.to_s).get :head => {'If-Modified-Since' => 'Thu, 1 Jan 1970 00:00:00 GMT', 'If-None-Match' => 0},  :timeout => 30
      sub_1.callback {
        assert_equal(200, sub_1.response_header.status, "Wrong status")
        response = JSON.parse(sub_1.response)
        assert_equal(channel_1, response["channel"], "Wrong channel")

        sub_2 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel_1.to_s + '/' + channel_2.to_s).get :head => {'If-Modified-Since' => sub_1.response_header['LAST_MODIFIED'], 'If-None-Match' => sub_1.response_header['ETAG']},  :timeout => 30
        sub_2.callback {
          assert_equal(200, sub_2.response_header.status, "Wrong status")
          response = JSON.parse(sub_2.response)
          assert_equal(channel_2, response["channel"], "Wrong channel")
          assert_equal(sub_1.response_header['ETAG'].to_i + 1, sub_2.response_header['ETAG'].to_i)

          EventMachine.stop
        }
      }

      publish_message_inline(channel_1.to_s, headers, body)
      publish_message_inline(channel_2.to_s, headers, body)

      add_test_timeout
    }
  end

  def config_test_delete_channel_with_long_polling_subscriber
    @publisher_mode = 'admin'
    @message_template = '{\"id\":\"~id~\", \"message\":\"~text~\", \"channel\":\"~channel~\"}'
  end

  def test_delete_channel_with_long_polling_subscriber
    headers = {'accept' => 'application/json'}
    body = 'published message'
    channel = 'ch_test_delete_channel_with_long_polling_subscriber'

    resp = ""
    EventMachine.run {
      sub_1 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => headers, :timeout => 30
      sub_1.callback {
        assert_equal(200, sub_1.response_header.status, "Wrong status")
        response = JSON.parse(sub_1.response)
        assert_equal(channel, response["channel"], "Wrong channel")
        assert_equal(-2, response["id"].to_i, "Wrong channel")
        EventMachine.stop
      }

      pub = EventMachine::HttpRequest.new(nginx_address + '/pub?id=' + channel.to_s).delete :head => headers, :timeout => 30
      pub.callback {
        assert_equal(200, pub.response_header.status, "Request was not received")
        assert_equal(0, pub.response_header.content_length, "Should response only with headers")
        assert_equal("Channel deleted.", pub.response_header['X_NGINX_PUSHSTREAM_EXPLAIN'], "Didn't receive the right error message")
      }

      add_test_timeout
    }
  end

  def config_test_send_modified_since_and_none_match_values_not_using_headers
    @last_received_message_time = "$arg_time"
    @last_received_message_tag = "$arg_tag"
  end

  def test_send_modified_since_and_none_match_values_not_using_headers
    headers = {'accept' => 'application/json'}
    channel = 'ch_test_send_modified_since_and_none_match_values_not_using_headers'
    body = 'body'
    response = ""

    EventMachine.run {
      sub_1 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => headers, :timeout => 30
      sub_1.stream { |chunk|
        response += chunk
      }
      sub_1.callback { |chunk|
        assert_equal("#{body}\r\n", response, "Wrong message")

        time = sub_1.response_header['LAST_MODIFIED']
        tag = sub_1.response_header['ETAG']

        response = ""
        sub_2 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s + '?time=' + time + '&tag=' + tag).get :head => headers, :timeout => 30
        sub_2.stream { |chunk|
          response += chunk
        }
        sub_2.callback { |chunk|
          assert_equal("#{body} 1\r\n", response, "Wrong message")
          EventMachine.stop
        }

        publish_message_inline(channel, {'accept' => 'text/html'}, body + " 1")
      }

      publish_message_inline(channel, {'accept' => 'text/html'}, body)

      add_test_timeout
    }
  end

  def test_return_message_using_function_name_specified_in_callback_parameter
    headers = {'accept' => 'application/javascript'}
    channel = 'ch_test_return_message_using_function_name_specified_in_callback_parameter'
    body = 'body'
    response = ""
    callback_function_name = "callback_function"

    EventMachine.run {

      sub_1 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s + '?callback=' + callback_function_name).get :head => headers, :timeout => 30
      sub_1.callback {
        assert_equal("#{callback_function_name}\r\n([#{body}\r\n]);\r\n", sub_1.response, "Wrong message")
        EventMachine.stop
      }

      publish_message_inline(channel, {'accept' => 'text/html'}, body)

      add_test_timeout
    }
  end

  def test_return_old_messages_using_function_name_specified_in_callback_parameter_grouping_in_one_answer
    headers = {'accept' => 'application/javascript'}
    channel = 'ch_test_return_old_messages_using_function_name_specified_in_callback_parameter_grouping_in_one_answer'
    body = 'body'
    response = ""
    callback_function_name = "callback_function"

    EventMachine.run {

      publish_message_inline(channel, {'accept' => 'text/html'}, body)
      publish_message_inline(channel, {'accept' => 'text/html'}, body + "1")

      sub_1 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s + '.b2' + '?callback=' + callback_function_name).get :head => headers, :timeout => 30
      sub_1.callback {
        assert_equal("#{callback_function_name}\r\n([#{body}\r\n,#{body + "1"}\r\n,]);\r\n", sub_1.response, "Wrong message")
        EventMachine.stop
      }

      add_test_timeout
    }
  end

  def config_test_force_content_type_to_be_application_javascript_when_using_function_name_specified_in_callback_parameter
    @content_type = "anything/value"
  end

  def test_force_content_type_to_be_application_javascript_when_using_function_name_specified_in_callback_parameter
    headers = {'accept' => 'otherknown/value'}
    channel = 'test_force_content_type_to_be_application_javascript_when_using_function_name_specified_in_callback_parameter'
    body = 'body'
    response = ""
    callback_function_name = "callback_function"

    EventMachine.run {

      sub_1 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s + '?callback=' + callback_function_name).get :head => headers, :timeout => 30
      sub_1.callback {
        assert_equal('application/javascript', sub_1.response_header['CONTENT_TYPE'], "Didn't receive the right content type")
        EventMachine.stop
      }

      publish_message_inline(channel, {'accept' => 'text/html'}, body)

      add_test_timeout
    }
  end

end
