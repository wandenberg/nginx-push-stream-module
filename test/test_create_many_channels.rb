require 'rubygems'
require 'popen4'
require 'em-http'
require 'test/unit'
require File.expand_path('base_test_case', File.dirname(__FILE__))

class TestCreateManyChannels < Test::Unit::TestCase
  include BaseTestCase

  def initialize(opts)
    super(opts)
    @test_config_file = "test_create_many_channels.conf"
    @max_reserved_memory = "256m"
  end

  def test_create_many_channels
    headers = {'accept' => 'application/json'}
    body = 'channel started'
    channels_to_be_created = 200
    channels_callback = 0;

    EventMachine.run {
      i = 0
      EM.add_periodic_timer(0.05) do
        i += 1
        if i <= channels_to_be_created
          pub = EventMachine::HttpRequest.new(nginx_address + '/pub?id=ch' + i.to_s ).post :head => headers, :body => body, :timeout => 30
          pub.callback {
            channels_callback += 1
          }
          fail_if_connecttion_error(pub)
        else
          EventMachine.stop
        end
      end
    }
  end
end
