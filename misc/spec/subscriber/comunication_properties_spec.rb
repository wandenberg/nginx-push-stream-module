require 'spec_helper'

describe "Comunication Properties" do
  let(:config) do
    {
      :authorized_channels_only => "off",
      :header_template => "connected",
      :message_ttl => "12s",
      :message_template => "~text~",
      :ping_message_interval => "1s"
    }
  end

  it "should not block to connected to a nonexistent channel" do
    channel = 'ch_test_all_authorized'

    nginx_run_server(config) do |conf|
      EventMachine.run do
        sub = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => headers
        sub.stream do |chunk|
          expect(chunk).to eql(conf.header_template)
          EventMachine.stop
        end
      end
    end
  end

  it "should block to connected to a nonexistent channel when authorized only is 'on'" do
    channel = 'ch_test_only_authorized'
    body = 'message to create a channel'

    nginx_run_server(config.merge(:authorized_channels_only => "on")) do |conf|
      EventMachine.run do
        sub_1 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => headers
        sub_1.callback do |chunk|
          expect(sub_1).to be_http_status(403).without_body
          expect(sub_1.response_header['X_NGINX_PUSHSTREAM_EXPLAIN']).to eql("Subscriber could not create channels.")

          pub = EventMachine::HttpRequest.new(nginx_address + '/pub?id=' + channel.to_s ).post :head => headers, :body => body
          pub.callback do
            sub_2 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s).get :head => headers
            sub_2.stream do |chunk2|
              expect(chunk2).to eql(conf.header_template)
              EventMachine.stop
            end
          end
        end
      end
    end
  end

  it "should discard messages published a more time than the value configured to message ttl" do
    channel = 'ch_test_message_ttl'
    body = 'message to test buffer timeout '
    response_1 = response_2 = response_3 = ""
    sub_1 = sub_2 = sub_3 = nil

    nginx_run_server(config, :timeout => 20) do |conf|
      EventMachine.run do
        pub = EventMachine::HttpRequest.new(nginx_address + '/pub?id=' + channel.to_s ).post :head => headers, :body => body
        time_2 = EM.add_timer(2) do
          sub_1 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s + '.b1').get :head => headers
          sub_1.stream do |chunk|
            response_1 += chunk unless response_1.include?(body)
            sub_1.close if response_1.include?(body)
          end
        end

        EM.add_timer(6) do
          sub_2 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s + '.b1').get :head => headers
          sub_2.stream do |chunk|
            response_2 += chunk unless response_2.include?(body)
            sub_2.close if response_2.include?(body)
          end
        end

        #message will be certainly expired at 15 seconds
        EM.add_timer(16) do
          sub_3 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s + '.b1').get :head => headers
          sub_3.stream do |chunk|
            response_3 += chunk unless response_3.include?(body)
            sub_3.close if response_3.include?(body)
          end
        end

        EM.add_timer(17) do
          expect(response_1).to eql("#{conf.header_template}#{body}")
          expect(response_2).to eql("#{conf.header_template}#{body}")
          expect(response_3).to eql("#{conf.header_template}")
          EventMachine.stop
        end
      end
    end
  end

  it "should apply the message template to published message with the available keyworkds" do
    channel = 'ch_test_message_template'
    body = 'message to create a channel'

    response = ""
    nginx_run_server(config.merge(:message_template => '|{\"duplicated\":\"~channel~\", \"channel\":\"~channel~\", \"message\":\"~text~\", \"message_id\":\"~id~\"}')) do |conf|
      publish_message(channel, headers, body)

      EventMachine.run do
        sub = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s + '.b1').get :head => headers
        sub.stream do |chunk|
          response += chunk

          lines = response.split("|")

          if lines.length >= 3
            expect(lines[0]).to eql("#{conf.header_template}")
            expect(lines[1]).to eql("{\"duplicated\":\"#{channel}\", \"channel\":\"#{channel}\", \"message\":\"#{body}\", \"message_id\":\"1\"}")
            expect(lines[2]).to eql("{\"duplicated\":\"\", \"channel\":\"\", \"message\":\" \", \"message_id\":\"-1\"}")
            EventMachine.stop
          end
        end
      end
    end
  end

  it "should not be in loop when channel or published message contains one of the keywords" do
    channel = 'ch_test_message_and_channel_with_same_pattern_of_the_template~channel~~channel~~channel~~text~~text~~text~'
    body = '~channel~~channel~~channel~~text~~text~~text~'

    response = ""
    nginx_run_server(config.merge(:message_template => '|{\"channel\":\"~channel~\", \"message\":\"~text~\", \"message_id\":\"~id~\"}')) do |conf|
      publish_message(channel, headers, body)

      EventMachine.run do
        sub = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s + '.b1').get :head => headers
        sub.stream do |chunk|
          response += chunk

          lines = response.split("|")

          if lines.length >= 3
            expect(lines[0]).to eql("#{conf.header_template}")
            expect(lines[1]).to eql("{\"channel\":\"ch_test_message_and_channel_with_same_pattern_of_the_template~channel~~channel~~channel~~text~~text~~text~\", \"message\":\"~channel~~channel~~channel~~text~~text~~text~\", \"message_id\":\"1\"}")
            expect(lines[2]).to eql("{\"channel\":\"\", \"message\":\" \", \"message_id\":\"-1\"}")
            EventMachine.stop
          end
        end
      end
    end
  end
end
