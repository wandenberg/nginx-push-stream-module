require File.expand_path('base_test_case', File.dirname(__FILE__))

class TestCreateManyChannels < Test::Unit::TestCase
  include BaseTestCase

  def config_test_create_many_channels
    @max_reserved_memory = "256m"
  end

  def test_create_many_channels
    headers = {'accept' => 'application/json'}
    body = 'channel started'
    channels_to_be_created = 4000

    EventMachine.run {
      i = 0
      EM.add_periodic_timer(0.001) do
        i += 1
        if i <= channels_to_be_created
          pub = EventMachine::HttpRequest.new(nginx_address + '/pub?id=ch_test_create_many_channels_' + i.to_s ).post :head => headers, :body => body, :timeout => 30
          pub.callback {
            if pub.response_header.status != 200
              assert_equal(200, pub.response_header.status, "Channel was not created: ch_test_create_many_channels_" + i.to_s)
            end
          }
        else
          EventMachine.stop
        end
      end
    }
  end
end
