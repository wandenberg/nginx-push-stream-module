require 'rubygems'

# Set up gems listed in the Gemfile.
ENV['BUNDLE_GEMFILE'] ||= File.expand_path('../../Gemfile', __FILE__)

require 'bundler/setup' if File.exists?(ENV['BUNDLE_GEMFILE'])
Bundler.require(:default, :test) if defined?(Bundler)

require 'nginx_configuration'
require 'custom_http_matchers'

RSpec.configure do |config|
  config.before(:suite) do
    FileUtils.rm_rf Dir[File.join(NginxTestHelper.nginx_tests_tmp_dir, "cores", "**")]
  end
  config.before(:each) do
    core_dir = File.join(File.join(NginxTestHelper.nginx_tests_tmp_dir, "cores", config_id))
    FileUtils.mkdir_p core_dir
    Dir.chdir core_dir
  end
  config.after(:each) do
    NginxTestHelper::Config.delete_config_and_log_files(config_id) if has_passed?
  end
  config.after(:suite) do
    cores = Dir[File.join(NginxTestHelper.nginx_tests_tmp_dir, "cores", "**", "core")]
    raise StandardError.new "\n\nCore dump(s) at:\n#{cores.join("\n")}\n\n" unless cores.empty?
  end
  config.order = "random"
  config.include(CustomHttpMatchers)
end

def publish_message_inline(channel, headers, body, &block)
  pub = EventMachine::HttpRequest.new(nginx_address + '/pub?id=' + channel.to_s).post :head => headers, :body => body
  pub.callback do
    pub.should be_http_status(200)
    block.call unless block.nil?
  end
  pub
end

def publish_message(channel, headers, body)
  EventMachine.run do
    pub = publish_message_inline(channel, headers, body) do
      response = JSON.parse(pub.response)
      response["channel"].to_s.should eql(channel)
      EventMachine.stop
    end
  end
end

def create_channel_by_subscribe(channel, headers, timeout=60, &block)
  EventMachine.run do
    sub_1 = EventMachine::HttpRequest.new(nginx_address + '/sub/' + channel.to_s, :connect_timeout => timeout, :inactivity_timeout => timeout).get :head => headers.merge({"accept-encoding" => ""})
    sub_1.stream do |chunk|
      block.call
    end

    sub_1.callback do
      EventMachine.stop
    end
  end
end

def publish_messages_until_fill_the_memory(channel, body, &block)
  i = 0
  resp_headers, resp_body = nil
  socket = open_socket(nginx_host, nginx_port)
  while (true) do
    socket.print("POST /pub?id=#{channel.to_s % (i)} HTTP/1.1\r\nHost: localhost\r\nContent-Length: #{body.size}\r\n\r\n#{body}")
    resp_headers, resp_body = read_response_on_socket(socket, {:wait_for => "}\r\n"})
    break unless resp_headers.match(/200 OK/)
    i += 1
  end
  socket.close

  status = resp_headers.match(/HTTP[^ ]* ([^ ]*)/)[1]
  block.call(status, resp_body) unless block.nil?
end
