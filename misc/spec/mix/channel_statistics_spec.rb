require 'spec_helper'

describe "Channel Statistics" do
  let(:config) do
   {}
  end

shared_examples_for "statistics location" do
  it "should return 404 for a nonexistent channel" do
    channel = 'ch_test_get_channel_statistics_whithout_created_channel'

    nginx_run_server(config) do |conf|
      EventMachine.run do
        pub_1 = EventMachine::HttpRequest.new(nginx_address + '/channels-stats?id=' + channel.to_s).get :head => headers
        pub_1.callback do
          pub_1.should be_http_status(404).without_body
          EventMachine.stop
        end
      end
    end
  end

  it "should return channels statistics for an existent channel" do
    channel = 'ch_test_get_channel_statistics_to_existing_channel'
    body = 'body'
    actual_response = ''

    nginx_run_server(config) do |conf|
      #create channel
      publish_message(channel, headers, body)

      EventMachine.run do
        pub_2 = EventMachine::HttpRequest.new(nginx_address + '/channels-stats?id=' + channel.to_s).get :head => headers, :decoding => false
        pub_2.stream do |chunk|
          actual_response << chunk
        end
        pub_2.callback do
          pub_2.should be_http_status(200)

          if (conf.gzip == "on")
            pub_2.response_header["CONTENT_ENCODING"].should eql("gzip")
            actual_response = Zlib::GzipReader.new(StringIO.new(actual_response)).read
          end

          response = JSON.parse(actual_response)
          response["channel"].to_s.should eql(channel)
          response["published_messages"].to_i.should eql(1)
          response["stored_messages"].to_i.should eql(1)
          response["subscribers"].to_i.should eql(0)
          EventMachine.stop
        end
      end
    end
  end

  it "should return channels statistics for an existent channel with subscriber" do
    channel = 'ch_test_get_channel_statistics_to_existing_channel_with_subscriber'
    body = 'body'

    nginx_run_server(config) do |conf|
      create_channel_by_subscribe(channel, headers) do
        pub_1 = EventMachine::HttpRequest.new(nginx_address + '/channels-stats?id=' + channel.to_s).get :head => headers
        pub_1.callback do
          pub_1.should be_http_status(200)
          response = JSON.parse(pub_1.response)
          response["channel"].to_s.should eql(channel)
          response["published_messages"].to_i.should eql(0)
          response["stored_messages"].to_i.should eql(0)
          response["subscribers"].to_i.should eql(1)
          EventMachine.stop
        end
      end
    end
  end

  it "should return detailed channels statistics without existing channels" do
    nginx_run_server(config) do |conf|
      EventMachine.run do
        pub_2 = EventMachine::HttpRequest.new(nginx_address + '/channels-stats?id=ALL').get :head => headers
        pub_2.callback do
          pub_2.should be_http_status(200)
          response = JSON.parse(pub_2.response)
          response["infos"].length.should eql(0)
          EventMachine.stop
        end
      end
    end
  end

  it "should return detailed channels statistics for an existent channel" do
    channel = 'ch_test_get_detailed_channels_statistics_to_existing_channel'
    body = 'body'
    actual_response = ''

    nginx_run_server(config) do |conf|
      #create channel
      publish_message(channel, headers, body)

      EventMachine.run do
        pub_2 = EventMachine::HttpRequest.new(nginx_address + '/channels-stats?id=ALL').get :head => headers, :decoding => false
        pub_2.stream do |chunk|
          actual_response << chunk
        end
        pub_2.callback do
          pub_2.should be_http_status(200)

          if (conf.gzip == "on")
            pub_2.response_header["CONTENT_ENCODING"].should eql("gzip")
            actual_response = Zlib::GzipReader.new(StringIO.new(actual_response)).read
          end

          response = JSON.parse(actual_response)
          response["infos"].length.should eql(1)
          response["infos"][0]["channel"].to_s.should eql(channel)
          response["infos"][0]["published_messages"].to_i.should eql(1)
          response["infos"][0]["stored_messages"].to_i.should eql(1)
          response["infos"][0]["subscribers"].to_i.should eql(0)
          EventMachine.stop
        end
      end
    end
  end

  it "should return detailed channels statistics for an existent wildcard channel" do
    channel = 'bd_test_get_detailed_channels_statistics_to_existing_wildcard_channel'
    body = 'body'

    nginx_run_server(config.merge(:wildcard_channel_prefix => 'bd_', :wildcard_channel_max_qtd => 1)) do |conf|
      #create channel
      publish_message(channel, headers, body)

      EventMachine.run do
        pub_2 = EventMachine::HttpRequest.new(nginx_address + '/channels-stats?id=ALL').get :head => headers
        pub_2.callback do
          pub_2.should be_http_status(200)
          response = JSON.parse(pub_2.response)
          response["infos"].length.should eql(1)
          response["channels"].to_i.should eql(0)
          response["wildcard_channels"].to_i.should eql(1)
          response["infos"][0]["channel"].to_s.should eql(channel)
          response["infos"][0]["published_messages"].to_i.should eql(1)
          response["infos"][0]["stored_messages"].to_i.should eql(1)
          response["infos"][0]["subscribers"].to_i.should eql(0)
          EventMachine.stop
        end
      end
    end
  end

  it "should return detailed channels statistics for an existent channel with subscriber" do
    channel = 'ch_test_detailed_channels_statistics_to_existing_channel_with_subscriber'
    body = 'body'

    nginx_run_server(config) do |conf|
      create_channel_by_subscribe(channel, headers) do
        pub_1 = EventMachine::HttpRequest.new(nginx_address + '/channels-stats?id=ALL').get :head => headers
        pub_1.callback do
          pub_1.should be_http_status(200)
          response = JSON.parse(pub_1.response)
          response["infos"].length.should eql(1)
          response["infos"][0]["channel"].to_s.should eql(channel)
          response["infos"][0]["published_messages"].to_i.should eql(0)
          response["infos"][0]["stored_messages"].to_i.should eql(0)
          response["infos"][0]["subscribers"].to_i.should eql(1)
          EventMachine.stop
        end
      end
    end
  end

  it "should return summarized channels statistics for a nonexistent channel" do
    nginx_run_server(config) do |conf|
      EventMachine.run do
        pub_1 = EventMachine::HttpRequest.new(nginx_address + '/channels-stats').get :head => headers
        pub_1.callback do
          pub_1.should be_http_status(200)
          response = JSON.parse(pub_1.response)
          response.has_key?("channels").should be_true
          response["channels"].to_i.should eql(0)
          EventMachine.stop
        end
      end
    end
  end

  it "should return summarized channels statistics for an existent channel" do
    channel = 'ch_test_get_summarized_channels_statistics_to_existing_channel'
    body = 'body'
    actual_response = ''

    nginx_run_server(config) do |conf|
      #create channel
      publish_message(channel, headers, body)

      EventMachine.run do
        pub_2 = EventMachine::HttpRequest.new(nginx_address + '/channels-stats').get :head => headers, :decoding => false
        pub_2.stream do |chunk|
          actual_response << chunk
        end
        pub_2.callback do
          pub_2.should be_http_status(200)

          if (conf.gzip == "on")
            pub_2.response_header["CONTENT_ENCODING"].should eql("gzip")
            actual_response = Zlib::GzipReader.new(StringIO.new(actual_response)).read
          end

          response = JSON.parse(actual_response)
          response.has_key?("channels").should be_true
          response["channels"].to_i.should eql(1)
          response["published_messages"].to_i.should eql(1)
          response["subscribers"].to_i.should eql(0)
          EventMachine.stop
        end
      end
    end
  end

  it "should return summarized channels statistics for an existent wildcard channel" do
    channel = 'bd_test_get_summarized_channels_statistics_to_existing_wildcard_channel'
    body = 'body'

    nginx_run_server(config.merge(:wildcard_channel_prefix => 'bd_', :wildcard_channel_max_qtd => 1)) do |conf|
      #create channel
      publish_message(channel, headers, body)

      EventMachine.run do
        pub_2 = EventMachine::HttpRequest.new(nginx_address + '/channels-stats').get :head => headers
        pub_2.callback do
          pub_2.should be_http_status(200)
          response = JSON.parse(pub_2.response)
          response.has_key?("channels").should be_true
          response["channels"].to_i.should eql(0)
          response["wildcard_channels"].to_i.should eql(1)
          response["published_messages"].to_i.should eql(1)
          response["subscribers"].to_i.should eql(0)
          EventMachine.stop
        end
      end
    end
  end

  it "should return summarized channels statistics for an existent channel with subscriber" do
    channel = 'ch_test_summarized_channels_statistics_to_existing_channel_with_subscriber'
    body = 'body'

    nginx_run_server(config) do |conf|
      create_channel_by_subscribe(channel, headers) do
        pub_1 = EventMachine::HttpRequest.new(nginx_address + '/channels-stats').get :head => headers
        pub_1.callback do
          pub_1.should be_http_status(200)
          response = JSON.parse(pub_1.response)
          response.has_key?("channels").should be_true
          response["channels"].to_i.should eql(1)
          response["published_messages"].to_i.should eql(0)
          response["subscribers"].to_i.should eql(1)
          EventMachine.stop
        end
      end
    end
  end

  it "should check accepted methods" do
    nginx_run_server(config) do |conf|
      EventMachine.run do
        multi = EventMachine::MultiRequest.new

        multi.add(:a, EventMachine::HttpRequest.new(nginx_address + '/channels-stats').get)
        multi.add(:b, EventMachine::HttpRequest.new(nginx_address + '/channels-stats').put(:body => 'body'))
        multi.add(:c, EventMachine::HttpRequest.new(nginx_address + '/channels-stats').post)
        multi.add(:d, EventMachine::HttpRequest.new(nginx_address + '/channels-stats').delete)
        multi.add(:e, EventMachine::HttpRequest.new(nginx_address + '/channels-stats').head)

        multi.callback do
          multi.responses[:callback].length.should eql(5)

          multi.responses[:callback][:a].should_not be_http_status(405)
          multi.responses[:callback][:a].req.method.should eql("GET")

          multi.responses[:callback][:b].should be_http_status(405)
          multi.responses[:callback][:b].req.method.should eql("PUT")

          multi.responses[:callback][:c].should be_http_status(405)
          multi.responses[:callback][:c].req.method.should eql("POST")

          multi.responses[:callback][:d].should be_http_status(405)
          multi.responses[:callback][:d].req.method.should eql("DELETE")

          multi.responses[:callback][:e].should be_http_status(405)
          multi.responses[:callback][:e].req.method.should eql("HEAD")

          EventMachine.stop
        end
      end
    end
  end

  it "should check accepted content types" do
    channel = 'ch_test_accepted_content_types'
    body = 'body'

    nginx_run_server(config) do |conf|
      #create channel
      publish_message(channel, headers, body)

      EventMachine.run do
        multi = EventMachine::MultiRequest.new

        multi.add(:a, EventMachine::HttpRequest.new(nginx_address + '/channels-stats').get) # default content_type
        multi.add(:b, EventMachine::HttpRequest.new(nginx_address + '/channels-stats').get(:head => {'accept' => 'text/plain'}))
        multi.add(:c, EventMachine::HttpRequest.new(nginx_address + '/channels-stats').get(:head => {'accept' => 'application/json'}))
        multi.add(:d, EventMachine::HttpRequest.new(nginx_address + '/channels-stats').get(:head => {'accept' => 'application/yaml'}))
        multi.add(:e, EventMachine::HttpRequest.new(nginx_address + '/channels-stats').get(:head => {'accept' => 'application/xml'}))
        multi.add(:f, EventMachine::HttpRequest.new(nginx_address + '/channels-stats').get(:head => {'accept' => 'text/x-json'}))
        multi.add(:g, EventMachine::HttpRequest.new(nginx_address + '/channels-stats').get(:head => {'accept' => 'text/x-yaml'}))

        multi.callback do
          multi.responses[:callback].length.should eql(7)

          multi.responses[:callback][:a].should be_http_status(200).with_body
          multi.responses[:callback][:a].req.method.should eql("GET")
          multi.responses[:callback][:a].response_header["CONTENT_TYPE"].should eql("application/json")

          multi.responses[:callback][:b].should be_http_status(200).with_body
          multi.responses[:callback][:b].req.method.should eql("GET")
          multi.responses[:callback][:b].response_header["CONTENT_TYPE"].should eql("text/plain")

          multi.responses[:callback][:c].should be_http_status(200).with_body
          multi.responses[:callback][:c].req.method.should eql("GET")
          multi.responses[:callback][:c].response_header["CONTENT_TYPE"].should eql("application/json")

          multi.responses[:callback][:d].should be_http_status(200).with_body
          multi.responses[:callback][:d].req.method.should eql("GET")
          multi.responses[:callback][:d].response_header["CONTENT_TYPE"].should eql("application/yaml")

          multi.responses[:callback][:e].should be_http_status(200).with_body
          multi.responses[:callback][:e].req.method.should eql("GET")
          multi.responses[:callback][:e].response_header["CONTENT_TYPE"].should eql("application/xml")

          multi.responses[:callback][:f].should be_http_status(200).with_body
          multi.responses[:callback][:f].req.method.should eql("GET")
          multi.responses[:callback][:f].response_header["CONTENT_TYPE"].should eql("text/x-json")

          multi.responses[:callback][:g].should be_http_status(200).with_body
          multi.responses[:callback][:g].req.method.should eql("GET")
          multi.responses[:callback][:g].response_header["CONTENT_TYPE"].should eql("text/x-yaml")

          EventMachine.stop
        end
      end
    end
  end

  it "should return detailed channels statistics for many channels" do
    channel = 'ch_test_get_detailed_channels_statistics_to_many_channels_'
    body = 'body'
    number_of_channels = 20000

    nginx_run_server(config.merge(:shared_memory_size => '200m', :keepalive_requests => 1000), :timeout => 15) do |conf|
      #create channels
      0.step(number_of_channels - 1, 1000) do |i|
        socket = open_socket(nginx_host, nginx_port)
        1.upto(1000) do |j|
          headers, body = post_in_socket("/pub?id=#{channel}#{i + j}", body, socket, {:wait_for => "}\r\n"})
          headers.should include("HTTP/1.1 200 OK")
        end
        socket.close
      end

      EventMachine.run do
        pub_2 = EventMachine::HttpRequest.new(nginx_address + '/channels-stats?id=ALL').get :head => headers
        pub_2.callback do
          pub_2.should be_http_status(200)
          response = JSON.parse(pub_2.response)
          response["infos"].length.should eql(number_of_channels)
          EventMachine.stop
        end
      end
    end
  end

  it "should return detailed channels statistics for a nonexistent channel using prefix id" do
    nginx_run_server(config) do |conf|
      EventMachine.run do
        pub_2 = EventMachine::HttpRequest.new(nginx_address + '/channels-stats?id=prefix_*').get :head => headers
        pub_2.callback do
          pub_2.should be_http_status(200)
          response = JSON.parse(pub_2.response)
          response["infos"].length.should eql(0)
          EventMachine.stop
        end
      end
    end
  end

  it "should return detailed channels statistics for an existent channel using prefix id" do
    channel = 'ch_test_get_detailed_channels_statistics_to_existing_channel_using_prefix'
    channel_1 = 'another_ch_test_get_detailed_channels_statistics_to_existing_channel_using_prefix'
    body = 'body'

    nginx_run_server(config) do |conf|
      #create channels
      publish_message(channel, headers, body)
      publish_message(channel_1, headers, body)

      EventMachine.run do
        pub_2 = EventMachine::HttpRequest.new(nginx_address + '/channels-stats?id=ch_test_*').get :head => headers
        pub_2.callback do
          pub_2.should be_http_status(200)
          response = JSON.parse(pub_2.response)
          response["infos"].length.should eql(1)
          response["infos"][0]["channel"].to_s.should eql(channel)
          response["infos"][0]["published_messages"].to_i.should eql(1)
          response["infos"][0]["stored_messages"].to_i.should eql(1)
          response["infos"][0]["subscribers"].to_i.should eql(0)
          EventMachine.stop
        end
      end
    end
  end

  it "should return detailed channels statistics using prefix id with same behavior as ALL" do
    channel = 'ch_test_get_detailed_channels_statistics_using_prefix_as_same_behavior_ALL'
    channel_1 = 'another_ch_test_get_detailed_channels_statistics_using_prefix_as_same_behavior_ALL'
    body = 'body'

    nginx_run_server(config) do |conf|
      #create channels
      publish_message(channel, headers, body)
      publish_message(channel_1, headers, body)

      EventMachine.run do
        pub_2 = EventMachine::HttpRequest.new(nginx_address + '/channels-stats?id=*').get :head => headers
        pub_2.callback do
          pub_2.should be_http_status(200)
          response = JSON.parse(pub_2.response)
          response["infos"].length.should eql(2)
          response["infos"][0]["channel"].to_s.should eql(channel)
          response["infos"][0]["published_messages"].to_i.should eql(1)
          response["infos"][0]["stored_messages"].to_i.should eql(1)
          response["infos"][0]["subscribers"].to_i.should eql(0)
          response["infos"][1]["channel"].to_s.should eql(channel_1)
          response["infos"][1]["published_messages"].to_i.should eql(1)
          response["infos"][1]["stored_messages"].to_i.should eql(1)
          response["infos"][1]["subscribers"].to_i.should eql(0)
          EventMachine.stop
        end
      end
    end
  end

  it "should return detailed channels statistics for an existent wildcard channel using prefix id" do
    channel = 'bd_test_get_detailed_channels_statistics_to_existing_wildcard_channel_using_prefix'
    body = 'body'

    nginx_run_server(config.merge(:wildcard_channel_prefix => 'bd_', :wildcard_channel_max_qtd => 1)) do |conf|
      #create channels
      publish_message(channel, headers, body)

      EventMachine.run do
        pub_2 = EventMachine::HttpRequest.new(nginx_address + '/channels-stats?id=bd_test_*').get :head => headers
        pub_2.callback do
          pub_2.should be_http_status(200)
          response = JSON.parse(pub_2.response)
          response["infos"].length.should eql(1)
          response["channels"].to_i.should eql(0)
          response["wildcard_channels"].to_i.should eql(1)
          response["infos"][0]["channel"].to_s.should eql(channel)
          response["infos"][0]["published_messages"].to_i.should eql(1)
          response["infos"][0]["stored_messages"].to_i.should eql(1)
          response["infos"][0]["subscribers"].to_i.should eql(0)
          EventMachine.stop
        end
      end
    end
  end

  it "should return detailed channels statistics for an existent channel using prefix id with subscriber" do
    channel = 'ch_test_detailed_channels_statistics_to_existing_channel_with_subscriber_using_prefix'
    body = 'body'

    nginx_run_server(config) do |conf|
      create_channel_by_subscribe(channel, headers) do
        pub_1 = EventMachine::HttpRequest.new(nginx_address + '/channels-stats?id=ch_test_*').get :head => headers
        pub_1.callback do
          pub_1.should be_http_status(200)
          response = JSON.parse(pub_1.response)
          response["infos"].length.should eql(1)
          response["infos"][0]["channel"].to_s.should eql(channel)
          response["infos"][0]["published_messages"].to_i.should eql(0)
          response["infos"][0]["stored_messages"].to_i.should eql(0)
          response["infos"][0]["subscribers"].to_i.should eql(1)
          EventMachine.stop
        end
      end
    end
  end

  it "should return detailed channels statistics for many channels using prefix id" do
    channel = 'ch_test_get_detailed_channels_statistics_to_many_channels_using_prefix_'
    body = 'body'
    number_of_channels = 20000

    nginx_run_server(config.merge(:shared_memory_size => '200m', :keepalive_requests => 1000), :timeout => 15) do |conf|
      #create channels
      0.step(number_of_channels - 1, 1000) do |i|
        socket = open_socket(nginx_host, nginx_port)
        1.upto(1000) do |j|
          headers, body = post_in_socket("/pub?id=#{channel}#{i + j}", body, socket, {:wait_for => "}\r\n"})
          headers.should include("HTTP/1.1 200 OK")
        end
        socket.close
      end

      EventMachine.run do
        pub_2 = EventMachine::HttpRequest.new(nginx_address + '/channels-stats?id=ch_test_get_detailed_channels_statistics_to_many_channels_using_prefix_10*').get :head => headers
        pub_2.callback do
          pub_2.should be_http_status(200)
          response = JSON.parse(pub_2.response)
          response["infos"].length.should eql(1111)
          EventMachine.stop
        end
      end
    end
  end

  it "should return uptime in detailed channels statistics" do
    channel = 'ch_test_get_uptime_in_detailed_channels_statistics'
    body = 'body'

    nginx_run_server(config) do |conf|
      #create channel
      publish_message(channel, headers, body)

      EventMachine.run do
        pub_2 = EventMachine::HttpRequest.new(nginx_address + '/channels-stats?id=ALL').get :head => headers
        pub_2.callback do
          pub_2.should be_http_status(200)
          response = JSON.parse(pub_2.response)
          response["hostname"].to_s.should_not be_empty
          response["time"].to_s.should_not be_empty
          response["channels"].to_s.should_not be_empty
          response["wildcard_channels"].to_s.should_not be_empty
          response["uptime"].to_s.should_not be_empty
          response["infos"].to_s.should_not be_empty

          sleep(2)
          pub_3 = EventMachine::HttpRequest.new(nginx_address + '/channels-stats?id=ALL').get :head => headers
          pub_3.callback do
            pub_3.should be_http_status(200)
            response = JSON.parse(pub_3.response)
            response["uptime"].to_i.should be_in_the_interval(2, 3)
            EventMachine.stop
          end
        end
      end
    end
  end

  it "should return uptime in summarized channels statistics" do
    channel = 'ch_test_get_uptime_in_summarized_channels_statistics'
    body = 'body'

    nginx_run_server(config) do |conf|
      #create channel
      publish_message(channel, headers, body)

      EventMachine.run do
        pub_2 = EventMachine::HttpRequest.new(nginx_address + '/channels-stats').get :head => headers
        pub_2.callback do
          pub_2.should be_http_status(200)
          response = JSON.parse(pub_2.response)
          response["hostname"].to_s.should_not be_empty
          response["time"].to_s.should_not be_empty
          response["channels"].to_s.should_not be_empty
          response["wildcard_channels"].to_s.should_not be_empty
          response["subscribers"].to_s.should_not be_empty
          response["uptime"].to_s.should_not be_empty
          response["by_worker"].to_s.should_not be_empty
          response["by_worker"][0]["pid"].to_s.should_not be_empty
          response["by_worker"][0]["subscribers"].to_s.should_not be_empty
          response["by_worker"][0]["uptime"].to_s.should_not be_empty

          sleep(2)
          pub_3 = EventMachine::HttpRequest.new(nginx_address + '/channels-stats').get :head => headers
          pub_3.callback do
            pub_3.should be_http_status(200)
            response = JSON.parse(pub_3.response)
            response["uptime"].to_i.should be_in_the_interval(2, 3)
            response["by_worker"][0]["uptime"].to_i.should be_in_the_interval(2, 3)
            EventMachine.stop
          end
        end
      end
    end
  end

  it "should return the number of messages in the trash in summarized channels statistics" do
    channel = 'ch_test_get_messages_in_trash_in_summarized_channels_statistics'
    body = 'body'

    nginx_run_server(config.merge(:message_ttl => '1s'), :timeout => 15) do |conf|
      #create channel
      publish_message(channel, headers, body)

      EventMachine.run do
        pub_2 = EventMachine::HttpRequest.new(nginx_address + '/channels-stats').get :head => headers
        pub_2.callback do
          pub_2.should be_http_status(200)
          response = JSON.parse(pub_2.response)
          response["stored_messages"].to_i.should eql(1)
          response["messages_in_trash"].to_i.should eql(0)

          sleep(5)
          pub_3 = EventMachine::HttpRequest.new(nginx_address + '/channels-stats').get :head => headers
          pub_3.callback do
            pub_3.should be_http_status(200)
            response = JSON.parse(pub_3.response)
            response["stored_messages"].to_i.should eql(0)
            response["messages_in_trash"].to_i.should eql(1)
            EventMachine.stop
          end
        end
      end
    end
  end

  it "should return the number of channels in the trash in summarized channels statistics" do
    channel = 'ch_test_get_channels_in_trash_in_summarized_channels_statistics'
    body = 'body'

    nginx_run_server(config.merge(:publisher_mode => 'admin', :wildcard_channel_prefix => 'bd_', :wildcard_channel_max_qtd => 1), :timeout => 55) do |conf|
      #create channel
      publish_message(channel, headers, body)
      publish_message("#{channel}_1", headers, body)
      publish_message("bd_#{channel}_1", headers, body)

      EventMachine.run do
        pub_2 = EventMachine::HttpRequest.new(nginx_address + '/channels-stats').get :head => headers
        pub_2.callback do
          pub_2.should be_http_status(200)
          response = JSON.parse(pub_2.response)
          response["channels"].to_i.should eql(2)
          response["wildcard_channels"].to_i.should eql(1)
          response["channels_in_trash"].to_i.should eql(0)

          pub = EventMachine::HttpRequest.new(nginx_address + '/pub?id=' + channel.to_s).delete :head => headers
          pub.callback do
            pub.should be_http_status(200).without_body

            sleep(2)

            pub_3 = EventMachine::HttpRequest.new(nginx_address + '/channels-stats').get :head => headers
            pub_3.callback do
              pub_3.should be_http_status(200)
              response = JSON.parse(pub_3.response)
              response["channels"].to_i.should eql(1)
              response["wildcard_channels"].to_i.should eql(1)
              response["channels_in_trash"].to_i.should eql(1)
              EventMachine.stop
            end
          end
        end
      end
    end
  end

  it "should not cache the response" do
    channel = 'ch_test_not_cache_the_response'

    nginx_run_server(config) do |conf|
      EventMachine.run do
        pub_1 = EventMachine::HttpRequest.new(nginx_address + '/channels-stats?id=' + channel.to_s).get :head => headers
        pub_1.callback do
          pub_1.response_header["EXPIRES"].should eql("Thu, 01 Jan 1970 00:00:01 GMT")
          pub_1.response_header["CACHE_CONTROL"].should eql("no-cache, no-store, must-revalidate")
          EventMachine.stop
        end
      end
    end
  end
end

  context "when getting statistics" do
    context "without gzip" do
      let(:config) do
       {:gzip => "off"}
      end

      let(:headers) do
        {'accept' => 'application/json'}
      end

      it_should_behave_like "statistics location"
    end

    context "with gzip" do
      let(:config) do
       {:gzip => "on"}
      end

      let(:headers) do
        {'accept' => 'application/json', 'accept-encoding' => 'gzip, compressed'}
      end

      it_should_behave_like "statistics location"
    end
  end
end
