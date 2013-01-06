require 'spec_helper'

describe "Publisher Properties" do

  shared_examples_for "publisher location" do
    it "should not accept access without a channel id" do
      nginx_run_server(config) do |conf|
        EventMachine.run do
          pub = EventMachine::HttpRequest.new(nginx_address + '/pub?id=').get :head => headers
          pub.callback do
            pub.should be_http_status(400).without_body
            pub.response_header['X_NGINX_PUSHSTREAM_EXPLAIN'].should eql("No channel id provided.")
            EventMachine.stop
          end
        end
      end
    end

    it "should not accept 'get' access to a nonexistent channel" do
      channel_1 = 'ch_test_access_whith_channel_id_to_absent_channel_1'
      channel_2 = 'ch_test_access_whith_channel_id_to_absent_channel_2'
      body = 'body'

      nginx_run_server(config) do |conf|
        EventMachine.run do
          pub_1 = EventMachine::HttpRequest.new(nginx_address + '/pub?id=' + channel_1.to_s).get :head => headers
          pub_1.callback do
            pub_1.should be_http_status(404).without_body
            EventMachine.stop
          end
        end

        EventMachine.run do
          pub_2 = EventMachine::HttpRequest.new(nginx_address + '/pub?id=' + channel_2.to_s ).post :head => headers, :body => body
          pub_2.callback do
            pub_2.should be_http_status(200).with_body
            response = JSON.parse(pub_2.response)
            response["channel"].to_s.should eql(channel_2)
            EventMachine.stop
          end
        end
      end
    end

    it "should accept 'get' access to an existent channel" do
      channel = 'ch_test_access_whith_channel_id_to_existing_channel'
      body = 'body'

      nginx_run_server(config) do |conf|
        #create channel
        EventMachine.run do
          pub_1 = EventMachine::HttpRequest.new(nginx_address + '/pub?id=' + channel.to_s).post :head => headers, :body => body
          pub_1.callback do
            pub_1.should be_http_status(200).with_body
            response = JSON.parse(pub_1.response)
            response["channel"].to_s.should eql(channel)
            EventMachine.stop
          end
        end

        EventMachine.run do
          pub_2 = EventMachine::HttpRequest.new(nginx_address + '/pub?id=' + channel.to_s).get :head => headers
          pub_2.callback do
            pub_2.should be_http_status(200).with_body
            response = JSON.parse(pub_2.response)
            response["channel"].to_s.should eql(channel)
            EventMachine.stop
          end
        end
      end
    end

    it "should check accepted methods" do
      nginx_run_server(config) do |conf|
        EventMachine.run do
          multi = EventMachine::MultiRequest.new

          multi.add(:a, EventMachine::HttpRequest.new(nginx_address + '/pub?id=ch_test_accepted_methods_1').get)
          multi.add(:b, EventMachine::HttpRequest.new(nginx_address + '/pub?id=ch_test_accepted_methods_2').put(:body => 'body'))
          multi.add(:c, EventMachine::HttpRequest.new(nginx_address + '/pub?id=ch_test_accepted_methods_3').post)
          multi.add(:d, EventMachine::HttpRequest.new(nginx_address + '/pub?id=ch_test_accepted_methods_4').delete)
          multi.add(:e, EventMachine::HttpRequest.new(nginx_address + '/pub?id=ch_test_accepted_methods_5').head)

          multi.callback do
            multi.responses[:callback].length.should eql(5)

            multi.responses[:callback][:a].should_not be_http_status(405)
            multi.responses[:callback][:a].req.method.should eql("GET")

            multi.responses[:callback][:b].should_not be_http_status(405)
            multi.responses[:callback][:b].req.method.should eql("PUT")

            multi.responses[:callback][:c].should_not be_http_status(405)
            multi.responses[:callback][:c].req.method.should eql("POST")

            multi.responses[:callback][:d].req.method.should eql("DELETE")
            if conf.publisher_mode == 'admin'
              multi.responses[:callback][:d].should_not be_http_status(405)
            else
              multi.responses[:callback][:d].should be_http_status(405)
              multi.responses[:callback][:d].response_header['ALLOW'].should eql(accepted_methods)
            end

            multi.responses[:callback][:e].should be_http_status(405)
            multi.responses[:callback][:e].req.method.should eql("HEAD")
            multi.responses[:callback][:e].response_header['ALLOW'].should eql(accepted_methods)

            EventMachine.stop
          end
        end
      end
    end

    it "should not accept create a channel with id 'ALL'" do
      channel = 'ALL'
      body = 'body'

      nginx_run_server(config) do |conf|
        EventMachine.run do
          pub_1 = EventMachine::HttpRequest.new(nginx_address + '/pub?id=' + channel.to_s).post :head => headers, :body => body
          pub_1.callback do
            pub_1.should be_http_status(403).without_body
            pub_1.response_header['X_NGINX_PUSHSTREAM_EXPLAIN'].should eql("Channel id not authorized for this method.")
            EventMachine.stop
          end
        end
      end
    end

    it "should not accept create a channel with id containing wildcard" do
      channel_1 = 'abcd*efgh'
      channel_2 = '*abcdefgh'
      channel_3 = 'abcdefgh*'
      body = 'body'

      nginx_run_server(config) do |conf|
        EventMachine.run do
          multi = EventMachine::MultiRequest.new

          multi.add(:a, EventMachine::HttpRequest.new(nginx_address + '/pub?id=' + channel_1).post(:head => headers, :body => body))
          multi.add(:b, EventMachine::HttpRequest.new(nginx_address + '/pub?id=' + channel_2).post(:head => headers, :body => body))
          multi.add(:c, EventMachine::HttpRequest.new(nginx_address + '/pub?id=' + channel_3).post(:head => headers, :body => body))
          multi.callback do
            multi.responses[:callback].length.should eql(3)
            multi.responses[:callback].each do |name, response|
              response.should be_http_status(403).without_body
              response.response_header['X_NGINX_PUSHSTREAM_EXPLAIN'].should eql("Channel id not authorized for this method.")
            end

            EventMachine.stop
          end
        end
      end
    end

    it "should not accept a message larger than max body size" do
      channel = 'ch_test_post_message_larger_than_max_body_size_should_be_rejected'
      body = '^'
      (1..40).each do |n|
        body += '0123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789|'
      end
      body += '$'

      nginx_run_server(config.merge(:client_max_body_size => '2k', :client_body_buffer_size => '1k')) do |conf|
        EventMachine.run do
          pub_1 = EventMachine::HttpRequest.new(nginx_address + '/pub?id=' + channel.to_s).post :head => headers, :body => body
          pub_1.callback do
            pub_1.should be_http_status(413)
            EventMachine.stop
          end
        end
      end
    end

    it "should accept a message larger than max buffer size and smaller than max body size" do
      channel = 'ch_test_post_message_larger_than_body_buffer_size_should_be_accepted'
      body = '^'
      (1..80).each do |n|
        body += '0123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789|'
      end
      body += '$'

      nginx_run_server(config.merge(:client_max_body_size => '10k', :client_body_buffer_size => '1k')) do |conf|
        EventMachine.run do
          pub_1 = EventMachine::HttpRequest.new(nginx_address + '/pub?id=' + channel.to_s).post :head => headers, :body => body
          pub_1.callback do
            pub_1.should be_http_status(200).with_body
            fail("Let a file on client body temp dir") unless Dir.entries(conf.client_body_temp).select {|f| f if File.file?(File.expand_path(f, conf.client_body_temp)) }.empty?
            EventMachine.stop
          end
        end
      end
    end

    it "should accept a message smaller than max body size" do
      channel = 'ch_test_post_message_shorter_than_body_buffer_size_should_be_accepted'
      body = '^'
      (1..40).each do |n|
        body += '0123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789|'
      end
      body += '$'

      nginx_run_server(config.merge(:client_max_body_size => '10k', :client_body_buffer_size => '6k')) do |conf|
        EventMachine.run do
          pub_1 = EventMachine::HttpRequest.new(nginx_address + '/pub?id=' + channel.to_s).post :head => headers, :body => body
          pub_1.callback do
            pub_1.should be_http_status(200).with_body
            fail("Let a file on client body temp dir") unless Dir.entries(conf.client_body_temp).select {|f| f if File.file?(File.expand_path(f, conf.client_body_temp)) }.empty?
            EventMachine.stop
          end
        end
      end
    end

    it "should store messages" do
      body = 'published message'
      channel = 'ch_test_stored_messages'

      nginx_run_server(config.merge(:store_messages => "on")) do |conf|
        EventMachine.run do
          pub_1 = EventMachine::HttpRequest.new(nginx_address + '/pub?id=' + channel.to_s ).post :head => headers, :body => body
          pub_1.callback do
            response = JSON.parse(pub_1.response)
            response["stored_messages"].to_i.should eql(1)

            pub_2 = EventMachine::HttpRequest.new(nginx_address + '/pub?id=' + channel.to_s ).post :head => headers, :body => body
            pub_2.callback do
              response = JSON.parse(pub_2.response)
              response["stored_messages"].to_i.should eql(2)
              EventMachine.stop
            end
          end
        end
      end
    end

    it "should not store messages when it is 'off'" do
      body = 'published message'
      channel = 'ch_test_not_stored_messages'

      nginx_run_server(config.merge(:store_messages => "off")) do |conf|
        EventMachine.run do
          pub = EventMachine::HttpRequest.new(nginx_address + '/pub?id=' + channel.to_s ).post :head => headers, :body => body
          pub.callback do
            response = JSON.parse(pub.response)
            response["stored_messages"].to_i.should eql(0)
            EventMachine.stop
          end
        end
      end
    end

    it "should limit the number of stored messages" do
      body_prefix = 'published message '
      channel = 'ch_test_max_stored_messages'
      messagens_to_publish = 10

      nginx_run_server(config.merge(:store_messages => "on", :max_messages_stored_per_channel => 4)) do |conf|
        EventMachine.run do
          i = 0
          stored_messages = 0
          EM.add_periodic_timer(0.001) do
            i += 1
            if i <= messagens_to_publish
              pub = EventMachine::HttpRequest.new(nginx_address + '/pub?id=' + channel.to_s ).post :head => headers, :body => body_prefix + i.to_s
              pub.callback do
                response = JSON.parse(pub.response)
                stored_messages = response["stored_messages"].to_i
              end
            else
              stored_messages.should eql(conf.max_messages_stored_per_channel)
              EventMachine.stop
            end
          end
        end
      end
    end

    it "should limit the size of channel id" do
      body = 'published message'
      channel = '123456'

      nginx_run_server(config.merge(:max_channel_id_length => 5)) do |conf|
        EventMachine.run do
          pub = EventMachine::HttpRequest.new(nginx_address + '/pub?id=' + channel.to_s ).post :head => headers, :body => body
          pub.callback do
            pub.should be_http_status(400).without_body
            pub.response_header['X_NGINX_PUSHSTREAM_EXPLAIN'].should eql("Channel id is too large.")
            EventMachine.stop
          end
        end
      end
    end

    it "should limit the number of channels" do
      body = 'published message'
      channel = 'ch_test_max_number_of_channels_'

      nginx_run_server(config.merge(:max_number_of_channels => 1)) do |conf|
        EventMachine.run do
          pub = EventMachine::HttpRequest.new(nginx_address + '/pub?id=' + channel.to_s + 1.to_s).post :head => headers, :body => body
          pub.callback do
            pub.should be_http_status(200).with_body
            EventMachine.stop
          end
        end

        EventMachine.run do
          pub = EventMachine::HttpRequest.new(nginx_address + '/pub?id=' + channel.to_s + 2.to_s).post :head => headers, :body => body
          pub.callback do
            pub.should be_http_status(403).without_body
            pub.response_header['X_NGINX_PUSHSTREAM_EXPLAIN'].should eql("Number of channels were exceeded.")
            EventMachine.stop
          end
        end
      end
    end

    it "should limit the number of broadcast channels" do
      body = 'published message'
      channel = 'bd_test_max_number_of_broadcast_channels_'

      nginx_run_server(config.merge(:max_number_of_broadcast_channels => 1, :broadcast_channel_prefix => 'bd_', :broadcast_channel_max_qtd => 1)) do |conf|
        EventMachine.run do
          pub = EventMachine::HttpRequest.new(nginx_address + '/pub?id=' + channel.to_s + 1.to_s).post :head => headers, :body => body
          pub.callback do
            pub.should be_http_status(200).with_body
            EventMachine.stop
          end
        end

        EventMachine.run do
          pub = EventMachine::HttpRequest.new(nginx_address + '/pub?id=' + channel.to_s + 2.to_s).post :head => headers, :body => body
          pub.callback do
            pub.should be_http_status(403).without_body
            pub.response_header['X_NGINX_PUSHSTREAM_EXPLAIN'].should eql("Number of channels were exceeded.")
            EventMachine.stop
          end
        end
      end
    end

    it "should not receive acess control allow headers by default" do
      channel = 'test_access_control_allow_headers'

      nginx_run_server(config) do |conf|
        EventMachine.run do
          pub = EventMachine::HttpRequest.new(nginx_address + '/pub?id=' + channel).get :head => headers
          pub.callback do
            pub.response_header['ACCESS_CONTROL_ALLOW_ORIGIN'].should be_nil

            EventMachine.stop
          end
        end
      end
    end

    it "should not receive channel info after publish a message when disabled" do
      body = 'published message'
      channel = 'ch_test_skip_channel_info'

      nginx_run_server(config.merge(:channel_info_on_publish => "off")) do |conf|
        EventMachine.run do
          pub_1 = EventMachine::HttpRequest.new(nginx_address + '/pub?id=' + channel.to_s ).post :head => headers, :body => body
          pub_1.callback do
            pub_1.should be_http_status(200).without_body

            EventMachine.stop
          end
        end
      end
    end

    it "should accept channel id inside an if block" do
      merged_config = config.merge({
        :header_template => nil,
        :footer_template => nil,
        :subscriber_connection_ttl => '1s',
        :extra_location => %{
          location /pub2 {
            push_stream_publisher #{config[:publisher_mode]};

            push_stream_channel_id               $arg_id;
            if ($arg_test) {
              push_stream_channel_id             test_$arg_id;
            }
          }
        }
      })

      channel = 'channel_id_inside_if_block'
      body = 'published message'
      resp_1 = ""
      resp_2 = ""

      nginx_run_server(merged_config) do |conf|
        EventMachine.run do
          sub_1 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => headers
          sub_1.stream do |chunk|
            resp_1 += chunk
          end

          sub_2 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + 'test_' + channel.to_s).get :head => headers
          sub_2.stream do |chunk|
            resp_2 += chunk
          end
          sub_2.callback do
            resp_1.should eql("<script>p(1,'channel_id_inside_if_block','published message');</script>\r\n")
            resp_2.should eql("<script>p(1,'test_channel_id_inside_if_block','published message');</script>\r\n")
            EventMachine.stop
          end

          pub_1 = EventMachine::HttpRequest.new(nginx_address + '/pub2?id=' + channel.to_s).post :head => headers, :body => body
          pub_1.callback do
            pub_1.should be_http_status(200)
          end

          pub_2 = EventMachine::HttpRequest.new(nginx_address + '/pub2?id=' + channel.to_s + '&test=1').post :head => headers, :body => body
          pub_2.callback do
            pub_2.should be_http_status(200)
          end
        end
      end
    end

    context "when allow origin directive is set" do
      it "should receive acess control allow headers" do
        channel = 'test_access_control_allow_headers'

        nginx_run_server(config.merge(:allowed_origins => "custom.domain.com")) do |conf|
          EventMachine.run do
            pub = EventMachine::HttpRequest.new(nginx_address + '/pub?id=' + channel).get :head => headers
            pub.callback do
              pub.response_header['ACCESS_CONTROL_ALLOW_ORIGIN'].should eql("custom.domain.com")

              EventMachine.stop
            end
          end
        end
      end
    end

    it "should not cache the response" do
      channel = 'ch_test_not_cache_the_response'

      nginx_run_server(config) do |conf|
        EventMachine.run do
          pub_1 = EventMachine::HttpRequest.new(nginx_address + '/pub?id=' + channel.to_s).get :head => headers
          pub_1.callback do
            pub_1.response_header["EXPIRES"].should eql("Thu, 01 Jan 1970 00:00:01 GMT")
            pub_1.response_header["CACHE_CONTROL"].should eql("no-cache, no-store, must-revalidate")
            EventMachine.stop
          end
        end
      end
    end

    it "should accept respond get requests with gzip" do
      channel = 'test_receive_get_response_with_gzip'
      body = 'body'

      actual_response = ''
      nginx_run_server(config.merge(:gzip => "on"), :timeout => 5) do |conf|
        EventMachine.run do
          #create channel
          publish_message_inline(channel, {}, body)

          pub = EventMachine::HttpRequest.new(nginx_address + '/pub?id=' + channel).get :head => headers.merge({'accept' => 'application/json', 'accept-encoding' => 'gzip, compressed'}), :decoding => false
          pub.stream do |chunk|
            actual_response << chunk
          end
          pub.callback do
            pub.response_header.status.should eql(200)
            pub.response_header.content_length.should_not eql(0)
            pub.response_header["CONTENT_ENCODING"].should eql("gzip")

            actual_response = Zlib::GzipReader.new(StringIO.new(actual_response)).read
            response = JSON.parse(actual_response)
            response["channel"].to_s.should eql(channel)
            EventMachine.stop
          end
        end
      end
    end

    it "should accept respond post requests with gzip" do
      channel = 'test_receive_post_response_with_gzip'
      body = 'body'

      actual_response = ''
      nginx_run_server(config.merge(:gzip => "on"), :timeout => 5) do |conf|
        EventMachine.run do
          pub = EventMachine::HttpRequest.new(nginx_address + '/pub?id=' + channel).post :body => body, :head => headers.merge({'accept' => 'application/json', 'accept-encoding' => 'gzip, compressed'}), :decoding => false
          pub.stream do |chunk|
            actual_response << chunk
          end
          pub.callback do
            pub.response_header.status.should eql(200)
            pub.response_header.content_length.should_not eql(0)
            pub.response_header["CONTENT_ENCODING"].should eql("gzip")

            actual_response = Zlib::GzipReader.new(StringIO.new(actual_response)).read
            response = JSON.parse(actual_response)
            response["channel"].to_s.should eql(channel)
            EventMachine.stop
          end
        end
      end
    end
  end

  context "when is on normal mode" do
    let(:config) do
      {}
    end

    let(:headers) do
      {'accept' => 'text/html'}
    end

    let(:accepted_methods) do
      "GET, POST, PUT"
    end

    it_should_behave_like "publisher location"
  end

  context "when is on admin mode" do
    let(:config) do
      {:publisher_mode => 'admin'}
    end

    let(:headers) do
      {'accept' => 'text/html'}
    end

    let(:accepted_methods) { "GET, POST, PUT, DELETE" }

    it_should_behave_like "publisher location"

    it "should delete a channel without subscribers" do
      channel = 'test_delete_channel_whithout_subscribers'
      body = 'published message'

      nginx_run_server(config) do |conf|
        publish_message(channel, headers, body)

        EventMachine.run do
          pub = EventMachine::HttpRequest.new(nginx_address + '/pub?id=' + channel.to_s).delete :head => headers
          pub.callback do
            pub.should be_http_status(200).without_body
            pub.response_header['X_NGINX_PUSHSTREAM_EXPLAIN'].should eql("Channel deleted.")

            stats = EventMachine::HttpRequest.new(nginx_address + '/channels-stats').get :head => headers
            stats.callback do
              stats.should be_http_status(200).with_body
              response = JSON.parse(stats.response)
              response["channels"].to_s.should_not be_empty
              response["channels"].to_i.should eql(0)
              EventMachine.stop
            end
          end
        end
      end
    end

    it "should delete a channel with subscriber" do
      channel = 'test_delete_channel_whith_subscriber_in_one_channel'
      body = 'published message'

      configuration = config.merge({
        :header_template => " ", # send a space as header to has a chunk received
        :footer_template => nil,
        :ping_message_interval => nil,
        :message_template => '{\"id\":\"~id~\", \"channel\":\"~channel~\", \"text\":\"~text~\"}'
      })

      resp = ""
      nginx_run_server(configuration) do |conf|
        EventMachine.run do
          sub_1 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => headers
          sub_1.stream do |chunk|

            resp = resp + chunk
            if resp.strip.empty?
              stats = EventMachine::HttpRequest.new(nginx_address + '/channels-stats').get :head => {'accept' => 'application/json'}
              stats.callback do
                stats.should be_http_status(200).with_body
                response = JSON.parse(stats.response)
                response["subscribers"].to_i.should eql(1)
                response["channels"].to_i.should eql(1)
                pub = EventMachine::HttpRequest.new(nginx_address + '/pub?id=' + channel.to_s).delete :head => headers
                pub.callback do
                  pub.should be_http_status(200).without_body
                  pub.response_header['X_NGINX_PUSHSTREAM_EXPLAIN'].should eql("Channel deleted.")
                end
              end
            else
              response = JSON.parse(resp)
              response["channel"].should eql(channel)
              response["id"].to_i.should eql(-2)
              response["text"].should eql("Channel deleted")

              stats = EventMachine::HttpRequest.new(nginx_address + '/channels-stats').get :head => {'accept' => 'application/json'}
              stats.callback do
                stats.should be_http_status(200).with_body
                response = JSON.parse(stats.response)
                response["subscribers"].to_i.should eql(0)
                response["channels"].to_i.should eql(0)
              end
              EventMachine.stop
            end
          end
        end
      end
    end

    it "should delete a channel with a custom message" do
      channel = 'test_delete_channel_whith_subscriber_in_one_channel'
      body = 'published message'

      configuration = config.merge({
        :header_template => " ", # send a space as header to has a chunk received
        :footer_template => nil,
        :ping_message_interval => nil,
        :message_template => '{\"id\":\"~id~\", \"channel\":\"~channel~\", \"text\":\"~text~\"}'
      })

      resp = ""
      nginx_run_server(configuration, :timeout => 5) do |conf|
        EventMachine.run do
          sub_1 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => headers, :timeout => 30
          sub_1.stream do |chunk|

            resp = resp + chunk
            if resp.strip.empty?
              stats = EventMachine::HttpRequest.new(nginx_address + '/channels-stats').get :head => {'accept' => 'application/json'}, :timeout => 30
              stats.callback do
                stats.response_header.status.should eql(200)
                stats.response_header.content_length.should_not eql(0)
                response = JSON.parse(stats.response)
                response["subscribers"].to_i.should eql(1)
                response["channels"].to_i.should eql(1)
                pub = EventMachine::HttpRequest.new(nginx_address + '/pub?id=' + channel.to_s).delete :head => headers, :body => "custom channel delete message", :timeout => 30
                pub.callback do
                  pub.response_header.status.should eql(200)
                  pub.response_header.content_length.should eql(0)
                  pub.response_header['X_NGINX_PUSHSTREAM_EXPLAIN'].should eql("Channel deleted.")
                end
              end
            else
              response = JSON.parse(resp)
              response["channel"].should eql(channel)
              response["id"].to_i.should eql(-2)
              response["text"].should eql("custom channel delete message")

              stats = EventMachine::HttpRequest.new(nginx_address + '/channels-stats').get :head => {'accept' => 'application/json'}, :timeout => 30
              stats.callback do
                stats.response_header.status.should eql(200)
                stats.response_header.content_length.should_not eql(0)
                response = JSON.parse(stats.response)
                response["subscribers"].to_i.should eql(0)
                response["channels"].to_i.should eql(0)
              end
              EventMachine.stop
            end
          end
        end
      end
    end

    it "should delete a channel with subscriber in two channels" do
      channel_1 = 'test_delete_channel_whith_subscriber_in_two_channels_1'
      channel_2 = 'test_delete_channel_whith_subscriber_in_two_channels_2'
      stage1_complete = stage2_complete = false
      body = 'published message'

      configuration = config.merge({
        :header_template => " ", # send a space as header to has a chunk received
        :footer_template => nil,
        :ping_message_interval => nil,
        :message_template => '{\"id\":\"~id~\", \"channel\":\"~channel~\", \"text\":\"~text~\"}'
      })

      resp = ""
      nginx_run_server(configuration) do |conf|
        EventMachine.run do
          sub_1 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel_1.to_s + '/' + channel_2.to_s).get :head => headers
          sub_1.stream do |chunk|

            resp = resp + chunk
            if resp.strip.empty?
              stats = EventMachine::HttpRequest.new(nginx_address + '/channels-stats').get :head => {'accept' => 'application/json'}
              stats.callback do
                stats.should be_http_status(200).with_body
                response = JSON.parse(stats.response)
                response["subscribers"].to_i.should eql(1)
                response["channels"].to_i.should eql(2)

                pub = EventMachine::HttpRequest.new(nginx_address + '/pub?id=' + channel_1.to_s).delete :head => headers
                pub.callback do
                  pub.should be_http_status(200).without_body
                  pub.response_header['X_NGINX_PUSHSTREAM_EXPLAIN'].should eql("Channel deleted.")
                end
              end
            else
              if !stage1_complete
                stage1_complete = true
                response = JSON.parse(resp)
                response["channel"].should eql(channel_1)
                response["id"].to_i.should eql(-2)
                response["text"].should eql("Channel deleted")

                stats = EventMachine::HttpRequest.new(nginx_address + '/channels-stats').get :head => {'accept' => 'application/json'}
                stats.callback do
                  stats.should be_http_status(200).with_body
                  response = JSON.parse(stats.response)
                  response["subscribers"].to_i.should eql(1)
                  response["channels"].to_i.should eql(1)

                  pub = EventMachine::HttpRequest.new(nginx_address + '/pub?id=' + channel_2.to_s).post :head => headers, :body=> body
                  pub.callback do
                    pub.should be_http_status(200).with_body
                  end
                end
              elsif !stage2_complete
                stage2_complete = true
                response = JSON.parse(resp.split("\r\n")[2])
                response["channel"].should eql(channel_2)
                response["id"].to_i.should eql(1)
                response["text"].should eql(body)

                pub = EventMachine::HttpRequest.new(nginx_address + '/pub?id=' + channel_2.to_s).delete :head => headers
                pub.callback do
                  pub.should be_http_status(200).without_body
                  pub.response_header['X_NGINX_PUSHSTREAM_EXPLAIN'].should eql("Channel deleted.")
                end
              else
                response = JSON.parse(resp.split("\r\n")[3])
                response["channel"].should eql(channel_2)
                response["id"].to_i.should eql(-2)
                response["text"].should eql("Channel deleted")

                stats = EventMachine::HttpRequest.new(nginx_address + '/channels-stats').get :head => {'accept' => 'application/json'}
                stats.callback do
                  stats.should be_http_status(200).with_body
                  response = JSON.parse(stats.response)
                  response["subscribers"].to_i.should eql(0)
                  response["channels"].to_i.should eql(0)
                  EventMachine.stop
                end
              end
            end
          end
        end
      end
    end

    it "should delete channels with subscribers" do
      channel_1 = 'test_delete_channels_whith_subscribers_1'
      channel_2 = 'test_delete_channels_whith_subscribers_2'
      body = 'published message'

      configuration = config.merge({
        :header_template => nil,
        :footer_template => "FOOTER",
        :ping_message_interval => nil,
        :shared_memory_cleanup_objects_ttl => nil,
        :message_template => '{\"id\":\"~id~\", \"channel\":\"~channel~\", \"text\":\"~text~\"}'
      })

      nginx_run_server(configuration, :timeout => 10) do |conf|
        EventMachine.run do
          resp_1 = ""
          sub_1 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel_1.to_s).get :head => headers
          sub_1.stream do |chunk|
            resp_1 += chunk
          end
          sub_1.callback do
            resp_1.should eql("{\"id\":\"-2\", \"channel\":\"test_delete_channels_whith_subscribers_1\", \"text\":\"Channel deleted\"}\r\nFOOTER\r\n")
          end

          resp_2 = ""
          sub_2 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel_2.to_s).get :head => headers
          sub_2.stream do |chunk|
            resp_2 += chunk
          end
          sub_2.callback do
            resp_2.should eql("{\"id\":\"-2\", \"channel\":\"test_delete_channels_whith_subscribers_2\", \"text\":\"Channel deleted\"}\r\nFOOTER\r\n")
          end

          stats = EventMachine::HttpRequest.new(nginx_address + '/channels-stats').get :head => {'accept' => 'application/json'}
          stats.callback do
            stats.should be_http_status(200).with_body
            response = JSON.parse(stats.response)
            response["subscribers"].to_i.should eql(2)
            response["channels"].to_i.should eql(2)
          end

          pub_1 = EventMachine::HttpRequest.new(nginx_address + '/pub?id=' + channel_1.to_s).delete :head => headers
          pub_1.callback do
            pub_1.should be_http_status(200).without_body
            pub_1.response_header['X_NGINX_PUSHSTREAM_EXPLAIN'].should eql("Channel deleted.")
          end

          pub_2 = EventMachine::HttpRequest.new(nginx_address + '/pub?id=' + channel_2.to_s).delete :head => headers
          pub_2.callback do
            pub_2.should be_http_status(200).without_body
            pub_2.response_header['X_NGINX_PUSHSTREAM_EXPLAIN'].should eql("Channel deleted.")
          end

          EM.add_timer(5) do
            stats_2 = EventMachine::HttpRequest.new(nginx_address + '/channels-stats').get :head => {'accept' => 'application/json'}
            stats_2.callback do
              stats_2.should be_http_status(200).with_body
              response = JSON.parse(stats_2.response)
              response["subscribers"].to_i.should eql(0)
              response["channels"].to_i.should eql(0)
              EventMachine.stop
            end
          end
        end
      end
    end

    it "should receive footer template when channel is deleted" do
      channel = 'ch_test_receive_footer_template_when_channel_is_deleted'
      body = 'published message'

      configuration = config.merge({
        :header_template => "HEADER_TEMPLATE",
        :footer_template => "FOOTER_TEMPLATE",
        :ping_message_interval => nil,
        :message_template => '~text~'
      })

      resp = ""
      nginx_run_server(configuration) do |conf|
        EventMachine.run do
          sub_1 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => headers
          sub_1.stream do |chunk|

            resp = resp + chunk
            if resp == "#{conf.header_template}\r\n"
              pub = EventMachine::HttpRequest.new(nginx_address + '/pub?id=' + channel.to_s).delete :head => headers
              pub.callback do
                pub.should be_http_status(200).without_body
                pub.response_header['X_NGINX_PUSHSTREAM_EXPLAIN'].should eql("Channel deleted.")
              end
            end
          end
          sub_1.callback do
            resp.should eql("#{conf.header_template}\r\nChannel deleted\r\n#{conf.footer_template}\r\n")
            EventMachine.stop
          end
        end
      end
    end

    it "should receive different header and footer template by location when channel is deleted" do
      channel = 'ch_test_different_header_and_footer_template_by_location'
      body = 'published message'

      configuration = config.merge({
        :header_template => "HEADER_TEMPLATE",
        :footer_template => "FOOTER_TEMPLATE",
        :ping_message_interval => nil,
        :message_template => '~text~',
        :extra_location => %{
          location ~ /sub2/(.*)? {
              # activate subscriber mode for this location
              push_stream_subscriber;

              # positional channel path
              push_stream_channels_path          $1;
              push_stream_header_template "<html><body>";
              push_stream_footer_template "</body></html>";
              push_stream_message_template "|~text~|";
          }
        }
      })

      resp = ""
      resp2 = ""
      nginx_run_server(configuration) do |conf|
        EventMachine.run do
          sub_1 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => headers
          sub_1.stream do |chunk|
            resp = resp + chunk
          end

          sub_2 = EventMachine::HttpRequest.new(nginx_address + '/sub2/' + channel.to_s).get :head => headers
          sub_2.stream do |chunk|
            resp2 = resp2 + chunk
          end

          EM.add_timer(1) do
            pub = EventMachine::HttpRequest.new(nginx_address + '/pub?id=' + channel.to_s).delete :head => headers
            pub.callback do
              pub.should be_http_status(200).without_body
              pub.response_header['X_NGINX_PUSHSTREAM_EXPLAIN'].should eql("Channel deleted.")
            end
          end

          EM.add_timer(2) do
            resp.should eql("#{conf.header_template}\r\nChannel deleted\r\n#{conf.footer_template}\r\n")
            resp2.should eql("<html><body>\r\n|Channel deleted|\r\n</body></html>\r\n")
            EventMachine.stop
          end
        end
      end
    end

    it "should receive custom delete message text when channel is deleted" do
      channel = 'test_custom_channel_deleted_message_text'
      body = 'published message'

      configuration = config.merge({
        :header_template => " ", # send a space as header to has a chunk received
        :footer_template => nil,
        :ping_message_interval => nil,
        :message_template => '{\"id\":\"~id~\", \"channel\":\"~channel~\", \"text\":\"~text~\"}',
        :channel_deleted_message_text => "Channel has gone away."
      })

      resp = ""
      nginx_run_server(configuration) do |conf|
        EventMachine.run do
          sub_1 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => headers
          sub_1.stream do |chunk|

            resp = resp + chunk
            if resp.strip.empty?
              pub = EventMachine::HttpRequest.new(nginx_address + '/pub?id=' + channel.to_s).delete :head => headers
              pub.callback do
                pub.should be_http_status(200).without_body
                pub.response_header['X_NGINX_PUSHSTREAM_EXPLAIN'].should eql("Channel deleted.")
              end
            else
              response = JSON.parse(resp)
              response["channel"].should eql(channel)
              response["id"].to_i.should eql(-2)
              response["text"].should eql(conf.channel_deleted_message_text)
              EventMachine.stop
            end
          end
        end
      end
    end

  end
end
