require 'spec_helper'

describe "Setup Parameters" do
  it "should not accept '0' as ping message interval" do
    expect(nginx_test_configuration({:ping_message_interval => 0})).to include("push_stream_ping_message_interval cannot be zero")
  end

  it "should not accept a blank message template" do
    expect(nginx_test_configuration({:message_template => ""})).to include("push_stream_message_template cannot be blank")
  end

  it "should not accept '0' as subscriber connection ttl" do
    expect(nginx_test_configuration({:subscriber_connection_ttl => 0})).to include("push_stream_subscriber_connection_ttl cannot be zero")
  end

  it "should not accept '0' as long polling subscriber connection ttl" do
    expect(nginx_test_configuration({:longpolling_connection_ttl => 0})).to include("push_stream_longpolling_connection_ttl cannot be zero")
  end

  it "should not accept '0' as max channel id length" do
    expect(nginx_test_configuration({:max_channel_id_length => 0})).to include("push_stream_max_channel_id_length cannot be zero")
  end

  it "should not accept '0' as message ttl" do
    expect(nginx_test_configuration({:message_ttl => 0})).to include("push_stream_message_ttl cannot be zero")
  end

  it "should not accept '0' as max subscribers per channel" do
    expect(nginx_test_configuration({:max_subscribers_per_channel => 0})).to include("push_stream_max_subscribers_per_channel cannot be zero")
  end

  it "should not accept '0' as max messages stored per channel" do
    expect(nginx_test_configuration({:max_messages_stored_per_channel => 0})).to include("push_stream_max_messages_stored_per_channel cannot be zero")
  end

  it "should not accept '0' as max number of channels" do
    expect(nginx_test_configuration({:max_number_of_channels => 0})).to include("push_stream_max_number_of_channels cannot be zero")
  end

  it "should not accept '0' as max number of wildcard channels" do
    expect(nginx_test_configuration({:max_number_of_wildcard_channels => 0})).to include("push_stream_max_number_of_wildcard_channels cannot be zero")
  end

  it "should not accept '0' as max wildcard channels" do
    expect(nginx_test_configuration({:wildcard_channel_max_qtd => 0})).to include("push_stream_wildcard_channel_max_qtd cannot be zero")
  end

  it "should not set max wildcard channels without set boadcast channel prefix" do
    expect(nginx_test_configuration({:wildcard_channel_max_qtd => 1, :wildcard_channel_prefix => ""})).to include("cannot set wildcard channel max qtd if push_stream_wildcard_channel_prefix is not set or blank")
  end

  it "should not accept '0' as max number of wildcard channels" do
    config = {:max_number_of_wildcard_channels => 3, :wildcard_channel_max_qtd => 4, :wildcard_channel_prefix => "broad_"}
    expect(nginx_test_configuration(config)).to include("max number of wildcard channels cannot be smaller than value in push_stream_wildcard_channel_max_qtd")
  end

  it "should accept a configuration without http block" do
    config = {
      :configuration_template => %q{
        pid                     <%= pid_file %>;
        error_log               <%= error_log %> debug;
        # Development Mode
        master_process  off;
        daemon          off;
        worker_processes        <%= nginx_workers %>;

        events {
            worker_connections  1024;
            use                 <%= (RUBY_PLATFORM =~ /darwin/) ? 'kqueue' : 'epoll' %>;
        }
      }
    }
    expect(nginx_test_configuration(config)).to include("ngx_http_push_stream_module will not be used with this configuration.")
  end

  it "should not accept an invalid push mode" do
    expect(nginx_test_configuration({:subscriber_mode => "unknown"})).to include("invalid push_stream_subscriber mode value: unknown, accepted values (streaming, polling, long-polling, eventsource, websocket)")
  end

  it "should accept the known push modes" do
    expect(nginx_test_configuration({:subscriber_mode => ""})).not_to include("invalid push_stream_subscriber mode value")
    expect(nginx_test_configuration({:subscriber_mode => "streaming"})).not_to include("invalid push_stream_subscriber mode value")
    expect(nginx_test_configuration({:subscriber_mode => "polling"})).not_to include("invalid push_stream_subscriber mode value")
    expect(nginx_test_configuration({:subscriber_mode => "long-polling"})).not_to include("invalid push_stream_subscriber mode value")
    expect(nginx_test_configuration({:subscriber_mode => "eventsource"})).not_to include("invalid push_stream_subscriber mode value")
    expect(nginx_test_configuration({:subscriber_mode => "websocket"})).not_to include("invalid push_stream_subscriber mode value")
  end

  it "should not accept an invalid publisher mode" do
    expect(nginx_test_configuration({:publisher_mode => "unknown"})).to include("invalid push_stream_publisher mode value: unknown, accepted values (normal, admin)")
  end

  it "should accept the known publisher modes" do
    expect(nginx_test_configuration({:publisher_mode => ""})).not_to include("invalid push_stream_publisher mode value")
    expect(nginx_test_configuration({:publisher_mode => "normal"})).not_to include("invalid push_stream_publisher mode value")
    expect(nginx_test_configuration({:publisher_mode => "admin"})).not_to include("invalid push_stream_publisher mode value")
  end

  it "should not accept an invalid pattern for padding by user agent" do
    expect(nginx_test_configuration({:padding_by_user_agent => "user_agent,as,df"})).to include("padding pattern not match the value user_agent,as,df")
    expect(nginx_test_configuration({:padding_by_user_agent => "user_agent;10;0"})).to include("padding pattern not match the value user_agent;10;0")
    expect(nginx_test_configuration({:padding_by_user_agent => "user_agent,10,0:other_user_agent;20;0:another_user_agent,30,0"})).to include("error applying padding pattern to other_user_agent;20;0:another_user_agent,30,0")
  end

  it "should not accept an invalid shared memory size" do
    expect(nginx_test_configuration({:shared_memory_size => nil})).to include("push_stream_shared_memory_size must be set.")
  end

  it "should not accept a small shared memory size" do
    expect(nginx_test_configuration({:shared_memory_size => "100k"})).to include("The push_stream_shared_memory_size value must be at least")
  end

  it "should not accept an invalid channels path value" do
    expect(nginx_test_configuration({:channels_path => nil})).to include("push stream module: push_stream_channels_path must be set.")
    expect(nginx_test_configuration({:channels_path_for_pub => nil})).to include("push stream module: push_stream_channels_path must be set.")
  end
end
