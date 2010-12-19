require 'rubygems'
require 'popen4'
require 'em-http'
require 'test/unit'
require File.expand_path('base_test_case', File.dirname(__FILE__))

class TestPublishMessages < Test::Unit::TestCase
  include BaseTestCase

  def initialize(opts)
    super(opts)
    @test_config_file = "test_publish_messages.conf"
    @header_template = ""
    @message_template = "~text~"
  end

  def test_publish_messages
    headers = {'accept' => 'text/html'}
    body = 'published unique message'
    channel = 'ch1'

    EventMachine.run {
      sub = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => headers
      sub.stream { | chunk |
        assert_equal(body + "\r\n", chunk, "The published message was not received correctly")
        EventMachine.stop
      }
      sub.errback { |error|
        fail("Erro inexperado na execucao do teste: #{error.last_effective_url.nil? ? "" : error.last_effective_url.request_uri} #{error.response}")
      }

      pub = EventMachine::HttpRequest.new(nginx_address + '/pub?id=' + channel.to_s ).post :head => headers, :body => body, :timeout => 30
      pub.errback { |error|
        fail("Erro inexperado na execucao do teste: #{error.last_effective_url.nil? ? "" : error.last_effective_url.request_uri} #{error.response}")
      }
    }
  end

  def test_publish_many_messages_in_the_same_channel
    headers = {'accept' => 'text/html'}
    body_prefix = 'published message '
    channel = 'ch2'
    messagens_to_publish = 400
    recieved_messages = 0

    EventMachine.run {
      sub = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => headers
      sub.stream { | chunk |
        chunk.each {|s|
          if s.chomp and s.chomp != ""
            recieved_messages +=1
          end
        }

        if chunk.include?(body_prefix + messagens_to_publish.to_s + "\r\n")
          EventMachine.stop
        end
      }
      sub.callback {
        assert_equal(messagens_to_publish, recieved_messages, "The published messages was not received correctly")
      }
      sub.errback { |error|
        fail("Erro inexperado na execucao do teste: #{error.last_effective_url.nil? ? "" : error.last_effective_url.request_uri} #{error.response}")
      }

      i = 0
      EM.add_periodic_timer(0.05) do
        i += 1
        if i <= messagens_to_publish
          pub = EventMachine::HttpRequest.new(nginx_address + '/pub?id=' + channel.to_s ).post :head => headers, :body => body_prefix + i.to_s, :timeout => 30
          pub.errback { |error|
            fail("Erro inexperado na execucao do teste: #{error.last_effective_url.nil? ? "" : error.last_effective_url.request_uri} #{error.response}")
          }
        end
      end
    }
  end
end
