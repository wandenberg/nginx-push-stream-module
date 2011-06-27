require File.expand_path('base_test_case', File.dirname(__FILE__))

class TestBroadcastProperties < Test::Unit::TestCase
  include BaseTestCase

  def config_test_broadcast_channel_prefix
    @authorized_channels_only = "on"
    @header_template = "connected"
    @broadcast_channel_prefix = "XXX_"
  end

  def test_broadcast_channel_prefix
    channel = 'ch_test_broadcast_channel_prefix'
    channel_broad = 'XXX_123'
    channel_broad_fail = 'YYY_123'
    headers = {'accept' => 'text/html'}
    body = 'broadcast channel prefix'

    EventMachine.run {
      pub = EventMachine::HttpRequest.new(nginx_address + '/pub?id=' + channel.to_s ).post :head => headers, :body => body, :timeout => 30
      pub.callback {
        sub_1 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s + '/' + channel_broad_fail).get :head => headers, :timeout => 60
        sub_1.callback { |chunk|
          assert_equal(403, sub_1.response_header.status, "Subscriber was not forbidden")

          sub_2 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s + '/' + channel_broad).get :head => headers, :timeout => 60
          sub_2.stream { |chunk|
            assert_equal("#{@header_template}\r\n", chunk, "Didn't received header template")
            EventMachine.stop
          }
        }
      }
    }
  end

  def config_test_broadcast_channel_max_qtd
    @authorized_channels_only = "on"
    @header_template = "connected"
    @broadcast_channel_prefix = "XXX_"
    @broadcast_channel_max_qtd = 2
  end

  def test_broadcast_channel_max_qtd
    channel = 'ch_test_broadcast_channel_max_qtd'
    channel_broad1 = 'XXX_123'
    channel_broad2 = 'XXX_321'
    channel_broad3 = 'XXX_213'
    headers = {'accept' => 'text/html'}
    body = 'broadcast channel prefix'

    EventMachine.run {
      pub = EventMachine::HttpRequest.new(nginx_address + '/pub?id=' + channel.to_s ).post :head => headers, :body => body, :timeout => 30
      pub.callback {
        sub_1 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s + '/' + channel_broad1 + '/' + channel_broad2  + '/' + channel_broad3).get :head => headers, :timeout => 60
        sub_1.callback { |chunk|
          assert_equal(403, sub_1.response_header.status, "Subscriber was not forbidden")
          sub_2 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s + '/' + channel_broad1 + '/' + channel_broad2).get :head => headers, :timeout => 60
          sub_2.stream { |chunk|
            EventMachine.stop
          }
        }
      }
    }
  end
end
