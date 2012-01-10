require File.expand_path('base_test_case', File.dirname(__FILE__))

class TestPaddingByUserAgent < Test::Unit::TestCase
  include BaseTestCase

  def global_configuration
    @padding_by_user_agent = "[T|t]est 1,1024,512:[T|t]est 2,4097,0"
    @user_agent = nil
    @subscriber_connection_timeout = '1s'
    @header_template = nil
    @message_template = nil
    @footer_template = nil
  end

  def config_test_header_padding
    @header_template = "0123456789"
  end

  def test_header_padding
    headers = {'accept' => 'application/json'}
    channel = 'ch_test_header_padding'

    EventMachine.run {
      sub_1 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => headers.merge("User-Agent" => "Test 1"), :timeout => 30
      sub_1.callback {
        assert_equal(200, sub_1.response_header.status, "Channel was founded")
        assert_equal(1100 + @header_template.size + 4, sub_1.response.size, "Didn't received headder with padding")

        sub_2 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => headers.merge("User-Agent" => "Test 2"), :timeout => 30
        sub_2.callback {
          assert_equal(200, sub_2.response_header.status, "Channel was founded")
          assert_equal(4097 + @header_template.size + 4, sub_2.response.size, "Didn't received headder with padding")

          sub_3 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => headers.merge("User-Agent" => "Test 3"), :timeout => 30
          sub_3.callback {
            assert_equal(200, sub_3.response_header.status, "Channel was founded")
            assert_equal(@header_template.size + 2, sub_3.response.size, "Didn't received headder with padding")

            EventMachine.stop
          }
        }
      }
      add_test_timeout
    }
  end

  def test_message_padding
    headers = {'accept' => 'application/json'}
    channel = 'ch_test_message_padding'

    body = "0123456789"

    EventMachine.run {
      sub_1 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => headers.merge("User-Agent" => "Test 1"), :timeout => 30
      sub_1.callback {
        assert_equal(200, sub_1.response_header.status, "Channel was founded")
        assert_equal(500 + body.size + 4, sub_1.response.size, "Didn't received headder with padding")

        sub_2 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => headers.merge("User-Agent" => "Test 2"), :timeout => 30
        sub_2.callback {
          assert_equal(200, sub_2.response_header.status, "Channel was founded")
          assert_equal(body.size + 2, sub_2.response.size, "Didn't received headder with padding")

          sub_3 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => headers.merge("User-Agent" => "Test 3"), :timeout => 30
          sub_3.callback {
            assert_equal(200, sub_3.response_header.status, "Channel was founded")
            assert_equal(body.size + 2, sub_3.response.size, "Didn't received headder with padding")

            EventMachine.stop
          }
          publish_message_inline(channel, headers, body)
        }
        publish_message_inline(channel, headers, body)
      }
      publish_message_inline(channel, headers, body)

      add_test_timeout
    }
  end

  def config_test_message_padding_with_different_sizes
    @padding_by_user_agent = "[T|t]est 1,0,545"
  end

  def test_message_padding_with_different_sizes
    headers = {'accept' => 'application/json'}
    channel = 'ch_test_message_padding_with_different_sizes'

    EventMachine.run {
      i = 1
      expected_padding = 545

      sub_1 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => headers.merge("User-Agent" => "Test 1"), :timeout => 30
      sub_1.callback {
        assert_equal(200, sub_1.response_header.status, "Channel was founded")
        assert_equal(expected_padding + i + 4, sub_1.response.size, "Didn't received headder with padding")

        i = 105
        expected_padding = 600 - ((i/100).to_i * 100)

        sub_1 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => headers.merge("User-Agent" => "Test 1"), :timeout => 30
        sub_1.callback {
          assert_equal(200, sub_1.response_header.status, "Channel was founded")
          assert_equal(expected_padding + i + 4, sub_1.response.size, "Didn't received headder with padding")

          i = 221
          expected_padding = 600 - ((i/100).to_i * 100)

          sub_1 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => headers.merge("User-Agent" => "Test 1"), :timeout => 30
          sub_1.callback {
            assert_equal(200, sub_1.response_header.status, "Channel was founded")
            assert_equal(expected_padding + i + 4, sub_1.response.size, "Didn't received headder with padding")

            i = 331
            expected_padding = 600 - ((i/100).to_i * 100)

            sub_1 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => headers.merge("User-Agent" => "Test 1"), :timeout => 30
            sub_1.callback {
              assert_equal(200, sub_1.response_header.status, "Channel was founded")
              assert_equal(expected_padding + i + 4, sub_1.response.size, "Didn't received headder with padding")

              i = 435
              expected_padding = 600 - ((i/100).to_i * 100)

              sub_1 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => headers.merge("User-Agent" => "Test 1"), :timeout => 30
              sub_1.callback {
                assert_equal(200, sub_1.response_header.status, "Channel was founded")
                assert_equal(expected_padding + i + 4, sub_1.response.size, "Didn't received headder with padding")

                i = 502
                expected_padding = 600 - ((i/100).to_i * 100)

                sub_1 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => headers.merge("User-Agent" => "Test 1"), :timeout => 30
                sub_1.callback {
                  assert_equal(200, sub_1.response_header.status, "Channel was founded")
                  assert_equal(expected_padding + i + 4, sub_1.response.size, "Didn't received headder with padding")

                  i = 550

                  sub_1 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => headers.merge("User-Agent" => "Test 1"), :timeout => 30
                  sub_1.callback {
                    assert_equal(200, sub_1.response_header.status, "Channel was founded")
                    assert_equal(i + 2, sub_1.response.size, "Didn't received headder with padding")

                    EventMachine.stop
                  }
                  publish_message_inline(channel, headers, "_" * i)
                }
                publish_message_inline(channel, headers, "_" * i)
              }
              publish_message_inline(channel, headers, "_" * i)
            }
            publish_message_inline(channel, headers, "_" * i)
          }
          publish_message_inline(channel, headers, "_" * i)
        }
        publish_message_inline(channel, headers, "_" * i)
      }
      publish_message_inline(channel, headers, "_" * i)

      add_test_timeout(10)
    }
  end

  def config_test_user_agent_by_complex_value
    @padding_by_user_agent = "[T|t]est 1,1024,512"
    @user_agent = "$arg_ua"
    @header_template = "0123456789"
  end

  def test_user_agent_by_complex_value
    headers = {'accept' => 'application/json'}
    channel = 'ch_test_user_agent_by_complex_value'

    EventMachine.run {
      sub_1 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s + '?ua=test 1').get :head => headers, :timeout => 30
      sub_1.callback {
        assert_equal(200, sub_1.response_header.status, "Channel was founded")
        assert_equal(1024 + @header_template.size + 4, sub_1.response.size, "Didn't received headder with padding")

        sub_2 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s + '?ua=test 2').get :head => headers, :timeout => 30
        sub_2.callback {
          assert_equal(200, sub_2.response_header.status, "Channel was founded")
          assert_equal(@header_template.size + 2, sub_2.response.size, "Didn't received headder with padding")

          EventMachine.stop
        }
      }
      add_test_timeout
    }
  end
end
