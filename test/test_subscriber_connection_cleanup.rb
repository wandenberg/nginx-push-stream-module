require 'rubygems'
require 'em-http'
require 'test/unit'
require File.expand_path('base_test_case', File.dirname(__FILE__))

class TestSubscriberConnectionCleanup < Test::Unit::TestCase
  include BaseTestCase

  def initialize(opts)
    super(opts)
    @test_config_file = "test_subscriber_connection_cleanup.conf"
    @subscriber_connection_timeout = "37s"
    @header_template = "HEADER_TEMPLATE"
    @ping_message_interval = "0s"
  end

  def test_subscriber_connection_cleanup
    channel = 'ch1'
    headers = {'accept' => 'text/html'}

    start = Time.now
    receivedHeaderTemplate = false
    EventMachine.run {
      http = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => headers, :timeout => 60
      http.stream { |chunk|
        assert(chunk.include?(@header_template), "Didn't received header template")
      }
      http.callback {
        stop = Time.now
        elapsed = time_diff_sec(start, stop)
        assert(elapsed >= 37 && elapsed <= 38.5, "Disconnect was in #{elapsed} seconds")
        EventMachine.stop
      }
      http.errback { |error|
        fail("Erro inexperado na execucao do teste: #{error.last_effective_url.nil? ? "" : error.last_effective_url.request_uri} #{error.response}")
      }
    }
  end
end
