require 'spec_helper'

describe "Events channel" do
  let(:config) do
   {
     events_channel_id: "events",
     allow_connections_to_events_channel: "on",
     message_template: "text: ~text~\\nchannel: ~channel~",
     header_template: nil,
     footer_template: nil,
     publisher_mode: "admin",
     ping_message_interval: nil,
     store_messages: "off"
   }
  end

  it "should send an event when a channel is created" do
    channel = 'ch_test_send_event_channel_created'
    body = 'any content'

    nginx_run_server(config) do |conf|
      EventMachine.run do
        sub_1 = EventMachine::HttpRequest.new("#{nginx_address}/sub/#{conf.events_channel_id}").get head: headers
        sub_1.stream do |chunk|
          expect(chunk).to eql(%(text: {"type": "channel_created", "channel": "#{channel}"}\nchannel: #{conf.events_channel_id}))
          EventMachine.stop
        end

        EM.add_timer(0.5) do
          pub_1 = EventMachine::HttpRequest.new("#{nginx_address}/pub?id=#{channel}").post head: headers, body: body
          pub_1.callback do
            expect(pub_1).to be_http_status(200).with_body
          end
        end
      end
    end
  end

  it "should send an event when a channel is deleted" do
    channel = 'ch_test_send_event_channel_deleted'
    body = 'any content'

    nginx_run_server(config) do |conf|
      EventMachine.run do
        pub_1 = EventMachine::HttpRequest.new("#{nginx_address}/pub?id=#{channel}").post head: headers, body: body
        pub_1.callback do
          expect(pub_1).to be_http_status(200).with_body

          sub_1 = EventMachine::HttpRequest.new("#{nginx_address}/sub/#{conf.events_channel_id}").get head: headers
          sub_1.stream do |chunk|
            expect(chunk).to eql(%(text: {"type": "channel_destroyed", "channel": "#{channel}"}\nchannel: #{conf.events_channel_id}))
            EventMachine.stop
          end

          EM.add_timer(0.5) do
            pub_2 = EventMachine::HttpRequest.new("#{nginx_address}/pub?id=#{channel}").delete head: headers
            pub_2.callback do
              expect(pub_2).to be_http_status(200).without_body
            end
          end
        end
      end
    end
  end

  it "should send an event when a channel is collected by inactivity" do
    channel = 'ch_test_send_event_channel_collected'
    body = 'any content'

    nginx_run_server(config, timeout: 40) do |conf|
      EventMachine.run do
        pub_1 = EventMachine::HttpRequest.new("#{nginx_address}/pub?id=#{channel}").post head: headers, body: body
        pub_1.callback do
          expect(pub_1).to be_http_status(200).with_body

          sub_1 = EventMachine::HttpRequest.new("#{nginx_address}/sub/#{conf.events_channel_id}", inactivity_timeout: 40).get head: headers
          sub_1.stream do |chunk|
            expect(chunk).to eql(%(text: {"type": "channel_destroyed", "channel": "#{channel}"}\nchannel: #{conf.events_channel_id}))
            EventMachine.stop
          end
        end
      end
    end
  end

  it "should send an event when a client subscribe to a channel" do
    channel = 'ch_test_send_event_client_subscribed'
    body = 'any content'

    nginx_run_server(config) do |conf|
      EventMachine.run do
        pub_1 = EventMachine::HttpRequest.new("#{nginx_address}/pub?id=#{channel}").post head: headers, body: body
        pub_1.callback do
          expect(pub_1).to be_http_status(200).with_body

          sub_1 = EventMachine::HttpRequest.new("#{nginx_address}/sub/#{conf.events_channel_id}").get head: headers
          sub_1.stream do |chunk|
            expect(chunk).to eql(%(text: {"type": "client_subscribed", "channel": "#{channel}"}\nchannel: #{conf.events_channel_id}))
            EventMachine.stop
          end

          EM.add_timer(0.5) do
            sub_2 = EventMachine::HttpRequest.new("#{nginx_address}/sub/#{channel}").get head: headers
          end
        end
      end
    end
  end

  it "should send an event when a websocket client subscribe to a channel" do
    channel = 'ch_test_send_event_websocket_client_subscribed'
    body = 'any content'

    nginx_run_server(config.merge(subscriber_mode: "websocket")) do |conf|
      EventMachine.run do
        pub_1 = EventMachine::HttpRequest.new("#{nginx_address}/pub?id=#{channel}").post head: headers, body: body
        pub_1.callback do
          expect(pub_1).to be_http_status(200).with_body

          sub_1 = WebSocket::EventMachine::Client.connect(uri: "ws://#{nginx_host}:#{nginx_port}/sub/#{conf.events_channel_id}")
          sub_1.onmessage do |text, type|
            expect(text).to eql(%(text: {"type": "client_subscribed", "channel": "#{channel}"}\nchannel: #{conf.events_channel_id}))
            EventMachine.stop
          end

          EM.add_timer(0.5) do
            ws = WebSocket::EventMachine::Client.connect(uri: "ws://#{nginx_host}:#{nginx_port}/sub/#{channel}")
          end
        end
      end
    end
  end

  it "should send an event when a long-polling client subscribe to a channel" do
    channel = 'ch_test_send_event_client_subscribed'
    body = 'any content'

    nginx_run_server(config.merge(subscriber_mode: "long-polling")) do |conf|
      EventMachine.run do
        pub_1 = EventMachine::HttpRequest.new("#{nginx_address}/pub?id=#{channel}").post head: headers, body: body
        pub_1.callback do
          expect(pub_1).to be_http_status(200).with_body

          response = ''
          sub_1 = EventMachine::HttpRequest.new("#{nginx_address}/sub/#{conf.events_channel_id}").get head: headers
          sub_1.stream { |chunk| response += chunk }
          sub_1.callback do
            expect(response).to eql(%(text: {"type": "client_subscribed", "channel": "#{channel}"}\nchannel: #{conf.events_channel_id}))
            EventMachine.stop
          end

          EM.add_timer(0.5) do
            sub_2 = EventMachine::HttpRequest.new("#{nginx_address}/sub/#{channel}").get head: headers
          end
        end
      end
    end
  end

  it "should send an event when a client unsubscribe to a channel by timeout" do
    channel = 'ch_test_send_event_client_unsubscribed'
    body = 'any content'

    nginx_run_server(config.merge(subscriber_connection_ttl: "5s"), timeout: 15) do |conf|
      EventMachine.run do
        sub_1 = EventMachine::HttpRequest.new("#{nginx_address}/sub/#{channel}", inactivity_timeout: 10).get head: headers

        EM.add_timer(2) do
          sub_2 = EventMachine::HttpRequest.new("#{nginx_address}/sub/#{conf.events_channel_id}", inactivity_timeout: 10).get head: headers
          sub_2.stream do |chunk|
            expect(chunk).to eql(%(text: {"type": "client_unsubscribed", "channel": "#{channel}"}\nchannel: #{conf.events_channel_id}))
            EventMachine.stop
          end
        end
      end
    end
  end

  it "should send an event when a client unsubscribe to a channel by delete" do
    channel = 'ch_test_send_event_client_unsubscribed'
    body = 'any content'

    nginx_run_server(config.merge(subscriber_connection_ttl: "50s"), timeout: 15) do |conf|
      EventMachine.run do
        sub_1 = EventMachine::HttpRequest.new("#{nginx_address}/sub/#{channel}").get head: headers

        EM.add_timer(0.5) do
          sub_2 = EventMachine::HttpRequest.new("#{nginx_address}/sub/#{conf.events_channel_id}").get head: headers
          sub_2.stream do |chunk|
            expect(chunk).to eql(%(text: {"type": "client_unsubscribed", "channel": "#{channel}"}\nchannel: #{conf.events_channel_id}))
            EventMachine.stop
          end
        end

        EM.add_timer(1) do
          pub_1 = EventMachine::HttpRequest.new("#{nginx_address}/pub?id=#{channel}").delete head: headers
          pub_1.callback do
            expect(pub_1).to be_http_status(200).without_body
          end
        end
      end
    end
  end

  it "should never collect the events channel by inactivity" do
    channel = 'ch_test_not_collect_events_channel'
    body = 'any content'

    nginx_run_server(config.merge(store_messages: 'on', message_ttl: '5s'), timeout: 120) do |conf|
      EventMachine.run do
        pub_1 = EventMachine::HttpRequest.new("#{nginx_address}/pub?id=#{channel}").post head: headers, body: body
        pub_1.callback do
          expect(pub_1).to be_http_status(200).with_body

          pub_2 = EventMachine::HttpRequest.new("#{nginx_address}/pub?id=#{channel}").get head: headers
          pub_2.callback do
            expect(pub_2).to be_http_status(200).with_body
            response = JSON.parse(pub_2.response)
            expect(response["channel"].to_s).to eql(channel)
            expect(response["published_messages"].to_i).to eql(1)
            expect(response["stored_messages"].to_i).to eql(1)
            expect(response["subscribers"].to_i).to eql(0)

            pub_3 = EventMachine::HttpRequest.new("#{nginx_address}/pub?id=#{conf.events_channel_id}").get head: headers
            pub_3.callback do
              expect(pub_3).to be_http_status(200).with_body
              response = JSON.parse(pub_3.response)
              expect(response["channel"].to_s).to eql(conf.events_channel_id)
              expect(response["published_messages"].to_i).to eql(1)
              expect(response["stored_messages"].to_i).to eql(1)
              expect(response["subscribers"].to_i).to eql(0)
            end
          end
        end

        EM.add_timer(35) do
          pub_4 = EventMachine::HttpRequest.new("#{nginx_address}/pub?id=#{channel}").get head: headers
          pub_4.callback do
            expect(pub_4).to be_http_status(404).without_body

            pub_5 = EventMachine::HttpRequest.new("#{nginx_address}/pub?id=#{conf.events_channel_id}").get head: headers
            pub_5.callback do
              expect(pub_5).to be_http_status(200).with_body
              response = JSON.parse(pub_5.response)
              expect(response["channel"].to_s).to eql(conf.events_channel_id)
              expect(response["published_messages"].to_i).to eql(2)
              expect(response["stored_messages"].to_i).to eql(1)
              expect(response["subscribers"].to_i).to eql(0)
            end
          end

          EM.add_timer(35) do
            pub_6 = EventMachine::HttpRequest.new("#{nginx_address}/pub?id=#{conf.events_channel_id}").get head: headers
            pub_6.callback do
              expect(pub_6).to be_http_status(200).with_body
              response = JSON.parse(pub_6.response)
              expect(response["channel"].to_s).to eql(conf.events_channel_id)
              expect(response["published_messages"].to_i).to eql(2)
              expect(response["stored_messages"].to_i).to eql(0)
              expect(response["subscribers"].to_i).to eql(0)
              EventMachine.stop
            end
          end
        end
      end
    end
  end

  it "should use a exclusive mutex lock for events channel" do
    channel = 'ch_test_exclusive_lock_events_channel'

    nginx_run_server(config.merge(header_template: 'H', subscriber_connection_ttl: '25s'), timeout: 50) do |conf|
      EventMachine.run do
        subscriber_in_loop_with_limit(channel, headers, 1, 20) do
          EM.add_timer(5) do
            pub_2 = EventMachine::HttpRequest.new("#{nginx_address}/pub?id=#{conf.events_channel_id}").get head: headers
            pub_2.callback do
              expect(pub_2).to be_http_status(200).with_body
              response = JSON.parse(pub_2.response)
              expect(response["channel"].to_s).to eql(conf.events_channel_id)
              expect(response["published_messages"].to_i).to eql(40)
              expect(response["stored_messages"].to_i).to eql(20)
              expect(response["subscribers"].to_i).to eql(0)
            end

            EM.add_timer(10) do
              10.times do |i|
                pub_1 = EventMachine::HttpRequest.new("#{nginx_address}/pub?id=#{channel}_#{i + 1}").delete head: headers
                pub_1.callback do
                  expect(pub_1).to be_http_status(200).without_body
                end
              end

              EM.add_timer(5) do
                pub_2 = EventMachine::HttpRequest.new("#{nginx_address}/pub?id=#{conf.events_channel_id}").get head: headers
                pub_2.callback do
                  expect(pub_2).to be_http_status(200).with_body
                  response = JSON.parse(pub_2.response)
                  expect(response["channel"].to_s).to eql(conf.events_channel_id)
                  expect(response["published_messages"].to_i).to eql(60)
                  expect(response["stored_messages"].to_i).to eql(20)
                  expect(response["subscribers"].to_i).to eql(0)
                end

                EM.add_timer(25) do
                  pub_3 = EventMachine::HttpRequest.new("#{nginx_address}/channels-stats?id=ALL").get head: headers
                  pub_3.callback do
                    expect(pub_3).to be_http_status(200)
                    response = JSON.parse(pub_3.response)
                    expect(response["infos"].length).to eql(1)
                    expect(response["infos"][0]["channel"].to_s).to eql(conf.events_channel_id)
                    expect(response["infos"][0]["published_messages"].to_i).to eql(80)
                    expect(response["infos"][0]["stored_messages"].to_i).to eql(20)
                    expect(response["infos"][0]["subscribers"].to_i).to eql(0)
                    EventMachine.stop
                  end
                end
              end
            end
          end
        end
      end
    end
  end

  it "should not accept publish outside messages to events channel" do
    body = 'any content'
    extra_config = {
      subscriber_mode: "websocket",
      extra_location: %q(
        location ~ /ws/(.*)? {
            # activate websocket mode for this location
            push_stream_subscriber websocket;

            # positional channel path
            push_stream_channels_path               $1;

            # allow subscriber to publish
            push_stream_websocket_allow_publish on;
            # store messages
            push_stream_store_messages on;
        }
      )
    }

    nginx_run_server(config.merge(extra_config)) do |conf|
      EventMachine.run do
        pub_1 = EventMachine::HttpRequest.new("#{nginx_address}/pub?id=#{conf.events_channel_id}").post head: headers, body: body
        pub_1.callback do
          expect(pub_1).to be_http_status(403).without_body

          received_message = false
          sub_1 = WebSocket::EventMachine::Client.connect(uri: "ws://#{nginx_host}:#{nginx_port}/ws/#{conf.events_channel_id}/other_valid_channel")
          sub_1.onmessage do |text, type|
            next if received_message
            received_message = true
            expect(text).to eql(%(text: {"type": "client_subscribed", "channel": "other_valid_channel"}\nchannel: #{conf.events_channel_id}))
            sub_1.send body

            pub_2 = EventMachine::HttpRequest.new("#{nginx_address}/pub?id=#{conf.events_channel_id}").get head: headers
            pub_2.callback do
              expect(pub_2).to be_http_status(200).with_body
              response = JSON.parse(pub_2.response)
              expect(response["channel"].to_s).to eql(conf.events_channel_id)
              expect(response["published_messages"].to_i).to eql(2)
              expect(response["stored_messages"].to_i).to eql(2)
              expect(response["subscribers"].to_i).to eql(1)

              pub_3 = EventMachine::HttpRequest.new("#{nginx_address}/pub?id=other_valid_channel").get head: headers
              pub_3.callback do
                expect(pub_3).to be_http_status(200).with_body
                response = JSON.parse(pub_3.response)
                expect(response["channel"].to_s).to eql("other_valid_channel")
                expect(response["published_messages"].to_i).to eql(1)
                expect(response["stored_messages"].to_i).to eql(1)
                expect(response["subscribers"].to_i).to eql(1)
                EventMachine.stop
              end
            end
          end
        end
      end
    end
  end

  it "should not accept delete events channel" do
    nginx_run_server(config) do |conf|
      EventMachine.run do
        pub_1 = EventMachine::HttpRequest.new("#{nginx_address}/pub?id=#{conf.events_channel_id}").delete head: headers
        pub_1.callback do
          expect(pub_1).to be_http_status(403).without_body

          pub_2 = EventMachine::HttpRequest.new("#{nginx_address}/pub?id=#{conf.events_channel_id}").get head: headers
          pub_2.callback do
            expect(pub_2).to be_http_status(200).with_body
            response = JSON.parse(pub_2.response)
            expect(response["channel"].to_s).to eql(conf.events_channel_id)
            expect(response["published_messages"].to_i).to eql(0)
            expect(response["stored_messages"].to_i).to eql(0)
            expect(response["subscribers"].to_i).to eql(0)
            EventMachine.stop
          end
        end
      end
    end
  end

  it "should not accept subscribe to events channel when access is not authorized" do
    extra_config = {
      allow_connections_to_events_channel: "off",
      extra_location: %(
        location ~ /sub_to_events_channel_only_here/(.*) {
          push_stream_subscriber;
          push_stream_channels_path $1;
          push_stream_allow_connections_to_events_channel "on";
        }

        location ~ /ws/(.*) {
          push_stream_subscriber websocket;
          push_stream_channels_path $1;
        }

        location ~ /ws_to_events_channel_only_here/(.*) {
          push_stream_subscriber websocket;
          push_stream_channels_path $1;
          push_stream_allow_connections_to_events_channel "on";
        }
      )
    }

    nginx_run_server(config.merge(extra_config)) do |conf|
      EventMachine.run do
        sub_1 = EventMachine::HttpRequest.new("#{nginx_address}/sub/#{conf.events_channel_id}/other_valid_channel").get head: headers
        sub_1.callback do
          expect(sub_1).to be_http_status(403).without_body

          sub_2 = WebSocket::EventMachine::Client.connect(uri: "ws://#{nginx_host}:#{nginx_port}/ws/#{conf.events_channel_id}/other_valid_channel")
          sub_2.onclose do |code, reason|
            received_message = false
            sub_3 = EventMachine::HttpRequest.new("#{nginx_address}/sub_to_events_channel_only_here/#{conf.events_channel_id}/other_valid_channel").get head: headers
            sub_3.stream do |chunck|
              next if received_message
              received_message = true

              expect(chunck).to eql(%(text: {"type": "client_subscribed", "channel": "other_valid_channel"}\nchannel: #{conf.events_channel_id}))

              sub_4 = WebSocket::EventMachine::Client.connect(uri: "ws://#{nginx_host}:#{nginx_port}/ws_to_events_channel_only_here/#{conf.events_channel_id}/other_valid_channel")
              sub_4.onmessage do |text, type|
                expect(text).to eql(%(text: {"type": "client_subscribed", "channel": "other_valid_channel"}\nchannel: #{conf.events_channel_id}))
                EventMachine.stop
              end
            end
          end
        end
      end
    end
  end

  it "should change the tag number for messages on the same second" do
    channel = 'ch_test_send_events_same_second'
    body = 'any content'

    messages = []
    nginx_run_server(config.merge(message_template: "{\\\"text\\\": ~text~, \\\"channel\\\": \\\"~channel~\\\", \\\"tag\\\": ~tag~, \\\"time\\\": \\\"~time~\\\", \\\"id\\\": \\\"~id~\\\"}")) do |conf|
      EventMachine.run do
        pub_1 = EventMachine::HttpRequest.new("#{nginx_address}/pub?id=#{channel}").post head: headers, body: body
        pub_1.callback do
          EM.add_timer(1) do
            sub = EventMachine::HttpRequest.new("#{nginx_address}/sub/#{conf.events_channel_id}").get head: headers
            sub.stream do |chunk|
              messages << chunk

              if messages.size == 3
                m0 = JSON.parse(messages[0])
                m1 = JSON.parse(messages[1])
                m2 = JSON.parse(messages[2])

                expect(m1["tag"]).to eq(m0["tag"] + 1)
                expect(m1["time"]).to eq(m0["time"])

                expect(m0["tag"]).to eq(m2["tag"])
                expect(m0["time"]).not_to eq(m2["time"])

                EventMachine.stop
              end
            end

            EM.add_timer(0.5) do
              sub_1 = EventMachine::HttpRequest.new("#{nginx_address}/sub/#{channel}").get head: headers
              sub_2 = EventMachine::HttpRequest.new("#{nginx_address}/sub/#{channel}").get head: headers
              EM.add_timer(1) do
                sub_3 = EventMachine::HttpRequest.new("#{nginx_address}/sub/#{channel}").get head: headers
              end
            end
          end
        end
      end
    end
  end

  def subscriber_in_loop_with_limit(channel, headers, index, limit, &block)
    called = false
    sub = EventMachine::HttpRequest.new("#{nginx_address}/sub/#{channel}_#{index}", inactivity_timeout: 60).get head: headers
    sub.stream do |chunk|
      if index == limit
        block.call
      else
        unless called
          called = true
          subscriber_in_loop_with_limit(channel, headers, index + 1, limit, &block)
         end
      end
    end
  end
end
