# encoding: ascii
require 'spec_helper'

describe "Receive old messages" do
  let(:config) do
    {
      :header_template => nil,
      :footer_template => nil,
      :message_template => '{\"channel\":\"~channel~\", \"id\":\"~id~\", \"message\":\"~text~\"}\r\n',
      :subscriber_mode => subscriber_mode,
      :ping_message_interval => '1s'
    }
  end

  let(:eol) { "\r\n" }

  shared_examples_for "can receive old messages" do
    it "should receive old messages in a multi channel subscriber using backtrack" do
      channel_1 = 'ch_test_retreive_old_messages_in_multichannel_subscribe_1'
      channel_2 = 'ch_test_retreive_old_messages_in_multichannel_subscribe_2'
      channel_3 = 'ch_test_retreive_old_messages_in_multichannel_subscribe_3'

      body = 'body'

      nginx_run_server(config.merge(:header_template => 'HEADER\r\n')) do |conf|
        #create channels with some messages
        1.upto(3) do |i|
          publish_message(channel_1, headers, body + i.to_s)
          publish_message(channel_2, headers, body + i.to_s)
          publish_message(channel_3, headers, body + i.to_s)
        end

        get_content(nginx_address + '/sub/' + channel_1.to_s + '/' + channel_2.to_s + '.b5' + '/' + channel_3.to_s + '.b2', 6, headers) do |response, response_headers|
          if ["long-polling", "polling"].include?(conf.subscriber_mode)
            expect(response_headers['LAST_MODIFIED'].to_s).not_to eql("")
            expect(response_headers['ETAG'].to_s).not_to eql("")
          end

          lines = response.split(eol)
          expect(lines[0]).to eql('HEADER')
          line = JSON.parse(lines[1])
          expect(line['channel']).to eql(channel_2.to_s)
          expect(line['message']).to eql('body1')
          expect(line['id'].to_i).to eql(1)

          line = JSON.parse(lines[2])
          expect(line['channel']).to eql(channel_2.to_s)
          expect(line['message']).to eql('body2')
          expect(line['id'].to_i).to eql(2)

          line = JSON.parse(lines[3])
          expect(line['channel']).to eql(channel_2.to_s)
          expect(line['message']).to eql('body3')
          expect(line['id'].to_i).to eql(3)

          line = JSON.parse(lines[4])
          expect(line['channel']).to eql(channel_3.to_s)
          expect(line['message']).to eql('body2')
          expect(line['id'].to_i).to eql(2)

          line = JSON.parse(lines[5])
          expect(line['channel']).to eql(channel_3.to_s)
          expect(line['message']).to eql('body3')
          expect(line['id'].to_i).to eql(3)
        end
      end
    end

    it "should receive old messages in a multi channel subscriber using 'if_modified_since' header" do
      channel_1 = 'ch_test_retreive_old_messages_in_multichannel_subscribe_using_if_modified_since_header_1'
      channel_2 = 'ch_test_retreive_old_messages_in_multichannel_subscribe_using_if_modified_since_header_2'
      channel_3 = 'ch_test_retreive_old_messages_in_multichannel_subscribe_using_if_modified_since_header_3'

      body = 'body'

      nginx_run_server(config.merge(:header_template => 'HEADER\r\n'), :timeout => 45) do |conf|
        #create channels with some messages with progressive interval (1,2,3,5,7,9,12,15,18 seconds)
        1.upto(3) do |i|
          sleep(i)
          publish_message(channel_1, headers, body + i.to_s)
          sleep(i)
          publish_message(channel_2, headers, body + i.to_s)
          sleep(i)
          publish_message(channel_3, headers, body + i.to_s)
        end

        #get messages published less then 10 seconds ago
        t = Time.now - 10

        sent_headers = headers.merge({'If-Modified-Since' => t.utc.strftime("%a, %d %b %Y %T %Z")})

        get_content(nginx_address + '/sub/' + channel_1.to_s + '/' + channel_2.to_s + '/' + channel_3.to_s, 5, sent_headers) do |response, response_headers|
          if ["long-polling", "polling"].include?(conf.subscriber_mode)
            expect(response_headers['LAST_MODIFIED'].to_s).not_to eql("")
            expect(response_headers['ETAG'].to_s).not_to eql("")
          end

          lines = response.split(eol)
          expect(lines[0]).to eql('HEADER')

          line = JSON.parse(lines[1])
          expect(line['channel']).to eql(channel_1.to_s)
          expect(line['message']).to eql('body3')
          expect(line['id'].to_i).to eql(3)

          line = JSON.parse(lines[2])
          expect(line['channel']).to eql(channel_2.to_s)
          expect(line['message']).to eql('body3')
          expect(line['id'].to_i).to eql(3)

          line = JSON.parse(lines[3])
          expect(line['channel']).to eql(channel_3.to_s)
          expect(line['message']).to eql('body2')
          expect(line['id'].to_i).to eql(2)

          line = JSON.parse(lines[4])
          expect(line['channel']).to eql(channel_3.to_s)
          expect(line['message']).to eql('body3')
          expect(line['id'].to_i).to eql(3)
        end
      end
    end

    it "should receive old messages in a multi channel subscriber using 'if_modified_since' header and backtrack mixed" do
      channel_1 = 'ch_test_retreive_old_messages_in_multichannel_subscribe_using_if_modified_since_header_and_backtrack_mixed_1'
      channel_2 = 'ch_test_retreive_old_messages_in_multichannel_subscribe_using_if_modified_since_header_and_backtrack_mixed_2'
      channel_3 = 'ch_test_retreive_old_messages_in_multichannel_subscribe_using_if_modified_since_header_and_backtrack_mixed_3'

      body = 'body'

      nginx_run_server(config.merge(:header_template => 'HEADER\r\n'), :timeout => 45) do |conf|
        #create channels with some messages with progressive interval (1,2,3,5,7,9,12,15,18 seconds)
        1.upto(3) do |i|
          sleep(i)
          publish_message(channel_1, headers, body + i.to_s)
          sleep(i)
          publish_message(channel_2, headers, body + i.to_s)
          sleep(i)
          publish_message(channel_3, headers, body + i.to_s)
        end

        #get messages published less then 10 seconds ago
        t = Time.now - 10

        sent_headers = headers.merge({'If-Modified-Since' => t.utc.strftime("%a, %d %b %Y %T %Z")})

        get_content(nginx_address + '/sub/' + channel_1.to_s + '/' + channel_2.to_s + '.b5' + '/' + channel_3.to_s, 7, sent_headers) do |response, response_headers|
          if ["long-polling", "polling"].include?(conf.subscriber_mode)
            expect(response_headers['LAST_MODIFIED'].to_s).not_to eql("")
            expect(response_headers['ETAG'].to_s).not_to eql("")
          end

          lines = response.split(eol)
          expect(lines[0]).to eql('HEADER')

          line = JSON.parse(lines[1])
          expect(line['channel']).to eql(channel_1.to_s)
          expect(line['message']).to eql('body3')
          expect(line['id'].to_i).to eql(3)

          line = JSON.parse(lines[2])
          expect(line['channel']).to eql(channel_2.to_s)
          expect(line['message']).to eql('body1')
          expect(line['id'].to_i).to eql(1)

          line = JSON.parse(lines[3])
          expect(line['channel']).to eql(channel_2.to_s)
          expect(line['message']).to eql('body2')
          expect(line['id'].to_i).to eql(2)

          line = JSON.parse(lines[4])
          expect(line['channel']).to eql(channel_2.to_s)
          expect(line['message']).to eql('body3')
          expect(line['id'].to_i).to eql(3)

          line = JSON.parse(lines[5])
          expect(line['channel']).to eql(channel_3.to_s)
          expect(line['message']).to eql('body2')
          expect(line['id'].to_i).to eql(2)

          line = JSON.parse(lines[6])
          expect(line['channel']).to eql(channel_3.to_s)
          expect(line['message']).to eql('body3')
          expect(line['id'].to_i).to eql(3)
        end
      end
    end

    it "should receive old messages by 'last_event_id'" do
      channel = 'ch_test_disconnect_after_receive_old_messages_by_last_event_id_when_longpolling_is_on'

      nginx_run_server(config.merge(:message_template => '~text~\r\n')) do |conf|
        publish_message(channel, {'Event-Id' => 'event 1'}, 'msg 1')
        publish_message(channel, {'Event-Id' => 'event 2'}, 'msg 2')
        publish_message(channel, {}, 'msg 3')
        publish_message(channel, {'Event-Id' => 'event 3'}, 'msg 4')

        sent_headers = headers.merge({'Last-Event-Id' => 'event 2'})
        get_content(nginx_address + '/sub/' + channel.to_s, 2, sent_headers) do |response, response_headers|
          if ["long-polling", "polling"].include?(conf.subscriber_mode)
            expect(response_headers['LAST_MODIFIED'].to_s).not_to eql("")
            expect(response_headers['ETAG'].to_s).not_to eql("")
          end

          expect(response).to eql("msg 3\r\nmsg 4\r\n")
        end
      end
    end

    it "should receive old messages with equals 'if_modified_since' header untie them by the 'if_none_match' header" do
      channel = 'ch_test_receiving_messages_untie_by_etag'
      body_prefix = 'msg '
      messages_to_publish = 10
      now = nil

      nginx_run_server(config.merge(:message_template => '~text~\r\n')) do |conf|
        messages_to_publish.times do |i|
          now = Time.now if i == 5
          publish_message(channel.to_s, headers, body_prefix + i.to_s)
        end

        sent_headers = headers.merge({'If-Modified-Since' => now.utc.strftime("%a, %d %b %Y %T %Z"), 'If-None-Match' => '6'})
        get_content(nginx_address + '/sub/' + channel.to_s, 4, sent_headers) do |response, response_headers|
          if ["long-polling", "polling"].include?(conf.subscriber_mode)
            expect(response_headers['LAST_MODIFIED'].to_s).not_to eql("")
            expect(response_headers['ETAG'].to_s).to eql("W/10")
          end

          expect(response).to eql("msg 6\r\nmsg 7\r\nmsg 8\r\nmsg 9\r\n")
        end
      end
    end

    it "should receive message published on same second a subscriber connect" do
      channel = 'ch_test_receiving_messages_untie_by_etag'
      body = 'msg 1'

      nginx_run_server(config.merge(:message_template => '~text~')) do |conf|
        now = Time.now
        publish_message(channel.to_s, headers, body)

        sent_headers = headers.merge({'If-Modified-Since' => now.utc.strftime("%a, %d %b %Y %T %Z"), 'If-None-Match' => '0'})
        get_content(nginx_address + '/sub/' + channel.to_s, 1, sent_headers) do |response, response_headers|
          if ["long-polling", "polling"].include?(conf.subscriber_mode)
            expect(response_headers['LAST_MODIFIED'].to_s).not_to eql("")
            expect(response_headers['ETAG'].to_s).to eql("W/1")
          end

          expect(response).to eql("msg 1#{eol}")
        end
      end
    end

    it "should accept modified since and none match values not using headers" do
      channel = 'ch_test_send_modified_since_and_none_match_values_not_using_headers'
      body_prefix = 'msg '
      messages_to_publish = 10
      now = nil

      nginx_run_server(config.merge(:last_received_message_time => "$arg_time", :last_received_message_tag => "$arg_tag", :message_template => '~text~\r\n')) do |conf|
        messages_to_publish.times do |i|
          now = Time.now if i == 5
          publish_message(channel.to_s, headers, body_prefix + i.to_s)
        end

        params = "time=#{CGI.escape(now.utc.strftime("%a, %d %b %Y %T %Z")).gsub(/\+/, '%20')}&tag=6"
        get_content(nginx_address + '/sub/' + channel.to_s + '?' + params, 4, headers) do |response, response_headers|
          if ["long-polling", "polling"].include?(conf.subscriber_mode)
            expect(response_headers['LAST_MODIFIED'].to_s).not_to eql("")
            expect(response_headers['ETAG'].to_s).to eql("W/10")
          end

          expect(response).to eql("msg 6\r\nmsg 7\r\nmsg 8\r\nmsg 9\r\n")
        end
      end
    end

    it "should accept event id value not using headers" do
      channel = 'ch_test_send_event_id_value_not_using_headers'
      body_prefix = 'msg '
      messages_to_publish = 10
      now = nil

      nginx_run_server(config.merge(:last_event_id => "$arg_event_id", :message_template => '~text~\r\n')) do |conf|
        publish_message(channel, {'Event-Id' => 'event 1'}, 'msg 1')
        publish_message(channel, {'Event-Id' => 'event 2'}, 'msg 2')
        publish_message(channel, {}, 'msg 3')
        publish_message(channel, {'Event-Id' => 'event 3'}, 'msg 4')

        params = "event_id=#{CGI.escape("event 2").gsub(/\+/, '%20')}"
        get_content(nginx_address + '/sub/' + channel.to_s + '?' + params, 2, headers) do |response, response_headers|
          if ["long-polling", "polling"].include?(conf.subscriber_mode)
            expect(response_headers['LAST_MODIFIED'].to_s).not_to eql("")
            expect(response_headers['ETAG'].to_s).not_to eql("")
          end

          expect(response).to eql("msg 3\r\nmsg 4\r\n")
        end
      end
    end
  end

  def get_content(url, number_expected_lines, request_headers, &block)
    response = ''
    EventMachine.run do
      sub_1 = EventMachine::HttpRequest.new(url).get :head => request_headers
      sub_1.stream do |chunk|
        response += chunk
        lines = response.split(eol).map {|line| line.gsub(/^: /, "").gsub(/^data: /, "").gsub(/^id: .*/, "") }.delete_if{|line| line.empty?}.compact

        if lines.length >= number_expected_lines
          EventMachine.stop
          block.call("#{lines.join(eol)}#{eol}", sub_1.response_header) unless block.nil?
        end
      end
    end
  end

  context "in stream mode" do
    let(:subscriber_mode) { "streaming" }

    it_should_behave_like "can receive old messages"
  end

  context "in pooling mode" do
    let(:subscriber_mode) { "polling" }

    it_should_behave_like "can receive old messages"
  end

  context "in long-pooling mode" do
    let(:subscriber_mode) { "long-polling" }

    it_should_behave_like "can receive old messages"
  end

  context "in event source mode" do
    let(:subscriber_mode) { "eventsource" }
    let(:eol) { "\n" }

    it_should_behave_like "can receive old messages"
  end

  context "in websocket mode" do
    let(:subscriber_mode) { "websocket" }

    def get_content(url, number_expected_lines, request_headers, &block)
      uri = URI.parse url

      request_headers = request_headers.empty? ? "" : "#{request_headers.each_key.map{|k| "#{k}: #{request_headers[k]}"}.join("\r\n")}\r\n"
      request = "GET #{uri.request_uri} HTTP/1.0\r\nConnection: Upgrade\r\nSec-WebSocket-Key: /mQoZf6pRiv8+6o72GncLQ==\r\nUpgrade: websocket\r\nSec-WebSocket-Version: 8\r\n#{request_headers}"

      socket = open_socket(uri.host, uri.port)
      socket.print("#{request}\r\n")
      resp_headers, body = read_response_on_socket(socket, "\x89\x00")
      socket.close

      resp_headers = resp_headers.split("\r\n").inject({}) do |hash_headers, header|
        parts = header.split(":")
        hash_headers[parts[0]] = parts[1] if parts.count == 2
        hash_headers
      end

      lines = body.gsub(/[^\w{:,}" ]/, "\n").gsub("f{", "{").split("\n").delete_if{|line| line.empty?}.compact

      expect(lines.length).to be >= number_expected_lines

      if lines.length >= number_expected_lines
        block.call("#{lines.join("\r\n")}\r\n", resp_headers) unless block.nil?
      end
    end

    it_should_behave_like "can receive old messages"
  end
end
