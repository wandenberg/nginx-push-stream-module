require 'rubygems'
require 'popen4'
require 'em-http'
require 'test/unit'
require 'json'
require File.expand_path('base_test_case', File.dirname(__FILE__))

class TestPublisherProperties < Test::Unit::TestCase
  include BaseTestCase

  def initialize(opts)
    super(opts)
    @header_template = ""
    @message_template = "~text~"
  end

  def config_test_stored_messages
    @test_config_file = "test_store_messages.conf"
    @store_messages = "on"
  end

  def test_stored_messages
    headers = {'accept' => 'application/json'}
    body = 'published message'
    channel = 'ch1'

    EventMachine.run {
      pub_1 = EventMachine::HttpRequest.new(nginx_address + '/pub?id=' + channel.to_s ).post :head => headers, :body => body, :timeout => 30
      pub_1.callback {
        response = JSON.parse(pub_1.response)
        assert_equal(1, response["stored_messages"].to_i, "Not stored messages")

        pub_2 = EventMachine::HttpRequest.new(nginx_address + '/pub?id=' + channel.to_s ).post :head => headers, :body => body, :timeout => 30
        pub_2.callback {
          response = JSON.parse(pub_2.response)
          assert_equal(2, response["stored_messages"].to_i, "Not stored messages")
          EventMachine.stop
        }
        fail_if_connecttion_error(pub_2)
      }
      fail_if_connecttion_error(pub_1)
    }
  end

  def config_test_not_stored_messages
    @test_config_file = "test_not_store_messages.conf"
    @store_messages = "off"
  end

  def test_not_stored_messages
    headers = {'accept' => 'application/json'}
    body = 'published message'
    channel = 'ch2'

    EventMachine.run {
      pub = EventMachine::HttpRequest.new(nginx_address + '/pub?id=' + channel.to_s ).post :head => headers, :body => body, :timeout => 30
      pub.callback {
        response = JSON.parse(pub.response)
        assert_equal(0, response["stored_messages"].to_i, "Stored messages")
        EventMachine.stop
      }
      fail_if_connecttion_error(pub)
    }
  end

  def config_test_max_stored_messages
    @test_config_file = "test_max_stored_messages.conf"
    @store_messages = "on"
    @max_message_buffer_length = 4
  end

  def test_max_stored_messages
    headers = {'accept' => 'application/json'}
    body_prefix = 'published message '
    channel = 'ch3'
    messagens_to_publish = 10

    EventMachine.run {

      i = 0
      EM.add_periodic_timer(0.05) do
        i += 1
        if i <= messagens_to_publish
          pub = EventMachine::HttpRequest.new(nginx_address + '/pub?id=' + channel.to_s ).post :head => headers, :body => body_prefix + i.to_s, :timeout => 30
          pub.callback {
            response = JSON.parse(pub.response)
            assert(response["stored_messages"].to_i <= @max_message_buffer_length, "Stored more messages then configured")
          }
          fail_if_connecttion_error(pub)
        else
          EventMachine.stop
        end
      end
    }
  end

  def config_test_max_channel_id_length
    @test_config_file = "test_max_channel_id_length.conf"
    @max_channel_id_length = 5
  end

  def test_max_channel_id_length
    headers = {'accept' => 'application/json'}
    body = 'published message'
    channel = '123456'

    EventMachine.run {
      pub = EventMachine::HttpRequest.new(nginx_address + '/pub?id=' + channel.to_s ).post :head => headers, :body => body, :timeout => 30
      pub.callback {
        response = JSON.parse(pub.response)
        assert_equal("12345", response["channel"], "No crop the channel id")
        EventMachine.stop
      }
      fail_if_connecttion_error(pub)
    }
  end

end
