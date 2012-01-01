require File.expand_path('base_test_case', File.dirname(__FILE__))

class TestMeasureMemory < Test::Unit::TestCase
  include BaseTestCase

  @@message_estimate_size = 200
  @@channel_estimate_size = 536
  @@subscriber_estimate_size = 230
  @@subscriber_estimate_system_size = 6500

  def global_configuration
    @max_reserved_memory = "2m"
    @memory_cleanup_timeout = "60m"
    @min_message_buffer_timeout = "60m"
    @max_message_buffer_length = nil
    @keepalive = "on"
    @header_template = nil
    @message_template = nil
    @footer_template = nil
    @ping_message_interval = nil
  end

  def test_message_size
    channel = 'ch_test_message_size'
    headers = {'accept' => 'text/html'}
    body = '1'

    shared_size = @max_reserved_memory.to_i * 1024 * 1024
    expected_message = shared_size / (@@message_estimate_size + body.size)

    EventMachine.run {
      EM.add_periodic_timer(0.0001) do
        pub_1 = EventMachine::HttpRequest.new(nginx_address + '/pub?id=' + channel.to_s).post :head => headers, :body => body, :timeout => 60
        pub_1.callback {
          EventMachine.stop if pub_1.response_header.status == 500
        }
      end
    }

    EventMachine.run {
      pub_2 = EventMachine::HttpRequest.new(nginx_address + '/channels-stats?id=' + channel.to_s).get :head => headers, :timeout => 60
      pub_2.callback {
        assert_equal(200, pub_2.response_header.status, "Don't get channels statistics")
        assert_not_equal(0, pub_2.response_header.content_length, "Don't received channels statistics")
        published_messages = JSON.parse(pub_2.response)["published_messages"].to_i

        assert(((expected_message - 10) < published_messages) && (published_messages < (expected_message + 10)), "Message size is far from %d bytes (expected: %d, published: %d)"  % ([@@message_estimate_size, expected_message, published_messages]))
        EventMachine.stop
      }
    }
  end

  def test_channel_size
    headers = {'accept' => 'text/html'}
    body = '1'

    shared_size = @max_reserved_memory.to_i * 1024 * 1024
    expected_channel = shared_size / (@@message_estimate_size + body.size + @@channel_estimate_size + 4) # 4 channel id size

    EventMachine.run {
      publish_message_in_loop(1000, headers, body)
    }

    EventMachine.run {
      pub_2 = EventMachine::HttpRequest.new(nginx_address + '/channels-stats').get :head => headers, :timeout => 60
      pub_2.callback {
        assert_equal(200, pub_2.response_header.status, "Don't get channels statistics")
        assert_not_equal(0, pub_2.response_header.content_length, "Don't received channels statistics")
        created_channels = JSON.parse(pub_2.response)["channels"].to_i

        assert(((expected_channel - 10) < created_channels) && (created_channels < (expected_channel + 10)), "Channel size is far from %d bytes (expected: %d, created: %d)"  % ([@@channel_estimate_size, expected_channel, created_channels]))
        EventMachine.stop
      }
    }
  end

  def config_test_subscriber_size
    @max_reserved_memory = "300k"
    @header_template = "H"
  end

  def test_subscriber_size
    headers = {'accept' => 'text/html'}
    body = '1'

    shared_size = @max_reserved_memory.to_i * 1024
    expected_subscriber = shared_size / (@@subscriber_estimate_size + @@channel_estimate_size + 4)

    EventMachine.run {
      subscriber_in_loop(1000, headers, body) do
        pub_2 = EventMachine::HttpRequest.new(nginx_address + '/channels-stats').get :head => headers, :timeout => 60
        pub_2.callback {
          assert_equal(200, pub_2.response_header.status, "Don't get channels statistics")
          assert_not_equal(0, pub_2.response_header.content_length, "Don't received channels statistics")
          created_subscriber = JSON.parse(pub_2.response)["subscribers"].to_i

          assert(((expected_subscriber - 10) < created_subscriber) && (created_subscriber < (expected_subscriber + 10)), "Subscriber size is far from %d bytes (expected: %d, created: %d)"  % ([@@subscriber_estimate_size, expected_subscriber, created_subscriber]))
          EventMachine.stop
        }
      end
    }
  end

  def config_test_subscriber_system_size
    @header_template = "H"
  end

  def test_subscriber_system_size
    headers = {'accept' => 'text/html'}
    body = '1'

    shared_size = @max_reserved_memory.to_i * 1024
    expected_subscriber = shared_size / (@@subscriber_estimate_size + @@channel_estimate_size + 4)

    EventMachine.run {
      memory_1 = `ps -eo vsz,cmd | grep -E 'ngin[xX] -c '`.split(' ')[0].to_i
      subscriber_in_loop_with_limit(1000, headers, body, 1199) do
        memory_2 = `ps -eo vsz,cmd | grep -E 'ngin[xX] -c '`.split(' ')[0].to_i

        per_subscriber = ((memory_2 - memory_1).to_f / 200) * 1000

        assert(((@@subscriber_estimate_system_size - 100) < per_subscriber) && (per_subscriber < (@@subscriber_estimate_system_size + 100)), "Subscriber system size is far from %d bytes (measured: %d)"  % ([@@subscriber_estimate_system_size, per_subscriber]))

        EventMachine.stop
      end
    }
  end

  def subscriber_in_loop(channel, headers, body, &block)
    sub = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_i.to_s).get :head => headers, :body => body, :timeout => 30
    sub.stream { |chunk|
      subscriber_in_loop(channel.to_i + 1, headers, body) do
        yield block
      end
    }
    sub.callback {
      block.call
    }
  end

  def subscriber_in_loop_with_limit(channel, headers, body, limit, &block)
    sub = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_i.to_s).get :head => headers, :body => body, :timeout => 30
    sub.stream { |chunk|
      if channel == limit
        block.call
        EventMachine.stop
      end
      subscriber_in_loop_with_limit(channel.to_i + 1, headers, body, limit) do
        yield block
      end
    }
    sub.callback {
      block.call
    }
  end

  def publish_message_in_loop(channel, headers, body)
    pub = EventMachine::HttpRequest.new(nginx_address + '/pub?id=' + channel.to_s).post :head => headers, :body => body, :timeout => 30
    pub.callback {
      EventMachine.stop if pub.response_header.status != 200
      publish_message_in_loop(channel.to_i + 1, headers, body)
    }
  end
end

