require File.expand_path('base_test_case', File.dirname(__FILE__))

class TestSubscriberPolling < Test::Unit::TestCase
  include BaseTestCase

  def global_configuration
    @ping_message_interval = nil
    @header_template = nil
    @footer_template = nil
    @message_template = nil
    @subscriber_mode = 'polling'
  end

  def test_receive_a_304_when_has_no_messages
    headers = {'accept' => 'application/json'}
    channel = 'ch_test_receive_a_304_when_has_no_messages'
    body = 'body'

    EventMachine.run {
      sub_1 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => headers, :timeout => 30
      sub_1.callback {
        assert_equal(304, sub_1.response_header.status, "Wrong status")
        assert_equal("", sub_1.response_header['LAST_MODIFIED'].to_s, "Wrong header")
        assert_equal("", sub_1.response_header['ETAG'].to_s, "Wrong header")
        assert_equal(0, sub_1.response_header.content_length, "Wrong response")
        EventMachine.stop
      }

      add_test_timeout
    }
  end

  def test_receive_a_304_when_has_no_messages_keeping_headers
    headers = {'accept' => 'application/json'}
    channel = 'ch_test_receive_a_304_when_has_no_messages_keeping_headers'
    body = 'body'

    headers = headers.merge({'If-Modified-Since' => Time.now.utc.strftime("%a, %d %b %Y %T %Z"), 'If-None-Match' => '3'})
    EventMachine.run {
      sub_1 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => headers, :timeout => 30
      sub_1.callback {
        assert_equal(304, sub_1.response_header.status, "Wrong status")
        assert_equal(headers['If-Modified-Since'], sub_1.response_header['LAST_MODIFIED'].to_s, "Wrong header")
        assert_equal(headers['If-None-Match'], sub_1.response_header['ETAG'].to_s, "Wrong header")
        assert_equal(0, sub_1.response_header.content_length, "Wrong response")
        EventMachine.stop
      }

      add_test_timeout
    }
  end

  def test_receive_specific_headers_when_has_messages
    headers = {'accept' => 'application/json'}
    channel = 'ch_test_receive_specific_headers_when_has_messages'
    body = 'body'

    EventMachine.run {
      publish_message_inline(channel, {'accept' => 'text/html'}, body)

      sub_1 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => headers, :timeout => 30
      sub_1.callback {
        assert_equal(200, sub_1.response_header.status, "Wrong status")
        assert_not_equal("", sub_1.response_header['LAST_MODIFIED'].to_s, "Wrong header")
        assert_equal("0", sub_1.response_header['ETAG'].to_s, "Wrong header")
        assert_equal("#{body}\r\n", sub_1.response, "The published message was not received correctly")
        EventMachine.stop
      }

      add_test_timeout
    }
  end

  def test_receive_old_messages_by_if_modified_since_header
    headers = {'accept' => 'application/json'}
    channel = 'ch_test_getting_messages_by_if_modified_since_header'
    body = 'body'

    EventMachine.run {
      publish_message_inline(channel, {'accept' => 'text/html'}, body)

      sub_1 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => headers, :timeout => 30
      sub_1.callback {
        assert_equal(200, sub_1.response_header.status, "Wrong status")
        assert_not_equal("", sub_1.response_header['LAST_MODIFIED'].to_s, "Wrong header")
        assert_not_equal("", sub_1.response_header['ETAG'].to_s, "Wrong header")
        assert_equal("#{body}\r\n", sub_1.response, "The published message was not received correctly")

        headers.merge!({'If-Modified-Since' => sub_1.response_header['LAST_MODIFIED'], 'If-None-Match' => sub_1.response_header['ETAG']})
        sub_2 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => headers, :timeout => 30
        sub_2.callback {
          assert_equal(304, sub_2.response_header.status, "Wrong status")
          assert_equal(0, sub_2.response_header.content_length, "Wrong response")
          assert_equal(sub_1.response_header['LAST_MODIFIED'], sub_2.response_header['LAST_MODIFIED'].to_s, "Wrong header")
          assert_equal(sub_1.response_header['ETAG'], sub_2.response_header['ETAG'].to_s, "Wrong header")

          sleep(1) # to publish the second message in a different second from the first
          publish_message_inline(channel, {'accept' => 'text/html'}, body + "1")

          headers.merge!({'If-Modified-Since' => sub_2.response_header['LAST_MODIFIED'], 'If-None-Match' => sub_2.response_header['ETAG']})
          sub_3 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => headers, :timeout => 30
          sub_3.callback {
            assert_equal(200, sub_3.response_header.status, "Wrong status")
            assert_not_equal(sub_2.response_header['LAST_MODIFIED'], sub_3.response_header['LAST_MODIFIED'].to_s, "Wrong header")
            assert_equal("0", sub_3.response_header['ETAG'].to_s, "Wrong header")
            assert_equal("#{body}1\r\n", sub_3.response, "The published message was not received correctly")

            EventMachine.stop
          }
        }
      }

      add_test_timeout
    }
  end

  def test_receive_old_messages_by_backtrack
    headers = {'accept' => 'application/json'}
    channel = 'ch_test_getting_messages_by_if_modified_since_header'
    body = 'body'

    EventMachine.run {
      publish_message_inline(channel, {'accept' => 'text/html'}, body)
      publish_message_inline(channel, {'accept' => 'text/html'}, body + "1")
      publish_message_inline(channel, {'accept' => 'text/html'}, body + "2")

      sub_1 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s + '.b1').get :head => headers, :timeout => 30
      sub_1.callback {
        assert_equal(200, sub_1.response_header.status, "Wrong status")
        assert_not_equal("", sub_1.response_header['LAST_MODIFIED'].to_s, "Wrong header")
        assert_equal("2", sub_1.response_header['ETAG'].to_s, "Wrong header")
        assert_equal("#{body}2\r\n", sub_1.response, "The published message was not received correctly")

        headers.merge!({'If-Modified-Since' => sub_1.response_header['LAST_MODIFIED'], 'If-None-Match' => sub_1.response_header['ETAG']})
        sub_2 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => headers, :timeout => 30
        sub_2.callback {
          assert_equal(304, sub_2.response_header.status, "Wrong status")
          assert_equal(0, sub_2.response_header.content_length, "Wrong response")
          assert_equal(sub_1.response_header['LAST_MODIFIED'], sub_2.response_header['LAST_MODIFIED'].to_s, "Wrong header")
          assert_equal(sub_1.response_header['ETAG'], sub_2.response_header['ETAG'].to_s, "Wrong header")

          sleep(1) # to publish the second message in a different second from the first
          publish_message_inline(channel, {'accept' => 'text/html'}, body + "3")

          headers.merge!({'If-Modified-Since' => sub_2.response_header['LAST_MODIFIED'], 'If-None-Match' => sub_2.response_header['ETAG']})
          sub_3 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => headers, :timeout => 30
          sub_3.callback {
            assert_equal(200, sub_3.response_header.status, "Wrong status")
            assert_not_equal(sub_2.response_header['LAST_MODIFIED'], sub_3.response_header['LAST_MODIFIED'].to_s, "Wrong header")
            assert_equal("0", sub_3.response_header['ETAG'].to_s, "Wrong header")
            assert_equal("#{body}3\r\n", sub_3.response, "The published message was not received correctly")

            EventMachine.stop
          }
        }
      }

      add_test_timeout
    }
  end

  def test_receive_old_messages_by_last_event_id
    headers = {'accept' => 'application/json'}
    channel = 'ch_test_receive_old_messages_by_last_event_id'
    body = 'body'

    EventMachine.run {
      publish_message_inline(channel, {'accept' => 'text/html', 'Event-Id' => 'event 1' }, 'msg 1')
      publish_message_inline(channel, {'accept' => 'text/html', 'Event-Id' => 'event 2' }, 'msg 2')
      publish_message_inline(channel, {'accept' => 'text/html' }, 'msg 3')
      publish_message_inline(channel, {'accept' => 'text/html', 'Event-Id' => 'event 3' }, 'msg 4')

      sub_1 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => {'Last-Event-Id' => 'event 2' }
      sub_1.callback {
        assert_equal(200, sub_1.response_header.status, "Wrong status")
        assert_not_equal("", sub_1.response_header['LAST_MODIFIED'].to_s, "Wrong header")
        assert_equal("3", sub_1.response_header['ETAG'].to_s, "Wrong header")
        assert_equal("msg 3\r\nmsg 4\r\n", sub_1.response, "The published message was not received correctly")

        headers.merge!({'If-Modified-Since' => sub_1.response_header['LAST_MODIFIED'], 'If-None-Match' => sub_1.response_header['ETAG']})
        sub_2 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => headers, :timeout => 30
        sub_2.callback {
          assert_equal(304, sub_2.response_header.status, "Wrong status")
          assert_equal(0, sub_2.response_header.content_length, "Wrong response")
          assert_equal(sub_1.response_header['LAST_MODIFIED'], sub_2.response_header['LAST_MODIFIED'].to_s, "Wrong header")
          assert_equal(sub_1.response_header['ETAG'], sub_2.response_header['ETAG'].to_s, "Wrong header")

          sleep(1) # to publish the second message in a different second from the first
          publish_message_inline(channel, {'accept' => 'text/html'}, body + "3")

          headers.merge!({'If-Modified-Since' => sub_2.response_header['LAST_MODIFIED'], 'If-None-Match' => sub_2.response_header['ETAG']})
          sub_3 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => headers, :timeout => 30
          sub_3.callback {
            assert_equal(200, sub_3.response_header.status, "Wrong status")
            assert_not_equal(sub_2.response_header['LAST_MODIFIED'], sub_3.response_header['LAST_MODIFIED'].to_s, "Wrong header")
            assert_equal("0", sub_3.response_header['ETAG'].to_s, "Wrong header")
            assert_equal("#{body}3\r\n", sub_3.response, "The published message was not received correctly")

            EventMachine.stop
          }
        }
      }

      add_test_timeout
    }
  end

  def test_receive_old_messages_from_different_channels
    headers = {'accept' => 'application/json'}
    channel_1 = 'ch_test_receive_old_messages_from_different_channels_1'
    channel_2 = 'ch_test_receive_old_messages_from_different_channels_2'
    body = 'body'

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
          assert_equal(304, sub_2.response_header.status, "Wrong status")
          assert_equal(0, sub_2.response_header.content_length, "Wrong response")
          assert_equal(sub_1.response_header['LAST_MODIFIED'], sub_2.response_header['LAST_MODIFIED'].to_s, "Wrong header")
          assert_equal(sub_1.response_header['ETAG'], sub_2.response_header['ETAG'].to_s, "Wrong header")

          sleep(1) # to publish the second message in a different second from the first
          publish_message_inline(channel_1, {'accept' => 'text/html'}, body + "1_1")

          headers.merge!({'If-Modified-Since' => sub_2.response_header['LAST_MODIFIED'], 'If-None-Match' => sub_2.response_header['ETAG']})
          sub_3 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel_2.to_s + '/' + channel_1.to_s).get :head => headers, :timeout => 30
          sub_3.callback {
            assert_equal(200, sub_3.response_header.status, "Wrong status")
            assert_not_equal(sub_2.response_header['LAST_MODIFIED'], sub_3.response_header['LAST_MODIFIED'].to_s, "Wrong header")
            assert_equal("0", sub_3.response_header['ETAG'].to_s, "Wrong header")
            assert_equal("#{body}1_1\r\n", sub_3.response, "The published message was not received correctly")

            headers.merge!({'If-Modified-Since' => sub_3.response_header['LAST_MODIFIED'], 'If-None-Match' => sub_3.response_header['ETAG']})
            sub_4 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel_2.to_s + '/' + channel_1.to_s).get :head => headers, :timeout => 30
            sub_4.callback {
              assert_equal(304, sub_4.response_header.status, "Wrong status")
              assert_equal(0, sub_4.response_header.content_length, "Wrong response")
              assert_equal(sub_3.response_header['LAST_MODIFIED'], sub_4.response_header['LAST_MODIFIED'].to_s, "Wrong header")
              assert_equal(sub_3.response_header['ETAG'], sub_4.response_header['ETAG'].to_s, "Wrong header")

              sleep(1) # to publish the second message in a different second from the first
              publish_message_inline(channel_2, {'accept' => 'text/html'}, body + "1_2")

              headers.merge!({'If-Modified-Since' => sub_4.response_header['LAST_MODIFIED'], 'If-None-Match' => sub_4.response_header['ETAG']})
              sub_5 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel_2.to_s + '/' + channel_1.to_s).get :head => headers, :timeout => 30
              sub_5.callback {
                assert_equal(200, sub_5.response_header.status, "Wrong status")
                assert_not_equal(sub_4.response_header['LAST_MODIFIED'], sub_5.response_header['LAST_MODIFIED'].to_s, "Wrong header")
                assert_equal("0", sub_5.response_header['ETAG'].to_s, "Wrong header")
                assert_equal("#{body}1_2\r\n", sub_5.response, "The published message was not received correctly")

                EventMachine.stop
              }
            }
          }
        }
      }

      add_test_timeout
    }
  end

  def conf_test_receive_a_304_when_has_no_messages_using_push_mode_header
    @subscriber_mode = nil
  end

  def test_receive_a_304_when_has_no_messages_using_push_mode_header
    headers = {'accept' => 'application/json', 'X-Nginx-PushStream-Mode' => 'polling'}
    channel = 'ch_test_receive_a_304_when_has_no_messages_using_push_mode_header'
    body = 'body'

    EventMachine.run {
      sub_1 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => headers, :timeout => 30
      sub_1.callback {
        assert_equal(304, sub_1.response_header.status, "Wrong status")
        assert_equal("", sub_1.response_header['LAST_MODIFIED'].to_s, "Wrong header")
        assert_equal("", sub_1.response_header['ETAG'].to_s, "Wrong header")
        assert_equal(0, sub_1.response_header.content_length, "Wrong response")
        EventMachine.stop
      }

      add_test_timeout
    }
  end

  def conf_test_receive_a_304_when_has_no_messages_keeping_headers_using_push_mode_header
    @subscriber_mode = nil
  end

  def test_receive_a_304_when_has_no_messages_keeping_headers_using_push_mode_header
    headers = {'accept' => 'application/json', 'X-Nginx-PushStream-Mode' => 'polling'}
    channel = 'ch_test_receive_a_304_when_has_no_messages_keeping_headers_using_push_mode_header'
    body = 'body'

    headers = headers.merge({'If-Modified-Since' => Time.now.utc.strftime("%a, %d %b %Y %T %Z"), 'If-None-Match' => '3'})
    EventMachine.run {
      sub_1 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => headers, :timeout => 30
      sub_1.callback {
        assert_equal(304, sub_1.response_header.status, "Wrong status")
        assert_equal(headers['If-Modified-Since'], sub_1.response_header['LAST_MODIFIED'].to_s, "Wrong header")
        assert_equal(headers['If-None-Match'], sub_1.response_header['ETAG'].to_s, "Wrong header")
        assert_equal(0, sub_1.response_header.content_length, "Wrong response")
        EventMachine.stop
      }

      add_test_timeout
    }
  end

  def conf_test_receive_specific_headers_when_has_messages_using_push_mode_header
    @subscriber_mode = nil
  end

  def test_receive_specific_headers_when_has_messages_using_push_mode_header
    headers = {'accept' => 'application/json', 'X-Nginx-PushStream-Mode' => 'polling'}
    channel = 'ch_test_receive_specific_headers_when_has_messages_using_push_mode_header'
    body = 'body'

    EventMachine.run {
      publish_message_inline(channel, {'accept' => 'text/html'}, body)

      sub_1 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => headers, :timeout => 30
      sub_1.callback {
        assert_equal(200, sub_1.response_header.status, "Wrong status")
        assert_not_equal("", sub_1.response_header['LAST_MODIFIED'].to_s, "Wrong header")
        assert_equal("0", sub_1.response_header['ETAG'].to_s, "Wrong header")
        assert_equal("#{body}\r\n", sub_1.response, "The published message was not received correctly")
        EventMachine.stop
      }

      add_test_timeout
    }
  end

  def conf_test_receive_old_messages_by_if_modified_since_header_using_push_mode_header
    @subscriber_mode = nil
  end

  def test_receive_old_messages_by_if_modified_since_header_using_push_mode_header
    headers = {'accept' => 'application/json', 'X-Nginx-PushStream-Mode' => 'polling'}
    channel = 'ch_test_getting_messages_by_if_modified_since_header_using_push_mode_header'
    body = 'body'

    EventMachine.run {
      publish_message_inline(channel, {'accept' => 'text/html'}, body)

      sub_1 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => headers, :timeout => 30
      sub_1.callback {
        assert_equal(200, sub_1.response_header.status, "Wrong status")
        assert_not_equal("", sub_1.response_header['LAST_MODIFIED'].to_s, "Wrong header")
        assert_not_equal("", sub_1.response_header['ETAG'].to_s, "Wrong header")
        assert_equal("#{body}\r\n", sub_1.response, "The published message was not received correctly")

        headers.merge!({'If-Modified-Since' => sub_1.response_header['LAST_MODIFIED'], 'If-None-Match' => sub_1.response_header['ETAG']})
        sub_2 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => headers, :timeout => 30
        sub_2.callback {
          assert_equal(304, sub_2.response_header.status, "Wrong status")
          assert_equal(0, sub_2.response_header.content_length, "Wrong response")
          assert_equal(sub_1.response_header['LAST_MODIFIED'], sub_2.response_header['LAST_MODIFIED'].to_s, "Wrong header")
          assert_equal(sub_1.response_header['ETAG'], sub_2.response_header['ETAG'].to_s, "Wrong header")

          sleep(1) # to publish the second message in a different second from the first
          publish_message_inline(channel, {'accept' => 'text/html'}, body + "1")

          headers.merge!({'If-Modified-Since' => sub_2.response_header['LAST_MODIFIED'], 'If-None-Match' => sub_2.response_header['ETAG']})
          sub_3 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => headers, :timeout => 30
          sub_3.callback {
            assert_equal(200, sub_3.response_header.status, "Wrong status")
            assert_not_equal(sub_2.response_header['LAST_MODIFIED'], sub_3.response_header['LAST_MODIFIED'].to_s, "Wrong header")
            assert_equal("0", sub_3.response_header['ETAG'].to_s, "Wrong header")
            assert_equal("#{body}1\r\n", sub_3.response, "The published message was not received correctly")

            EventMachine.stop
          }
        }
      }

      add_test_timeout
    }
  end

  def conf_test_receive_old_messages_by_backtrack_using_push_mode_header
    @subscriber_mode = nil
  end

  def test_receive_old_messages_by_backtrack_using_push_mode_header
    headers = {'accept' => 'application/json', 'X-Nginx-PushStream-Mode' => 'polling'}
    channel = 'ch_test_getting_messages_by_if_modified_since_header_using_push_mode_header'
    body = 'body'

    EventMachine.run {
      publish_message_inline(channel, {'accept' => 'text/html'}, body)
      publish_message_inline(channel, {'accept' => 'text/html'}, body + "1")
      publish_message_inline(channel, {'accept' => 'text/html'}, body + "2")

      sub_1 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s + '.b1').get :head => headers, :timeout => 30
      sub_1.callback {
        assert_equal(200, sub_1.response_header.status, "Wrong status")
        assert_not_equal("", sub_1.response_header['LAST_MODIFIED'].to_s, "Wrong header")
        assert_equal("2", sub_1.response_header['ETAG'].to_s, "Wrong header")
        assert_equal("#{body}2\r\n", sub_1.response, "The published message was not received correctly")

        headers.merge!({'If-Modified-Since' => sub_1.response_header['LAST_MODIFIED'], 'If-None-Match' => sub_1.response_header['ETAG']})
        sub_2 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => headers, :timeout => 30
        sub_2.callback {
          assert_equal(304, sub_2.response_header.status, "Wrong status")
          assert_equal(0, sub_2.response_header.content_length, "Wrong response")
          assert_equal(sub_1.response_header['LAST_MODIFIED'], sub_2.response_header['LAST_MODIFIED'].to_s, "Wrong header")
          assert_equal(sub_1.response_header['ETAG'], sub_2.response_header['ETAG'].to_s, "Wrong header")

          sleep(1) # to publish the second message in a different second from the first
          publish_message_inline(channel, {'accept' => 'text/html'}, body + "3")

          headers.merge!({'If-Modified-Since' => sub_2.response_header['LAST_MODIFIED'], 'If-None-Match' => sub_2.response_header['ETAG']})
          sub_3 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => headers, :timeout => 30
          sub_3.callback {
            assert_equal(200, sub_3.response_header.status, "Wrong status")
            assert_not_equal(sub_2.response_header['LAST_MODIFIED'], sub_3.response_header['LAST_MODIFIED'].to_s, "Wrong header")
            assert_equal("0", sub_3.response_header['ETAG'].to_s, "Wrong header")
            assert_equal("#{body}3\r\n", sub_3.response, "The published message was not received correctly")

            EventMachine.stop
          }
        }
      }

      add_test_timeout
    }
  end

  def conf_test_receive_old_messages_by_last_event_id_using_push_mode_header
    @subscriber_mode = nil
  end

  def test_receive_old_messages_by_last_event_id_using_push_mode_header
    headers = {'accept' => 'application/json', 'X-Nginx-PushStream-Mode' => 'polling'}
    channel = 'ch_test_receive_old_messages_by_last_event_id_using_push_mode_header'
    body = 'body'

    EventMachine.run {
      publish_message_inline(channel, {'accept' => 'text/html', 'Event-Id' => 'event 1' }, 'msg 1')
      publish_message_inline(channel, {'accept' => 'text/html', 'Event-Id' => 'event 2' }, 'msg 2')
      publish_message_inline(channel, {'accept' => 'text/html' }, 'msg 3')
      publish_message_inline(channel, {'accept' => 'text/html', 'Event-Id' => 'event 3' }, 'msg 4')

      sub_1 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => headers.merge({'Last-Event-Id' => 'event 2'})
      sub_1.callback {
        assert_equal(200, sub_1.response_header.status, "Wrong status")
        assert_not_equal("", sub_1.response_header['LAST_MODIFIED'].to_s, "Wrong header")
        assert_equal("3", sub_1.response_header['ETAG'].to_s, "Wrong header")
        assert_equal("msg 3\r\nmsg 4\r\n", sub_1.response, "The published message was not received correctly")

        headers.merge!({'If-Modified-Since' => sub_1.response_header['LAST_MODIFIED'], 'If-None-Match' => sub_1.response_header['ETAG']})
        sub_2 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => headers, :timeout => 30
        sub_2.callback {
          assert_equal(304, sub_2.response_header.status, "Wrong status")
          assert_equal(0, sub_2.response_header.content_length, "Wrong response")
          assert_equal(sub_1.response_header['LAST_MODIFIED'], sub_2.response_header['LAST_MODIFIED'].to_s, "Wrong header")
          assert_equal(sub_1.response_header['ETAG'], sub_2.response_header['ETAG'].to_s, "Wrong header")

          sleep(1) # to publish the second message in a different second from the first
          publish_message_inline(channel, {'accept' => 'text/html'}, body + "3")

          headers.merge!({'If-Modified-Since' => sub_2.response_header['LAST_MODIFIED'], 'If-None-Match' => sub_2.response_header['ETAG']})
          sub_3 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => headers, :timeout => 30
          sub_3.callback {
            assert_equal(200, sub_3.response_header.status, "Wrong status")
            assert_not_equal(sub_2.response_header['LAST_MODIFIED'], sub_3.response_header['LAST_MODIFIED'].to_s, "Wrong header")
            assert_equal("0", sub_3.response_header['ETAG'].to_s, "Wrong header")
            assert_equal("#{body}3\r\n", sub_3.response, "The published message was not received correctly")

            EventMachine.stop
          }
        }
      }

      add_test_timeout
    }
  end

  def conf_test_receive_old_messages_from_different_channels_using_push_mode_header
    @subscriber_mode = nil
  end

  def test_receive_old_messages_from_different_channels_using_push_mode_header
    headers = {'accept' => 'application/json', 'X-Nginx-PushStream-Mode' => 'polling'}
    channel_1 = 'ch_test_receive_old_messages_from_different_channels_using_push_mode_header_1'
    channel_2 = 'ch_test_receive_old_messages_from_different_channels_using_push_mode_header_2'
    body = 'body'

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
          assert_equal(304, sub_2.response_header.status, "Wrong status")
          assert_equal(0, sub_2.response_header.content_length, "Wrong response")
          assert_equal(sub_1.response_header['LAST_MODIFIED'], sub_2.response_header['LAST_MODIFIED'].to_s, "Wrong header")
          assert_equal(sub_1.response_header['ETAG'], sub_2.response_header['ETAG'].to_s, "Wrong header")

          sleep(1) # to publish the second message in a different second from the first
          publish_message_inline(channel_1, {'accept' => 'text/html'}, body + "1_1")

          headers.merge!({'If-Modified-Since' => sub_2.response_header['LAST_MODIFIED'], 'If-None-Match' => sub_2.response_header['ETAG']})
          sub_3 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel_2.to_s + '/' + channel_1.to_s).get :head => headers, :timeout => 30
          sub_3.callback {
            assert_equal(200, sub_3.response_header.status, "Wrong status")
            assert_not_equal(sub_2.response_header['LAST_MODIFIED'], sub_3.response_header['LAST_MODIFIED'].to_s, "Wrong header")
            assert_equal("0", sub_3.response_header['ETAG'].to_s, "Wrong header")
            assert_equal("#{body}1_1\r\n", sub_3.response, "The published message was not received correctly")

            headers.merge!({'If-Modified-Since' => sub_3.response_header['LAST_MODIFIED'], 'If-None-Match' => sub_3.response_header['ETAG']})
            sub_4 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel_2.to_s + '/' + channel_1.to_s).get :head => headers, :timeout => 30
            sub_4.callback {
              assert_equal(304, sub_4.response_header.status, "Wrong status")
              assert_equal(0, sub_4.response_header.content_length, "Wrong response")
              assert_equal(sub_3.response_header['LAST_MODIFIED'], sub_4.response_header['LAST_MODIFIED'].to_s, "Wrong header")
              assert_equal(sub_3.response_header['ETAG'], sub_4.response_header['ETAG'].to_s, "Wrong header")

              sleep(1) # to publish the second message in a different second from the first
              publish_message_inline(channel_2, {'accept' => 'text/html'}, body + "1_2")

              headers.merge!({'If-Modified-Since' => sub_4.response_header['LAST_MODIFIED'], 'If-None-Match' => sub_4.response_header['ETAG']})
              sub_5 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel_2.to_s + '/' + channel_1.to_s).get :head => headers, :timeout => 30
              sub_5.callback {
                assert_equal(200, sub_5.response_header.status, "Wrong status")
                assert_not_equal(sub_4.response_header['LAST_MODIFIED'], sub_5.response_header['LAST_MODIFIED'].to_s, "Wrong header")
                assert_equal("0", sub_5.response_header['ETAG'].to_s, "Wrong header")
                assert_equal("#{body}1_2\r\n", sub_5.response, "The published message was not received correctly")

                EventMachine.stop
              }
            }
          }
        }
      }

      add_test_timeout
    }
  end

  def config_test_send_modified_since_and_none_match_values_not_using_headers_when_polling
    @last_received_message_time = "$arg_time"
    @last_received_message_tag = "$arg_tag"
  end

  def test_send_modified_since_and_none_match_values_not_using_headers_when_polling
    headers = {'accept' => 'application/json'}
    channel = 'ch_test_send_modified_since_and_none_match_values_not_using_headers_when_polling'
    body = 'body'
    response = ""

    EventMachine.run {
      publish_message_inline(channel, {'accept' => 'text/html'}, body)

      sub_1 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => headers, :timeout => 30
      sub_1.callback {
        assert_equal("#{body}\r\n", sub_1.response, "Wrong message")

        time = sub_1.response_header['LAST_MODIFIED']
        tag = sub_1.response_header['ETAG']

        publish_message_inline(channel, {'accept' => 'text/html'}, body + " 1")

        response = ""
        sub_2 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s + '?time=' + time + '&tag=' + tag).get :head => headers, :timeout => 30
        sub_2.callback {
          assert_equal("#{body} 1\r\n", sub_2.response, "Wrong message")
          EventMachine.stop
        }

      }

      add_test_timeout
    }
  end

  def test_return_message_using_function_name_specified_in_callback_parameter_when_polling
    headers = {'accept' => 'application/javascript'}
    channel = 'ch_test_return_message_using_function_name_specified_in_callback_parameter_when_polling'
    body = 'body'
    response = ""
    callback_function_name = "callback_function"

    EventMachine.run {
      publish_message_inline(channel, {'accept' => 'text/html'}, body)

      sub_1 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s + '?callback=' + callback_function_name).get :head => headers, :timeout => 30
      sub_1.callback {
        assert_equal("#{callback_function_name}\r\n([#{body}\r\n,]);\r\n", sub_1.response, "Wrong message")
        EventMachine.stop
      }

      add_test_timeout
    }
  end

  def config_test_force_content_type_to_be_application_javascript_when_using_function_name_specified_in_callback_parameter_when_polling
    @content_type = "anything/value"
  end

  def test_force_content_type_to_be_application_javascript_when_using_function_name_specified_in_callback_parameter_when_polling
    headers = {'accept' => 'otherknown/value'}
    channel = 'test_force_content_type_to_be_application_javascript_when_using_function_name_specified_in_callback_parameter_when_polling'
    body = 'body'
    response = ""
    callback_function_name = "callback_function"

    EventMachine.run {
      publish_message_inline(channel, {'accept' => 'text/html'}, body)

      sub_1 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s + '?callback=' + callback_function_name).get :head => headers, :timeout => 30
      sub_1.callback {
        assert_equal('application/javascript', sub_1.response_header['CONTENT_TYPE'], "Didn't receive the right content type")
        EventMachine.stop
      }

      add_test_timeout
    }
  end

end
