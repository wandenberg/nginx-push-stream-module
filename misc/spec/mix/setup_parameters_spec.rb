require 'spec_helper'

describe "Setup Parameters" do
  it "should not accept '0' as ping message interval" do
    nginx_test_configuration({:ping_message_interval => 0}).should include("push_stream_ping_message_interval cannot be zero")
  end

  it "should not accept a blank message template" do
    nginx_test_configuration({:message_template => ""}).should include("push_stream_message_template cannot be blank")
  end

  it "should not accept '0' as subscriber connection ttl" do
    nginx_test_configuration({:subscriber_connection_ttl => 0}).should include("push_stream_subscriber_connection_ttl cannot be zero")
  end

  it "should not accept '0' as long polling subscriber connection ttl" do
    nginx_test_configuration({:longpolling_connection_ttl => 0}).should include("push_stream_longpolling_connection_ttl cannot be zero")
  end

  it "should not accept '0' as max channel id length" do
    nginx_test_configuration({:max_channel_id_length => 0}).should include("push_stream_max_channel_id_length cannot be zero")
  end

  it "should not accept '0' as message ttl" do
    nginx_test_configuration({:message_ttl => 0}).should include("push_stream_message_ttl cannot be zero")
  end

  it "should not accept '0' as max subscribers per channel" do
    nginx_test_configuration({:max_subscribers_per_channel => 0}).should include("push_stream_max_subscribers_per_channel cannot be zero")
  end

  it "should not accept '0' as max messages stored per channel" do
    nginx_test_configuration({:max_messages_stored_per_channel => 0}).should include("push_stream_max_messages_stored_per_channel cannot be zero")
  end

  it "should not accept '0' as max number of channels" do
    nginx_test_configuration({:max_number_of_channels => 0}).should include("push_stream_max_number_of_channels cannot be zero")
  end

  it "should not accept '0' as max number of broadcast channels" do
    nginx_test_configuration({:max_number_of_broadcast_channels => 0}).should include("push_stream_max_number_of_broadcast_channels cannot be zero")
  end

  it "should not accept '0' as max broadcast channels" do
    nginx_test_configuration({:broadcast_channel_max_qtd => 0}).should include("push_stream_broadcast_channel_max_qtd cannot be zero")
  end

  it "should not set max broadcast channels without set boadcast channel prefix" do
    nginx_test_configuration({:broadcast_channel_max_qtd => 1, :broadcast_channel_prefix => ""}).should include("cannot set broadcast channel max qtd if push_stream_broadcast_channel_prefix is not set or blank")
  end

  it "should not accept '0' as max number of broadcast channels" do
    config = {:max_number_of_broadcast_channels => 3, :broadcast_channel_max_qtd => 4, :broadcast_channel_prefix => "broad_"}
    nginx_test_configuration(config).should include("max number of broadcast channels cannot be smaller than value in push_stream_broadcast_channel_max_qtd")
  end

  it "should not accept a value less than '10s' as shared memory cleanup objects ttl" do
    nginx_test_configuration({:shared_memory_cleanup_objects_ttl => '5s'}).should include("memory cleanup objects ttl cannot't be less than 10.")
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
    nginx_test_configuration(config).should include("ngx_http_push_stream_module will not be used with this configuration.")
  end

  it "should not accept an invalid push mode" do
    nginx_test_configuration({:subscriber_mode => "unknown"}).should include("invalid push_stream_subscriber mode value: unknown, accepted values (streaming, polling, long-polling, eventsource)")
  end

  it "should accept the known push modes" do
    nginx_test_configuration({:subscriber_mode => ""}).should_not include("invalid push_stream_subscriber mode value")
    nginx_test_configuration({:subscriber_mode => "streaming"}).should_not include("invalid push_stream_subscriber mode value")
    nginx_test_configuration({:subscriber_mode => "polling"}).should_not include("invalid push_stream_subscriber mode value")
    nginx_test_configuration({:subscriber_mode => "long-polling"}).should_not include("invalid push_stream_subscriber mode value")
    nginx_test_configuration({:subscriber_mode => "eventsource"}).should_not include("invalid push_stream_subscriber mode value")
  end

  it "should not accept an invalid publisher mode" do
    nginx_test_configuration({:publisher_mode => "unknown"}).should include("invalid push_stream_publisher mode value: unknown, accepted values (normal, admin)")
  end

  it "should accept the known publisher modes" do
    nginx_test_configuration({:publisher_mode => ""}).should_not include("invalid push_stream_publisher mode value")
    nginx_test_configuration({:publisher_mode => "normal"}).should_not include("invalid push_stream_publisher mode value")
    nginx_test_configuration({:publisher_mode => "admin"}).should_not include("invalid push_stream_publisher mode value")
  end

  it "should not accept an invalid pattern for padding by user agent" do
    nginx_test_configuration({:padding_by_user_agent => "user_agent,as,df"}).should include("padding pattern not match the value user_agent,as,df")
    nginx_test_configuration({:padding_by_user_agent => "user_agent;10;0"}).should include("padding pattern not match the value user_agent;10;0")
    nginx_test_configuration({:padding_by_user_agent => "user_agent,10,0:other_user_agent;20;0:another_user_agent,30,0"}).should include("error applying padding pattern to other_user_agent;20;0:another_user_agent,30,0")
  end
end
