require 'spec_helper'

describe "Subscriber Properties" do
  let(:config) do
    {
      :authorized_channels_only => "off",
      :header_template => "HEADER\r\nTEMPLATE\r\n1234\r\n",
      :content_type => "custom content type",
      :subscriber_connection_ttl => "1s",
      :ping_message_interval => "2s"
    }
  end

  it "should not accept access without a channel path" do
    nginx_run_server(config) do |conf|
      EventMachine.run do
        sub = EventMachine::HttpRequest.new(nginx_address + '/sub/').get :head => headers
        sub.callback do
          expect(sub).to be_http_status(400).without_body
          expect(sub.response_header['X_NGINX_PUSHSTREAM_EXPLAIN']).to eql("No channel id provided.")
          EventMachine.stop
        end
      end
    end
  end

  it "should check accepted methods" do
    nginx_run_server(config) do |conf|
      # testing OPTIONS method, EventMachine::HttpRequest does not have support to it
      socket = open_socket(nginx_host, nginx_port)
      socket.print("OPTIONS /sub/ch_test_accepted_methods_0 HTTP/1.0\r\n\r\n")
      headers, body = read_response_on_socket(socket)
      expect(headers).to match_the_pattern(/HTTP\/1\.1 200 OK/)
      expect(headers).to match_the_pattern(/Content-Length: 0/)
      socket.close

      EventMachine.run do
        multi = EventMachine::MultiRequest.new

        multi.add(:a, EventMachine::HttpRequest.new(nginx_address + '/sub/ch_test_accepted_methods_1').head)
        multi.add(:b, EventMachine::HttpRequest.new(nginx_address + '/sub/ch_test_accepted_methods_2').put(:body => 'body'))
        multi.add(:c, EventMachine::HttpRequest.new(nginx_address + '/sub/ch_test_accepted_methods_3').post)
        multi.add(:d, EventMachine::HttpRequest.new(nginx_address + '/sub/ch_test_accepted_methods_4').delete)
        multi.add(:e, EventMachine::HttpRequest.new(nginx_address + '/sub/ch_test_accepted_methods_5').get)

        multi.callback do
          expect(multi.responses[:callback].length).to eql(5)

          expect(multi.responses[:callback][:a]).to be_http_status(405)
          expect(multi.responses[:callback][:a].req.method).to eql("HEAD")
          expect(multi.responses[:callback][:a].response_header['ALLOW']).to eql("GET")

          expect(multi.responses[:callback][:b]).to be_http_status(405)
          expect(multi.responses[:callback][:b].req.method).to eql("PUT")
          expect(multi.responses[:callback][:b].response_header['ALLOW']).to eql("GET")

          expect(multi.responses[:callback][:c]).to be_http_status(405)
          expect(multi.responses[:callback][:c].req.method).to eql("POST")
          expect(multi.responses[:callback][:c].response_header['ALLOW']).to eql("GET")

          expect(multi.responses[:callback][:d]).to be_http_status(405)
          expect(multi.responses[:callback][:d].req.method).to eql("DELETE")
          expect(multi.responses[:callback][:d].response_header['ALLOW']).to eql("GET")

          expect(multi.responses[:callback][:e]).not_to be_http_status(405)
          expect(multi.responses[:callback][:e].req.method).to eql("GET")

          EventMachine.stop
        end
      end
    end
  end

  it "should not accept access to a channel with id 'ALL'" do
    channel = 'ALL'

    nginx_run_server(config) do |conf|
      EventMachine.run do
        sub_1 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => headers
        sub_1.callback do
          expect(sub_1).to be_http_status(403).without_body
          expect(sub_1.response_header['X_NGINX_PUSHSTREAM_EXPLAIN']).to eql("Channel id not authorized for this method.")
          EventMachine.stop
        end
      end
    end
  end

  it "should not accept access to a channel with id containing wildcard" do
    channel_1 = 'abcd*efgh'
    channel_2 = '*abcdefgh'
    channel_3 = 'abcdefgh*'

    nginx_run_server(config) do |conf|
      EventMachine.run do
        multi = EventMachine::MultiRequest.new

        multi.add(:a, EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel_1).get(:head => headers))
        multi.add(:b, EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel_2).get(:head => headers))
        multi.add(:c, EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel_3).get(:head => headers))
        multi.callback do
          expect(multi.responses[:callback].length).to eql(3)
          multi.responses[:callback].each do |name, response|
            expect(response).to be_http_status(403).without_body
            expect(response.response_header['X_NGINX_PUSHSTREAM_EXPLAIN']).to eql("Channel id not authorized for this method.")
          end

          EventMachine.stop
        end
      end
    end
  end

  it "should accept access to multiple channels" do
    nginx_run_server(config) do |conf|
      EventMachine.run do
        multi = EventMachine::MultiRequest.new

        multi.add(:a, EventMachine::HttpRequest.new(nginx_address + '/sub/ch_multi_channels_1').get)
        multi.add(:b, EventMachine::HttpRequest.new(nginx_address + '/sub/ch_multi_channels_1.b10').get)
        multi.add(:c, EventMachine::HttpRequest.new(nginx_address + '/sub/ch_multi_channels_2/ch_multi_channels_3').get)
        multi.add(:d, EventMachine::HttpRequest.new(nginx_address + '/sub/ch_multi_channels_2.b2/ch_multi_channels_3').get)
        multi.add(:e, EventMachine::HttpRequest.new(nginx_address + '/sub/ch_multi_channels_2/ch_multi_channels_3.b3').get)
        multi.add(:f, EventMachine::HttpRequest.new(nginx_address + '/sub/ch_multi_channels_2.b2/ch_multi_channels_3.b3').get)
        multi.add(:g, EventMachine::HttpRequest.new(nginx_address + '/sub/ch_multi_channels_4.b').get)

        multi.callback do
          expect(multi.responses[:callback].length).to eql(7)
          multi.responses[:callback].each do |name, response|
            expect(response).to be_http_status(200)
          end

          EventMachine.stop
        end
      end
    end
  end

  it "should not accept access with a big channel id" do
    channel = '123456'

    nginx_run_server(config.merge(:max_channel_id_length => 5)) do |conf|
      EventMachine.run do
        sub = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s ).get :head => headers
        sub.callback do
          expect(sub).to be_http_status(400).without_body
          expect(sub.response_header['X_NGINX_PUSHSTREAM_EXPLAIN']).to eql("Channel id is too large.")
          EventMachine.stop
        end
      end
    end
  end

  it "should not accept access to a wildcard channel without a normal channel" do
    nginx_run_server(config.merge(:wildcard_channel_prefix => "bd_")) do |conf|
      EventMachine.run do
        multi = EventMachine::MultiRequest.new

        multi.add(:a, EventMachine::HttpRequest.new(nginx_address + '/sub/bd_test_wildcard_channels_without_common_channel').get)
        multi.add(:b, EventMachine::HttpRequest.new(nginx_address + '/sub/bd_').get)
        multi.add(:c, EventMachine::HttpRequest.new(nginx_address + '/sub/bd1').get)
        multi.add(:d, EventMachine::HttpRequest.new(nginx_address + '/sub/bd').get)

        multi.callback do
          expect(multi.responses[:callback].length).to eql(4)

          expect(multi.responses[:callback][:a]).to be_http_status(403).without_body
          expect(multi.responses[:callback][:a].response_header['X_NGINX_PUSHSTREAM_EXPLAIN']).to eql("Subscribed too much wildcard channels.")
          expect(multi.responses[:callback][:a].req.uri.to_s).to eql(nginx_address + '/sub/bd_test_wildcard_channels_without_common_channel')

          expect(multi.responses[:callback][:b]).to be_http_status(403).without_body
          expect(multi.responses[:callback][:b].response_header['X_NGINX_PUSHSTREAM_EXPLAIN']).to eql("Subscribed too much wildcard channels.")
          expect(multi.responses[:callback][:b].req.uri.to_s).to eql(nginx_address + '/sub/bd_')

          expect(multi.responses[:callback][:c]).to be_http_status(200)
          expect(multi.responses[:callback][:c].req.uri.to_s).to eql(nginx_address + '/sub/bd1')

          expect(multi.responses[:callback][:d]).to be_http_status(200)
          expect(multi.responses[:callback][:d].req.uri.to_s).to eql(nginx_address + '/sub/bd')

          EventMachine.stop
        end
      end
    end
  end

  it "should accept access to a wildcard channel with a normal channel" do
    nginx_run_server(config.merge(:wildcard_channel_prefix => "bd_", :wildcard_channel_max_qtd => 2, :authorized_channels_only => "off")) do |conf|
      EventMachine.run do
        multi = EventMachine::MultiRequest.new

        multi.add(:a, EventMachine::HttpRequest.new(nginx_address + '/sub/bd1/bd2/bd3/bd4/bd_1/bd_2/bd_3').get)
        multi.add(:b, EventMachine::HttpRequest.new(nginx_address + '/sub/bd1/bd2/bd_1/bd_2').get)
        multi.add(:c, EventMachine::HttpRequest.new(nginx_address + '/sub/bd1/bd_1').get)
        multi.add(:d, EventMachine::HttpRequest.new(nginx_address + '/sub/bd1/bd2').get)

        multi.callback do
          expect(multi.responses[:callback].length).to eql(4)

          expect(multi.responses[:callback][:a]).to be_http_status(403).without_body
          expect(multi.responses[:callback][:a].response_header['X_NGINX_PUSHSTREAM_EXPLAIN']).to eql("Subscribed too much wildcard channels.")
          expect(multi.responses[:callback][:a].req.uri.to_s).to eql(nginx_address + '/sub/bd1/bd2/bd3/bd4/bd_1/bd_2/bd_3')

          expect(multi.responses[:callback][:b]).to be_http_status(200)
          expect(multi.responses[:callback][:b].req.uri.to_s).to eql(nginx_address + '/sub/bd1/bd2/bd_1/bd_2')

          expect(multi.responses[:callback][:c]).to be_http_status(200)
          expect(multi.responses[:callback][:c].req.uri.to_s).to eql(nginx_address + '/sub/bd1/bd_1')

          expect(multi.responses[:callback][:d]).to be_http_status(200)
          expect(multi.responses[:callback][:d].req.uri.to_s).to eql(nginx_address + '/sub/bd1/bd2')

          EventMachine.stop
        end
      end
    end
  end

  it "should not accept access to an nonexistent channel with authorized only 'on'" do
    channel = 'ch_test_subscribe_an_absent_channel_with_authorized_only_on'

    nginx_run_server(config.merge(:authorized_channels_only => 'on')) do |conf|
      EventMachine.run do
        sub_1 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => headers
        sub_1.callback do
          expect(sub_1).to be_http_status(403).without_body
          expect(sub_1.response_header['X_NGINX_PUSHSTREAM_EXPLAIN']).to eql("Subscriber could not create channels.")
          EventMachine.stop
        end
      end
    end
  end

  it "should accept access to an existent channel with authorized channel only 'on'" do
    channel = 'ch_test_subscribe_an_existing_channel_with_authorized_only_on'
    body = 'body'

    nginx_run_server(config.merge(:authorized_channels_only => 'on')) do |conf|
      #create channel
      publish_message(channel, headers, body)

      EventMachine.run do
        sub_1 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => headers
        sub_1.callback do
          expect(sub_1).to be_http_status(200)
          EventMachine.stop
        end
      end
    end
  end

  it "should accept access to an existing channel and a nonexistent wildcard channel with authorized only 'on'" do
    channel = 'ch_test_subscribe_an_existing_channel_and_absent_wildcard_channel_with_authorized_only_on'
    wildcard_channel = 'bd_test_subscribe_an_existing_channel_and_absent_wildcard_channel_with_authorized_only_on'

    body = 'body'

    nginx_run_server(config.merge(:authorized_channels_only => 'on', :wildcard_channel_prefix => "bd_", :wildcard_channel_max_qtd => 1)) do |conf|
      #create channel
      publish_message(channel, headers, body)

      EventMachine.run do
        sub_1 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s + '/' + wildcard_channel.to_s).get :head => headers
        sub_1.callback do
          expect(sub_1).to be_http_status(200)
          EventMachine.stop
        end
      end
    end
  end

  it "should not accept access to an existing channel without messages with authorized only 'on'" do
    channel = 'ch_test_subscribe_an_existing_channel_without_messages_and_with_authorized_only_on'

    body = 'body'

    nginx_run_server(config.merge(:authorized_channels_only => 'on', :message_ttl => "1s"), :timeout => 10) do |conf|
      #create channel
      publish_message(channel, headers, body)
      sleep(5) #to ensure message was gone

      EventMachine.run do
        sub_1 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => headers
        sub_1.callback do
          expect(sub_1).to be_http_status(403).without_body
          expect(sub_1.response_header['X_NGINX_PUSHSTREAM_EXPLAIN']).to eql("Subscriber could not create channels.")
          EventMachine.stop
        end
      end
    end
  end

  it "should not accept access to an existing channel without messages and an nonexistent wildcard channel with authorized only 'on'" do
    channel = 'ch_test_subscribe_an_existing_channel_without_messages_and_absent_wildcard_channel_and_with_authorized_only_on_should_fail'
    wildcard_channel = 'bd_test_subscribe_an_existing_channel_without_messages_and_absent_wildcard_channel_and_with_authorized_only_on_should_fail'

    body = 'body'

    nginx_run_server(config.merge(:authorized_channels_only => 'on', :message_ttl => "1s", :wildcard_channel_prefix => "bd_", :wildcard_channel_max_qtd => 1), :timeout => 10) do |conf|
      #create channel
      publish_message(channel, headers, body)
      sleep(5) #to ensure message was gone

      EventMachine.run do
        sub_1 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s + '/' + wildcard_channel.to_s).get :head => headers
        sub_1.callback do
          expect(sub_1).to be_http_status(403).without_body
          expect(sub_1.response_header['X_NGINX_PUSHSTREAM_EXPLAIN']).to eql("Subscriber could not create channels.")
          EventMachine.stop
        end
      end
    end
  end

  it "should receive new messages in a multi channel subscriber" do
    channel_1 = 'test_retreive_new_messages_in_multichannel_subscribe_1'
    channel_2 = 'test_retreive_new_messages_in_multich_subscribe_2'
    channel_3 = 'test_retreive_new_messages_in_multchannel_subscribe_3'
    channel_4 = 'test_retreive_new_msgs_in_multichannel_subscribe_4'
    channel_5 = 'test_retreive_new_messages_in_multichannel_subs_5'
    channel_6 = 'test_retreive_new_msgs_in_multichannel_subs_6'

    body = 'body'

    response = ""
    nginx_run_server(config.merge(:header_template => nil, :message_template => '{\"channel\":\"~channel~\", \"id\":\"~id~\", \"message\":\"~text~\"}|')) do |conf|
      EventMachine.run do
        sub_1 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel_1.to_s + '/' + channel_2.to_s + '/' + channel_3.to_s + '/' + channel_4.to_s + '/' + channel_5.to_s + '/' + channel_6.to_s).get :head => headers
        sub_1.stream do |chunk|
          response += chunk
          lines = response.split("|")

          if lines.length >= 6
            line = JSON.parse(lines[0])
            expect(line['channel']).to eql(channel_1.to_s)
            expect(line['message']).to eql('body' + channel_1.to_s)
            expect(line['id'].to_i).to eql(1)

            line = JSON.parse(lines[1])
            expect(line['channel']).to eql(channel_2.to_s)
            expect(line['message']).to eql('body' + channel_2.to_s)
            expect(line['id'].to_i).to eql(1)

            line = JSON.parse(lines[2])
            expect(line['channel']).to eql(channel_3.to_s)
            expect(line['message']).to eql('body' + channel_3.to_s)
            expect(line['id'].to_i).to eql(1)

            line = JSON.parse(lines[3])
            expect(line['channel']).to eql(channel_4.to_s)
            expect(line['message']).to eql('body' + channel_4.to_s)
            expect(line['id'].to_i).to eql(1)

            line = JSON.parse(lines[4])
            expect(line['channel']).to eql(channel_5.to_s)
            expect(line['message']).to eql('body' + channel_5.to_s)
            expect(line['id'].to_i).to eql(1)

            line = JSON.parse(lines[5])
            expect(line['channel']).to eql(channel_6.to_s)
            expect(line['message']).to eql('body' + channel_6.to_s)
            expect(line['id'].to_i).to eql(1)

            EventMachine.stop
          end
        end

        EM.add_timer(0.5) do
          publish_message(channel_1, headers, body + channel_1.to_s)
          publish_message(channel_2, headers, body + channel_2.to_s)
          publish_message(channel_3, headers, body + channel_3.to_s)
          publish_message(channel_4, headers, body + channel_4.to_s)
          publish_message(channel_5, headers, body + channel_5.to_s)
          publish_message(channel_6, headers, body + channel_6.to_s)
        end
      end
    end
  end

  it "should limit the number of channels" do
    channel = 'ch_test_max_number_of_channels_'

    nginx_run_server(config.merge(:max_number_of_channels => 1)) do |conf|
      EventMachine.run do
        sub_1 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s + 1.to_s).get :head => headers
        sub_1.stream do
          expect(sub_1).to be_http_status(200)

          sub_2 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s + 2.to_s).get :head => headers
          sub_2.callback do
            expect(sub_2).to be_http_status(403).without_body
            expect(sub_2.response_header['X_NGINX_PUSHSTREAM_EXPLAIN']).to eql("Number of channels were exceeded.")
            EventMachine.stop
          end
        end
      end
    end
  end

  it "should limit the number of wildcard channels" do
    channel = 'bd_test_max_number_of_wildcard_channels_'

    nginx_run_server(config.merge(:max_number_of_wildcard_channels => 1, :wildcard_channel_prefix => 'bd_', :wildcard_channel_max_qtd => 1)) do |conf|
      EventMachine.run do
        sub_1 = EventMachine::HttpRequest.new(nginx_address + '/sub/ch1/' + channel.to_s + 1.to_s).get :head => headers
        sub_1.stream do
          expect(sub_1).to be_http_status(200)

          sub_2 = EventMachine::HttpRequest.new(nginx_address + '/sub/ch1/' + channel.to_s + 2.to_s).get :head => headers
          sub_2.callback do
            expect(sub_2).to be_http_status(403).without_body
            expect(sub_2.response_header['X_NGINX_PUSHSTREAM_EXPLAIN']).to eql("Number of channels were exceeded.")
            EventMachine.stop
          end
        end
      end
    end
  end

  it "should accept different message templates in each location" do
    configuration = config.merge({
      :message_template => '{\"text\":\"~text~\"}',
      :header_template => nil,
      :extra_location => %q{
        location ~ /sub2/(.*)? {
          # activate subscriber mode for this location
          push_stream_subscriber;

          # positional channel path
          push_stream_channels_path               $1;
          # message template
          push_stream_message_template "{\"msg\":\"~text~\"}";
        }

      }
    })

    channel = 'ch_test_different_message_templates'
    body = 'body'

    nginx_run_server(configuration) do |conf|
      EventMachine.run do
        sub_1 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => headers
        sub_1.stream do |chunk|
          response = JSON.parse(chunk)
          expect(response['msg']).to be_nil
          expect(response['text']).to eql(body)
          EventMachine.stop
        end

        sub_2 = EventMachine::HttpRequest.new(nginx_address + '/sub2/' + channel.to_s + '.b1').get :head => headers
        sub_2.stream do |chunk|
          response = JSON.parse(chunk)
          expect(response['text']).to be_nil
          expect(response['msg']).to eql(body)
          EventMachine.stop
        end

        #publish a message
        publish_message_inline(channel, headers, body)
      end

      EventMachine.run do
        sub_3 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s + '.b1').get :head => headers
        sub_3.stream do |chunk|
          response = JSON.parse(chunk)
          expect(response['msg']).to be_nil
          expect(response['text']).to eql(body)
          EventMachine.stop
        end
      end

      EventMachine.run do
        sub_4 = EventMachine::HttpRequest.new(nginx_address + '/sub2/' + channel.to_s + '.b1').get :head => headers
        sub_4.stream do |chunk|
          response = JSON.parse(chunk)
          expect(response['text']).to be_nil
          expect(response['msg']).to eql(body)
          EventMachine.stop
        end
      end
    end
  end

  it "should use default message template" do
    channel = 'ch_test_default_message_template'
    body = 'body'

    nginx_run_server(config.merge(:message_template => nil, :header_template => nil)) do |conf|
      EventMachine.run do
        sub_1 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => headers
        sub_1.stream do |chunk|
          expect(chunk).to eql("#{body}")
          EventMachine.stop
        end

        #publish a message
        publish_message_inline(channel, headers, body)
      end
    end
  end

  it "should receive default ping message with default message template" do
    channel = 'ch_test_default_ping_message_with_default_message_template'
    body = 'body'

    nginx_run_server(config.merge(:subscriber_connection_ttl => nil, :message_template => nil, :header_template => nil, :ping_message_interval => '1s', :ping_message_text => nil)) do |conf|
      EventMachine.run do
        sub_1 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => headers
        sub_1.stream do |chunk|
          expect(chunk).to eql(" ")
          EventMachine.stop
        end
      end
    end
  end

  it "should receive custom ping message with default message template" do
    channel = 'ch_test_custom_ping_message_with_default_message_template'
    body = 'body'

    nginx_run_server(config.merge(:subscriber_connection_ttl => nil, :message_template => nil, :header_template => nil, :ping_message_interval => '1s', :ping_message_text => "pinging you!!!")) do |conf|
      EventMachine.run do
        sub_1 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => headers
        sub_1.stream do |chunk|
          expect(chunk).to eql(conf.ping_message_text)
          EventMachine.stop
        end
      end
    end
  end

  it "should receive default ping message with custom message template" do
    channel = 'ch_test_default_ping_message_with_custom_message_template'
    body = 'body'

    nginx_run_server(config.merge(:subscriber_connection_ttl => nil, :message_template => "~id~:~text~", :header_template => nil, :ping_message_interval => '1s', :ping_message_text => nil)) do |conf|
      EventMachine.run do
        sub_1 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => headers
        sub_1.stream do |chunk|
          expect(chunk).to eql("-1: ")
          EventMachine.stop
        end
      end
    end
  end

  it "should receive custom ping message with custom message template" do
    channel = 'ch_test_custom_ping_message_with_default_message_template'
    body = 'body'

    nginx_run_server(config.merge(:subscriber_connection_ttl => nil, :message_template => "~id~:~text~", :header_template => nil, :ping_message_interval => '1s', :ping_message_text => "pinging you!!!")) do |conf|
      EventMachine.run do
        sub_1 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => headers
        sub_1.stream do |chunk|
          expect(chunk).to eql("-1:#{conf.ping_message_text}")
          EventMachine.stop
        end
      end
    end
  end

  it "should receive transfer enconding as 'chunked'" do
    channel = 'ch_test_transfer_encoding_chuncked'

    nginx_run_server(config) do |conf|
      EventMachine.run do
        sub_1 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => headers
        sub_1.stream do |chunk|
          expect(sub_1.response_header['TRANSFER_ENCODING']).to eql("chunked")
          EventMachine.stop
        end
      end
    end
  end

  it "should limit the number of subscribers to one channel" do
    channel = 'ch_test_cannot_add_more_subscriber_to_one_channel_than_allowed'
    other_channel = 'ch_test_cannot_add_more_subscriber_to_one_channel_than_allowed_2'

    nginx_run_server(config.merge(:max_subscribers_per_channel => 3, :subscriber_connection_ttl => "3s")) do |conf|
      EventMachine.run do
        EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get(:head => headers).stream do
          EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get(:head => headers).stream do
            EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get(:head => headers).stream do
              sub_4 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => headers
              sub_4.callback do
                expect(sub_4).to be_http_status(403).without_body
                expect(sub_4.response_header['X_NGINX_PUSHSTREAM_EXPLAIN']).to eql("Subscribers limit per channel has been exceeded.")

                sub_5 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + other_channel.to_s).get :head => headers
                sub_5.callback do
                  expect(sub_5).to be_http_status(200)
                  EventMachine.stop
                end
              end
            end
          end
        end
      end
    end
  end

  it "should accept channels with '.b' in the name" do
    channel = 'room.b18.beautiful'
    response = ''

    nginx_run_server(config.merge(:ping_message_interval => nil, :header_template => nil, :footer_template => nil, :message_template => nil)) do |conf|
      EventMachine.run do
        publish_message(channel, {'accept' => 'text/html'}, 'msg 1')
        publish_message(channel, {'accept' => 'text/html'}, 'msg 2')
        publish_message(channel, {'accept' => 'text/html'}, 'msg 3')
        publish_message(channel, {'accept' => 'text/html'}, 'msg 4')

        sub = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s + '.b3').get
        sub.stream do |chunk|
          response += chunk
        end
        sub.callback do
          expect(response).to eql("msg 2msg 3msg 4")

          response = ''
          sub_1 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get
          sub_1.stream do |chunk|
            response += chunk
          end
          sub_1.callback do
            expect(response).to eql("msg 5")

            EventMachine.stop
          end

          publish_message_inline(channel, {'accept' => 'text/html'}, 'msg 5')
        end
      end
    end
  end

  it "should not receive acess control allow headers by default" do
    channel = 'test_access_control_allow_headers'

    nginx_run_server(config) do |conf|
      EventMachine.run do
        sub_1 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => headers
        sub_1.stream do |chunk|
          expect(sub_1.response_header['ACCESS_CONTROL_ALLOW_ORIGIN']).to be_nil
          expect(sub_1.response_header['ACCESS_CONTROL_ALLOW_METHODS']).to be_nil
          expect(sub_1.response_header['ACCESS_CONTROL_ALLOW_HEADERS']).to be_nil

          EventMachine.stop
        end
      end
    end
  end

  context "when allow origin directive is set" do
    it "should receive acess control allow headers" do
      channel = 'test_access_control_allow_headers'

      nginx_run_server(config.merge(:allowed_origins => "custom.domain.com")) do |conf|
        EventMachine.run do
          sub_1 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => headers
          sub_1.stream do |chunk|
            expect(sub_1.response_header['ACCESS_CONTROL_ALLOW_ORIGIN']).to eql("custom.domain.com")
            expect(sub_1.response_header['ACCESS_CONTROL_ALLOW_METHODS']).to eql("GET")
            expect(sub_1.response_header['ACCESS_CONTROL_ALLOW_HEADERS']).to eql("If-Modified-Since,If-None-Match,Etag,Event-Id,Event-Type,Last-Event-Id")

            EventMachine.stop
          end
        end
      end
    end

    it "should accept a complex value" do
      channel = 'test_access_control_allow_origin_as_complex'

      nginx_run_server(config.merge(:allowed_origins => "$arg_domain")) do |conf|
        EventMachine.run do
          sub_1 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s + '?domain=test.com').get :head => headers
          sub_1.stream do |chunk|
            expect(sub_1.response_header['ACCESS_CONTROL_ALLOW_ORIGIN']).to eql("test.com")
            expect(sub_1.response_header['ACCESS_CONTROL_ALLOW_METHODS']).to eql("GET")
            expect(sub_1.response_header['ACCESS_CONTROL_ALLOW_HEADERS']).to eql("If-Modified-Since,If-None-Match,Etag,Event-Id,Event-Type,Last-Event-Id")

            EventMachine.stop
          end
        end
      end
    end
  end

  it "should receive the configured header template" do
    channel = 'ch_test_header_template'

    nginx_run_server(config) do |conf|
      EventMachine.run do
        sub = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => headers
        sub.stream do |chunk|
          expect(chunk).to eql("#{conf.header_template}")
          EventMachine.stop
        end
      end
    end
  end

  context "when header template file is set" do
    before do
      FileUtils.mkdir_p nginx_tests_tmp_dir
      File.open(header_template_file, "w") {|f| f.write header_template_content }
    end
    after { File.delete(header_template_file) }

    let(:header_template_file) { File.join(nginx_tests_tmp_dir, "header_template.txt") }
    let(:header_template_content) { "Header\nTemplate\ninside a file" }

    def assert_response_for(cfg, path, expected_response)
      nginx_run_server(cfg) do |conf|
        EventMachine.run do
          sub = EventMachine::HttpRequest.new(nginx_address + path).get :head => headers
          sub.stream do |chunk|
            expect(chunk).to eql(expected_response)
            EventMachine.stop
          end
        end
      end
    end

    it "should receive the file content" do
      channel = 'ch_test_header_template_file'
      merged_config = config.merge({
        header_template: nil,
        header_template_file: header_template_file
      })

      assert_response_for(merged_config, '/sub/' + channel.to_s, header_template_content)
    end

    it "should not accept header_template and header_template_file on same level" do
      merged_config = config.merge({
        :header_template => nil,
        :extra_location => %{
          location /sub2 {
            push_stream_subscriber;

            push_stream_header_template 'inline header template\\r\\n\\r\\n';
            push_stream_header_template_file #{header_template_file};
          }
        }
      })

      expect(nginx_test_configuration(merged_config)).to include(%{"push_stream_header_template_file" directive is duplicate or template set by 'push_stream_header_template'})
    end

    it "should not accept header_template_file and header_template on same level" do
      merged_config = config.merge({
        :header_template => nil,
        :extra_location => %{
          location /sub2 {
            push_stream_subscriber;

            push_stream_header_template_file #{header_template_file};
            push_stream_header_template 'inline header template\\r\\n\\r\\n';
          }
        }
      })

      expect(nginx_test_configuration(merged_config)).to include(%{"push_stream_header_template" directive is duplicate})
    end

    it "should accept header_template_file and header_template on different levels" do
      channel = 'ch_test_override_header_template_file'

      merged_config = config.merge({
        :header_template_file => header_template_file,
        :header_template => nil,
        :extra_location => %{
          location ~ /sub2/(.*) {
            push_stream_subscriber;
            push_stream_channels_path $1;

            push_stream_header_template 'inline header template';
          }
        }
      })

      assert_response_for(merged_config, '/sub/' + channel.to_s, header_template_content)
      assert_response_for(merged_config, '/sub2/' + channel.to_s, 'inline header template')
    end

    it "should accept header_template and header_template_file on different levels" do
      channel = 'ch_test_override_header_template_file'

      merged_config = config.merge({
        :header_template => 'inline header template',
        :extra_location => %{
          location ~ /sub2/(.*) {
            push_stream_subscriber;
            push_stream_channels_path $1;

            push_stream_header_template_file #{header_template_file};
          }
        }
      })

      assert_response_for(merged_config, '/sub/' + channel.to_s, 'inline header template')
      assert_response_for(merged_config, '/sub2/' + channel.to_s, header_template_content)
    end

    it "should return error when could not open the file" do
      merged_config = config.merge({
        :header_template => nil,
        :header_template_file => "/unexistent/path"
      })

      expect(nginx_test_configuration(merged_config)).to include(%{push stream module: unable to open file "/unexistent/path" for header template})
    end
  end

  it "should receive the configured content type" do
    channel = 'ch_test_content_type'

    nginx_run_server(config) do |conf|
      EventMachine.run do
        sub = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => headers
        sub.stream do |chunk|
          expect(sub.response_header['CONTENT_TYPE']).to eql(conf.content_type)
          EventMachine.stop
        end
      end
    end
  end

  it "should receive ping message on the configured ping message interval" do
    channel = 'ch_test_ping_message_interval'

    step1 = step2 = step3 = step4 = nil
    chunks_received = 0

    nginx_run_server(config.merge(:subscriber_connection_ttl => nil), :timeout => 10) do |conf|
      EventMachine.run do
        sub = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => headers
        sub.stream do |chunk|
          chunks_received += 1;
          step1 = Time.now if chunks_received == 1
          step2 = Time.now if chunks_received == 2
          step3 = Time.now if chunks_received == 3
          step4 = Time.now if chunks_received == 4
          EventMachine.stop if chunks_received == 4
        end
        sub.callback do
          expect(chunks_received).to eql(4)
          expect(time_diff_sec(step2, step1).round).to eql(time_diff_sec(step4, step3).round)
        end
      end
    end
  end

  it "should not cache the response" do
    channel = 'ch_test_not_cache_the_response'

    nginx_run_server(config.merge(:subscriber_connection_ttl => '1s')) do |conf|
      EventMachine.run do
        sub_1 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => headers
        sub_1.callback do
          expect(sub_1.response_header["EXPIRES"]).to eql("Thu, 01 Jan 1970 00:00:01 GMT")
          expect(sub_1.response_header["CACHE_CONTROL"]).to eql("no-cache, no-store, must-revalidate")
          EventMachine.stop
        end
      end
    end
  end

  it "should accept channels path inside an if block" do
    merged_config = config.merge({
      :header_template => nil,
      :footer_template => nil,
      :subscriber_connection_ttl => '1s',
      :extra_location => %{
        location /sub2 {
          push_stream_subscriber;

          push_stream_channels_path            $arg_id;
          if ($arg_test) {
            push_stream_channels_path          test_$arg_id;
          }
        }
      }
    })

    channel = 'channels_path_inside_if_block'
    body = 'published message'
    resp_1 = ""
    resp_2 = ""

    nginx_run_server(merged_config) do |conf|
      EventMachine.run do
        sub_1 = EventMachine::HttpRequest.new(nginx_address + '/sub2?id=' + channel.to_s).get :head => headers
        sub_1.stream do |chunk|
          resp_1 += chunk
        end

        sub_2 = EventMachine::HttpRequest.new(nginx_address + '/sub2?id=' + channel.to_s + '&test=1').get :head => headers
        sub_2.stream do |chunk|
          resp_2 += chunk
        end
        sub_2.callback do
          expect(resp_1).to eql("<script>p(1,'channels_path_inside_if_block','published message');</script>")
          expect(resp_2).to eql("<script>p(1,'test_channels_path_inside_if_block','published message');</script>")
          EventMachine.stop
        end

        publish_message_inline(channel, {}, body)
        publish_message_inline('test_' + channel, {}, body)
      end
    end
  end

  it "should accept return content gzipped" do
    channel = 'ch_test_get_content_gzipped'
    body = 'body'
    actual_response = ''

    nginx_run_server(config.merge({:gzip => "on", :subscriber_connection_ttl => '1s', :content_type => "text/html"})) do |conf|
      EventMachine.run do
        sent_headers = headers.merge({'accept-encoding' => 'gzip, compressed'})
        sub_1 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => sent_headers, :decoding => false
        sub_1.stream do |chunk|
          actual_response << chunk
        end
        sub_1.callback do
          expect(sub_1).to be_http_status(200)

          expect(sub_1.response_header["CONTENT_ENCODING"]).to eql("gzip")
          actual_response = Zlib::GzipReader.new(StringIO.new(actual_response)).read

          expect(actual_response).to eql("HEADER\r\nTEMPLATE\r\n1234\r\n<script>p(1,'ch_test_get_content_gzipped','body');</script></body></html>")
          EventMachine.stop
        end
        publish_message_inline(channel, {}, body)
      end
    end
  end

  xit "should accept a configuration with two shared memory zones without mix messages" do
    extra_config = {
      :subscriber_connection_ttl => '1s',
      :content_type => "text/html",
      :extra_configuration => %(
        http {
          push_stream_shared_memory_size         10m second;
          push_stream_subscriber_connection_ttl         1s;
          server {
            listen #{nginx_port.to_i + 1};
            location /pub {
              push_stream_publisher;
              push_stream_channels_path               $arg_id;
            }

            location ~ /sub/(.*) {
              push_stream_subscriber;
              push_stream_channels_path                   $1;
            }
          }
        }
      )
    }

    channel = 'ch_test_extra_http'
    body = 'body'
    actual_response_1 = ''
    actual_response_2 = ''

    nginx_run_server(config.merge(extra_config)) do |conf|
      EventMachine.run do
        sub_1 = EventMachine::HttpRequest.new("http://#{nginx_host}:#{nginx_port.to_i}/sub/" + channel.to_s).get
        sub_1.stream do |chunk|
          actual_response_1 += chunk
        end
        sub_2 = EventMachine::HttpRequest.new("http://#{nginx_host}:#{nginx_port.to_i + 1}/sub/" + channel.to_s).get
        sub_2.stream do |chunk|
          actual_response_2 += chunk
        end
        EM.add_timer(1.5) do
          expect(sub_1).to be_http_status(200)
          expect(sub_2).to be_http_status(200)

          expect(actual_response_1).to eql("HEADER\r\nTEMPLATE\r\n1234\r\n<script>p(1,'ch_test_extra_http','body_1');</script></body></html>")
          expect(actual_response_2).to eql("body_2")
          EventMachine.stop
        end

        EM.add_timer(0.5) do
          EventMachine::HttpRequest.new("http://#{nginx_host}:#{nginx_port.to_i}/pub/?id=" + channel.to_s).post :body => "#{body}_1"
          EventMachine::HttpRequest.new("http://#{nginx_host}:#{nginx_port.to_i + 1}/pub/?id=" + channel.to_s).post :body => "#{body}_2"
        end
      end
    end
  end
end
