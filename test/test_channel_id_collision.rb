require File.expand_path('base_test_case', File.dirname(__FILE__))

class TestChannelIdCollision < Test::Unit::TestCase
  include BaseTestCase

  def test_create_and_retrieve_channels_with_ids_that_collide
    channels = ["A", "plumless", "buckeroo", "B", "fc0591", "123rainerbommert", "C", "a1sellers", "advertees", "D"]

    channels.each do |channel|
      EventMachine.run {
        pub = EventMachine::HttpRequest.new(nginx_address + '/pub?id=' + channel).post :body => 'x', :timeout => 30
        pub.callback {
          assert_equal(200, pub.response_header.status, "Channel '#{channel}' was not created")
          EventMachine.stop
        }
      }
    end

    channels.each do |channel|
      EventMachine.run {
        pub = EventMachine::HttpRequest.new(nginx_address + '/channels-stats?id=' + channel).get :timeout => 30
        pub.callback {
          assert_equal(200, pub.response_header.status, "Channel '#{channel}' was not founded")
          EventMachine.stop
        }
      }
    end
  end
end
