require 'spec_helper'

describe "Publisher Channel id collision" do

  it "should create and retrieve channels with ids that collide" do
    channels = ["A", "plumless", "buckeroo", "B", "fc0591", "123rainerbommert", "C", "a1sellers", "advertees", "D"]

    nginx_run_server do |conf|
      channels.each do |channel|
        EventMachine.run do
          pub = EventMachine::HttpRequest.new(nginx_address + '/pub?id=' + channel).post :body => 'x'
          pub.callback do
            expect(pub).to be_http_status(200)
            EventMachine.stop
          end
        end
      end

      channels.each do |channel|
        EventMachine.run do
          pub = EventMachine::HttpRequest.new(nginx_address + '/channels-stats?id=' + channel).get :timeout => 30
          pub.callback do
            expect(pub).to be_http_status(200)
            EventMachine.stop
          end
        end
      end
    end
  end
end
