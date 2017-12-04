describe("PushStream", function() {
  var originalTimeout;
  beforeEach(function() {
    originalTimeout = jasmine.DEFAULT_TIMEOUT_INTERVAL;
    jasmine.DEFAULT_TIMEOUT_INTERVAL = 10000;
  });

  afterEach(function() {
    jasmine.DEFAULT_TIMEOUT_INTERVAL = originalTimeout;
  });

  describe("when defining library external interface", function() {
    it("should has a class named PushStream", function() {
      expect(new PushStream()).toBeDefined();
    });

    it("should has a log level constant", function() {
      expect(PushStream.LOG_LEVEL).toBeDefined();
    });

    it("should has a log output element id constant", function() {
      expect(PushStream.LOG_OUTPUT_ELEMENT_ID).toBeDefined();
    });

    it("should define status code constants", function() {
      expect(PushStream.CLOSED).toBeDefined();
      expect(PushStream.CONNECTING).toBeDefined();
      expect(PushStream.OPEN).toBeDefined();
    });

    it("should has a PushStream instances manager", function() {
      expect(PushStreamManager).toBeDefined();
      expect(PushStreamManager instanceof Array).toBeTruthy();
    });

  });

  describe("when using default values", function() {
    var pushstream = null;
    beforeEach(function() {
      pushstream = new PushStream();
    });

    it("should use current hostname", function() {
      expect(pushstream.host).toBe(window.location.hostname);
    });

    it("should use port 80", function() {
      expect(pushstream.port).toBe(80);
    });

    it("should not use ssl", function() {
      expect(pushstream.useSSL).toBeFalsy();
    });

    it("should not use JSONP", function() {
      expect(pushstream.useJSONP).toBeFalsy();
    });

    it("should set state as uninitialised", function() {
      expect(pushstream.readyState).toBe(PushStream.CLOSED);
    });

    it("should set messagesPublishedAfter as undefined", function() {
      expect(pushstream.messagesPublishedAfter).toBe(undefined);
    });

    describe("for operation timeouts", function() {
      it("should has a connection timeout", function() {
        expect(pushstream.timeout).toBe(30000);
      });

      it("should has a ping message timeout", function() {
        expect(pushstream.pingtimeout).toBe(30000);
      });

      it("should has a reconnect interval, to be used when a timeout happens", function() {
        expect(pushstream.reconnectOnTimeoutInterval).toBe(3000);
      });

      it("should has a reconnect interval, to be used when a channel is unavailable", function() {
        expect(pushstream.reconnectOnChannelUnavailableInterval).toBe(60000);
      });
    });

    describe("for url prefix", function() {
      it("should use '/pub' for publish message", function() {
        expect(pushstream.urlPrefixPublisher).toBe('/pub');
      });

      it("should use '/sub' for stream", function() {
        expect(pushstream.urlPrefixStream).toBe('/sub');
      });

      it("should use '/ev' for event source", function() {
        expect(pushstream.urlPrefixEventsource).toBe('/ev');
      });

      it("should use '/lp' for long-polling", function() {
        expect(pushstream.urlPrefixLongpolling).toBe('/lp');
      });

      it("should use '/ws' for websocket", function() {
        expect(pushstream.urlPrefixWebsocket).toBe('/ws');
      });
    });

    describe("for json keys", function() {
      it("should has a key for 'id'", function() {
        expect(pushstream.jsonIdKey).toBe('id');
      });

      it("should has a key for 'channel'", function() {
        expect(pushstream.jsonChannelKey).toBe('channel');
      });

      it("should has a key for 'text'", function() {
        expect(pushstream.jsonTextKey).toBe('text');
      });

      it("should has a key for 'tag'", function() {
        expect(pushstream.jsonTagKey).toBe('tag');
      });

      it("should has a key for 'time'", function() {
        expect(pushstream.jsonTimeKey).toBe('time');
      });

      it("should has a key for 'eventid'", function() {
        expect(pushstream.jsonEventIdKey).toBe('eventid');
      });
    });

    describe("for arguments names", function() {
      it("should has a argument for 'tag'", function() {
        expect(pushstream.tagArgument).toBe('tag');
      });

      it("should has a argument for 'time'", function() {
        expect(pushstream.timeArgument).toBe('time');
      });

      it("should has a argument for 'eventid'", function() {
        expect(pushstream.eventIdArgument).toBe('eventid');
      });

      it("should has a argument for 'channels'", function() {
        expect(pushstream.channelsArgument).toBe('channels');
      });
    });

    it("should has all modes availables", function() {
      expect(pushstream.modes).toEqual(['eventsource', 'websocket', 'stream', 'longpolling']);
    });

    it("should define callbacks attributes", function() {
      expect(pushstream.onchanneldeleted).toBeDefined();
      expect(pushstream.onmessage).toBeDefined();
      expect(pushstream.onerror).toBeDefined();
      expect(pushstream.onstatuschange).toBeDefined();
    });

    it("should has an empty channels list", function() {
      expect(pushstream.channels).toEqual({});
      expect(pushstream.channelsCount).toBe(0);
    });

    it("should use the url path to send channels names instead of a query string parameter", function() {
      expect(pushstream.channelsByArgument).toBeFalsy();
    });

    it("should use headers to set values to request old messages or indicate the last received message instead of a query string parameter", function() {
      expect(pushstream.messagesControlByArgument).toBeFalsy();
    });
  });

  describe("when manipulating channels", function() {
    var pushstream = null;
    beforeEach(function() {
      pushstream = new PushStream();
    });

    describe("and is not connected", function() {

      describe("and is adding a channel", function() {
        it("should keep channel name", function() {
          pushstream.addChannel("ch1");
          expect(pushstream.channels.ch1).toBeDefined();
        });

        it("should keep channel options", function() {
          var options = {key:"value"};
          pushstream.addChannel("ch2", options);
          expect(pushstream.channels.ch2).toBe(options);
        });

        it("should increment channels counter", function() {
          var count = pushstream.channelsCount;
          pushstream.addChannel("ch3");
          expect(pushstream.channelsCount).toBe(count + 1);
        });
      });

      describe("and is removing a channel", function() {
        beforeEach(function() {
          pushstream.addChannel("ch1", {key:"value1"});
          pushstream.addChannel("ch2", {key:"value2"});
          pushstream.addChannel("ch3");
        });

        it("should remove channel name and options", function() {
          pushstream.removeChannel("ch2");
          expect(pushstream.channels.ch1).toEqual({key:"value1"});
          expect(pushstream.channels.ch2).not.toBeDefined();
          expect(pushstream.channels.ch3).toBeDefined();
        });

        it("should decrement channels counter", function() {
          var count = pushstream.channelsCount;
          pushstream.removeChannel("ch2");
          expect(pushstream.channelsCount).toBe(count - 1);
        });
      });

      describe("and is removing all channels", function() {
        beforeEach(function() {
          pushstream.addChannel("ch1", {key:"value1"});
          pushstream.addChannel("ch2", {key:"value2"});
          pushstream.addChannel("ch3");
        });

        it("should remove channels names and options", function() {
          pushstream.removeAllChannels();
          expect(pushstream.channels.ch1).not.toBeDefined();
          expect(pushstream.channels.ch2).not.toBeDefined();
          expect(pushstream.channels.ch3).not.toBeDefined();
        });

        it("should reset channels counter", function() {
          pushstream.removeAllChannels();
          expect(pushstream.channelsCount).toBe(0);
        });
      });
    });
  });

  it("should define an id as a sequential number based on PushStreamManager size", function() {
    var p1 = new PushStream();
    var p2 = new PushStream();
    expect(p1.id).toBe(p2.id - 1);
    expect(p2.id).toBe(PushStreamManager.length - 1);
  });

  describe("when checking available modes", function() {
    var eventsourceClass = null;

    beforeEach(function() {
      eventsourceClass = window.EventSource;
      window.EventSource = null;
    });

    afterEach(function() { window.EventSource = eventsourceClass; });

    it("should use only connection modes supported by the browser on the given order", function() {
      var pushstream = new PushStream({modes: "stream|eventsource|longpolling"});
      expect(pushstream.wrappers.length).toBe(2);
      expect(pushstream.wrappers[0].type).toBe("Stream");
      expect(pushstream.wrappers[1].type).toBe("LongPolling");
    });
  });

  function itShouldHaveCommonBehavior(mode, useJSONP) {
    var pushstream = null;
    var channelName = null;
    var port = 9080;
    var nginxServer = "localhost:" + port;
    var jsonp = useJSONP || false;
    var urlPrefixLongpolling = useJSONP ? '/jsonp' : '/lp';

    beforeEach(function() {
      for (var i = 0; i < PushStreamManager.length; i++) {
        PushStreamManager[i].disconnect();
      }
      channelName = "ch_" + new Date().getTime() + "_" + Math.floor((Math.random() * 1000) + 1);
    });

    afterEach(function() {
      for (var i = 0; i < PushStreamManager.length; i++) {
        PushStreamManager[i].disconnect();
      }
    });

    describe("when connecting", function() {
      it("should call onstatuschange callback", function(done) {
        var status = [];
        pushstream = new PushStream({
          modes: mode,
          port: port,
          useJSONP: jsonp,
          urlPrefixLongpolling: urlPrefixLongpolling,
          onstatuschange: function(st) {
            status.push(st);
          }
        });
        pushstream.addChannel(channelName);

        pushstream.connect();

        waitsForAndRuns(
          function() { return status.length >= 2; },

          function() {
            expect(status).toEqual([PushStream.CONNECTING, PushStream.OPEN]);
            setTimeout(function() {
              $.ajax({
                url: "http://" + nginxServer + "/pub?id=" + channelName,
                success: function(data) {
                  expect(data.subscribers).toBe(1);
                  done();
                }
              });
            }, 1000);
          },

          1000
        );
      });
    });

    describe("when receiving a message", function() {
      it("should call onmessage callback", function(done) {
        var receivedMessage = false;
        pushstream = new PushStream({
          modes: mode,
          port: port,
          useJSONP: jsonp,
          urlPrefixLongpolling: urlPrefixLongpolling,
          onmessage: function(text, id, channel, eventid, isLastMessageFromBatch, time) {
            expect([text, id, channel, eventid, isLastMessageFromBatch]).toEqual(["a test message", 1, channelName, "", true]);
            expect(new Date(time).getTime()).toBeLessThan(new Date().getTime());
            receivedMessage = true;
          }
        });
        pushstream.addChannel(channelName);

        pushstream.connect();

        setTimeout(function() {
          $.post("http://" + nginxServer + "/pub?id=" + channelName, "a test message");
        }, 500);

        waitsForAndRuns(
          function() { return receivedMessage; },
          function() { done(); },
          1000
        );
      });
    });

    describe("when disconnecting", function() {
      it("should call onstatuschange callback with CLOSED status", function(done) {
        var status = null;
        pushstream = new PushStream({
          modes: mode,
          port: port,
          useJSONP: jsonp,
          urlPrefixLongpolling: urlPrefixLongpolling,
          onstatuschange: function(st) {
            status = st;
          }
        });
        pushstream.addChannel(channelName);

        pushstream.connect();

        setTimeout(function() {
          $.ajax({
            url: "http://" + nginxServer + "/pub?id=" + channelName,
            success: function(data) {
              expect(data.subscribers).toBe(1);
              pushstream.disconnect();
            }
          });
        }, 500);


        waitsForAndRuns(
          function() { return status == PushStream.CLOSED; },

          function() {
            expect(pushstream.readyState).toBe(PushStream.CLOSED);
            done();
          },

          1000
        );
      });
    });

    if ((mode === "websocket") || (mode === "stream")) {
      describe("when the connection timeout", function() {
        it("should call onerror callback with a timeout error type", function(done) {
          var error = null;
          pushstream = new PushStream({
            modes: mode,
            port: port,
            onerror: function(err) {
              error = err;
            }
          });
          pushstream.addChannel(channelName);

          pushstream.connect();

          waitsForAndRuns(
            function() { return error !== null; },

            function() {
              expect(pushstream.readyState).toBe(PushStream.CLOSED);
              expect(error.type).toBe("timeout");
              done();
            },

            6000
          );
        });
      });

      describe("when reconnecting", function() {
        it("should reconnect after disconnected by the server", function(done) {
          var status = [];
          pushstream = new PushStream({
            modes: mode,
            port: port,
            useJSONP: jsonp,
            urlPrefixLongpolling: urlPrefixLongpolling,
            reconnectOnTimeoutInterval: 500,
            reconnectOnChannelUnavailableInterval: 500,
            onstatuschange: function(st) {
              if (PushStream.OPEN === st) {
                status.push(st);
              }
            }
          });
          pushstream.addChannel(channelName);

          pushstream.connect();

          waitsForAndRuns(
            function() { return status.length >= 2; },

            function() {
              expect(status).toEqual([PushStream.OPEN, PushStream.OPEN]);
              setTimeout(function() {
                $.ajax({
                  url: "http://" + nginxServer + "/pub?id=" + channelName,
                  success: function(data) {
                    expect(data.subscribers).toBe(1);
                    done();
                  }
                });
              }, 1000);
            },

            7000
          );
        });

        it("should not reconnect after disconnected by the server if autoReconnect is off", function(done) {
          var status = [];
          pushstream = new PushStream({
            modes: mode,
            port: port,
            useJSONP: jsonp,
            urlPrefixLongpolling: urlPrefixLongpolling,
            reconnectOnTimeoutInterval: 500,
            reconnectOnChannelUnavailableInterval: 500,
            autoReconnect: false,
            onstatuschange: function(st) {
              status.push(st);
            }
          });
          pushstream.addChannel(channelName);

          pushstream.connect();

          waitsForAndRuns(
            function() { return status.length >= 3; },

            function() {
              expect(status).toEqual([PushStream.CONNECTING, PushStream.OPEN, PushStream.CLOSED]);
              setTimeout(function() {
                $.ajax({
                  url: "http://" + nginxServer + "/pub?id=" + channelName,
                  success: function(data) {
                    expect(data.subscribers).toBe(0);
                    done();
                  }
                });
              }, 2000);
            },

            7000
          );
        });
      });
    }

    describe("when adding a new channel", function() {
      it("should reconnect", function(done) {
        var status = [];
        var messages = [];
        pushstream = new PushStream({
          modes: mode,
          port: port,
          useJSONP: jsonp,
          urlPrefixLongpolling: '/jsonp',
          onstatuschange: function(st) {
            status.push(st);
          },
          onmessage: function(text, id, channel, eventid, isLastMessageFromBatch) {
            messages.push([text, id, channel, eventid, isLastMessageFromBatch]);
          }
        });
        pushstream.addChannel(channelName);

        pushstream.connect();

        setTimeout(function() {
          pushstream.addChannel("other_" + channelName);
        }, 200);

        waitsForAndRuns(
          function() { return pushstream.channelsCount >= 2; },

          function() {
            setTimeout(function() {
              $.post("http://" + nginxServer + "/pub?id=" + channelName, "a test message", function() {
                setTimeout(function() {
                  $.post("http://" + nginxServer + "/pub?id=" + "other_" + channelName, "message on other channel");
                }, 700);
              });
            }, 700);
          },

          300
        );

        waitsForAndRuns(
          function() { return messages.length >= 2; },

          function() {
            expect(status).toEqual([PushStream.CONNECTING, PushStream.OPEN, PushStream.CLOSED, PushStream.CONNECTING, PushStream.OPEN]);
            expect(messages[0]).toEqual(["a test message", 1, channelName, "", true]);
            expect(messages[1]).toEqual(["message on other channel", 1, "other_" + channelName, "", true]);
            done();
          },

          2500
        );
      });
    });

    describe("when deleting a channel", function() {
      it("should call onchanneldeleted callback", function(done) {
        var channel = null;
        pushstream = new PushStream({
          modes: mode,
          port: port,
          useJSONP: jsonp,
          urlPrefixLongpolling: urlPrefixLongpolling,
          onchanneldeleted: function(ch) {
            channel = ch;
          }
        });
        pushstream.addChannel(channelName);

        pushstream.connect();

        setTimeout(function() {
           $.ajax({type: "DELETE", url: "http://" + nginxServer + "/pub?id=" + channelName});
        }, 500);

        waitsForAndRuns(
          function() { return channel !== null; },

          function() {
            $.post("http://" + nginxServer + "/pub?id=" + channelName, "a test message", function() {
              $.ajax({
                url: "http://" + nginxServer + "/pub?id=" + channelName,
                success: function(data) {
                  expect(data.published_messages).toBe(1);
                }
              });
            });
            expect(channel).toBe(channelName);
            done();
          },

          1000
        );
      });
    });

    describe("when sending extra params", function() {
      it("should call extraParams function", function(done) {
        var receivedMessage = false;
        pushstream = new PushStream({
          modes: mode,
          port: port,
          useJSONP: jsonp,
          urlPrefixLongpolling: urlPrefixLongpolling,
          extraParams: function() {
            return {"tests":"on"};
          },
          onmessage: function(text, id, channel, eventid, isLastMessageFromBatch) {
            expect([text, id, channel, eventid, isLastMessageFromBatch]).toEqual(["a test message", 1, "test_" + channelName, "", true]);
            receivedMessage = true;
          }
        });
        pushstream.addChannel(channelName);

        pushstream.connect();

        setTimeout(function() {
          $.post("http://" + nginxServer + "/pub?id=" + "test_" + channelName, "a test message");
        }, 500);

        waitsForAndRuns(
          function() { return receivedMessage; },
          function() { done(); },
          1000
        );
      });
    });

    describe("when an error on connecting happens", function() {
      it("should call onerror callback with a load error type", function(done) {
        var error = null;
        pushstream = new PushStream({
          modes: mode,
          port: port,
          useJSONP: jsonp,
          urlPrefixStream: '/pub',
          urlPrefixEventsource: '/pub',
          urlPrefixLongpolling: '/pub',
          urlPrefixWebsocket: '/pub',
          onerror: function(err) {
            error = err;
          }
        });
        pushstream.addChannel(channelName);

        pushstream.connect();

        waitsForAndRuns(
          function() { return error !== null; },

          function() {
            expect(pushstream.readyState).toBe(PushStream.CLOSED);
            expect(error.type).toBe("load");
            done();
          },

          3000
        );
      });
    });

    describe("when getting old messages", function() {
      it("should be possible use time", function(done) {
        var messages = [];
        var receivedMessage = receivedMessage2 = false;
        var finished = false;
        pushstream = new PushStream({
          messagesControlByArgument: true,
          messagesPublishedAfter: 1,
          modes: mode,
          port: port,
          useJSONP: jsonp,
          urlPrefixLongpolling: urlPrefixLongpolling,
          extraParams: function() {
            return {"qs":"on"};
          },
          onmessage: function(text, id, channel, eventid, isLastMessageFromBatch, time) {
            messages.push([text, id, channel, eventid, isLastMessageFromBatch, time]);
            if (messages.length == 1) {
              receivedMessage = true;
              pushstream.disconnect();
            }
            if (messages.length >= 2) {
              receivedMessage2 = true;
              pushstream.disconnect();
            }
          }
        });
        pushstream.addChannel(channelName);

        $.post("http://" + nginxServer + "/pub?id=" + channelName, "a test message", function() {
          pushstream.connect();
        });

        waitsForAndRuns(
          function() { return receivedMessage; },

          function() {
            setTimeout(function() {
              $.ajax({
                url: "http://" + nginxServer + "/pub?id=" + channelName,
                success: function(data) {
                  expect(data.subscribers).toBe(0);
                  $.post("http://" + nginxServer + "/pub?id=" + channelName, "another test message", function() {
                    pushstream.connect();
                  });
                }
              });
            }, 1500);
          },

          2000
        );

        waitsForAndRuns(
          function() { return receivedMessage2; },

          function() {
            setTimeout(function() {
              expect(messages[0].slice(0, -1)).toEqual(["a test message", 1, channelName, "", true]);
              expect(messages[1].slice(0, -1)).toEqual(["another test message", 2, channelName, "", true]);
              expect(new Date(messages[0][messages[0].length - 1]).getTime()).toBeLessThan(new Date(messages[1][messages[1].length - 1]).getTime());
              finished = true;
            }, 500);
          },

          3000
        );

        waitsForAndRuns(
          function() { return finished; },
          function() { done(); },
          5000
        );
      });

      it("should be possible use a Date object", function(done) {
        var messages = [];
        var receivedMessage = receivedMessage2 = false;
        var finished = false;
        var now = new Date();
        pushstream = new PushStream({
          messagesControlByArgument: true,
          messagesPublishedAfter: new Date(now.getTime() - 1000),
          modes: mode,
          port: port,
          useJSONP: jsonp,
          urlPrefixLongpolling: urlPrefixLongpolling,
          extraParams: function() {
            return {"qs":"on"};
          },
          onmessage: function(text, id, channel, eventid, isLastMessageFromBatch) {
            messages.push([text, id, channel, eventid, isLastMessageFromBatch]);
            if (messages.length == 1) {
              receivedMessage = true;
              pushstream.disconnect();
            }
            if (messages.length >= 2) {
              receivedMessage2 = true;
              pushstream.disconnect();
            }
          }
        });
        pushstream.addChannel(channelName);

        $.post("http://" + nginxServer + "/pub?id=" + channelName, "a test message", function() {
          pushstream.connect();
        });

        waitsForAndRuns(
          function() { return receivedMessage; },

          function() {
            setTimeout(function() {
              $.ajax({
                url: "http://" + nginxServer + "/pub?id=" + channelName,
                success: function(data) {
                  expect(data.subscribers).toBe(0);
                  $.post("http://" + nginxServer + "/pub?id=" + channelName, "another test message", function() {
                    pushstream.connect();
                  });
                }
              });
            }, 1500);
          },

          2000
        );

        waitsForAndRuns(
          function() { return receivedMessage2; },

          function() {
            setTimeout(function() {
              expect(messages[0]).toEqual(["a test message", 1, channelName, "", true]);
              expect(messages[1]).toEqual(["another test message", 2, channelName, "", true]);
              finished = true;
            }, 500);
          },

          3000
        );

        waitsForAndRuns(
          function() { return finished; },
          function() { done(); },
          5000
        );
      });

      it("should be possible use a negative value to get messages since epoch time", function(done) {
        var messages = [];
        var receivedMessage = receivedMessage2 = false;
        var finished = false;
        pushstream = new PushStream({
          messagesControlByArgument: true,
          messagesPublishedAfter: -10,
          modes: mode,
          port: port,
          useJSONP: jsonp,
          urlPrefixLongpolling: urlPrefixLongpolling,
          extraParams: function() {
            return {"qs":"on"};
          },
          onmessage: function(text, id, channel, eventid, isLastMessageFromBatch) {
            messages.push([text, id, channel, eventid, isLastMessageFromBatch]);
            if (messages.length == 2) {
              receivedMessage = true;
              // set a delay to wait for a ping message on streaming
              setTimeout(function() {
                pushstream.disconnect();
              }, (pushstream.wrapper.type === "LongPolling") ? 5 : 1500);
            }
            if (messages.length >= 4) {
              receivedMessage2 = true;
              pushstream.disconnect();
            }
          }
        });
        pushstream.addChannel(channelName);

        $.post("http://" + nginxServer + "/pub?id=" + channelName, "a test message 1", function() {
          $.post("http://" + nginxServer + "/pub?id=" + channelName, "a test message 2", function() {
            pushstream.connect();
          });
        });

        waitsForAndRuns(
          function() { return receivedMessage; },

          function() {
            setTimeout(function() {
              $.ajax({
                url: "http://" + nginxServer + "/pub?id=" + channelName,
                success: function(data) {
                  expect(data.subscribers).toBe(0);
                  $.post("http://" + nginxServer + "/pub?id=" + channelName, "another test message 1", function() {
                    $.post("http://" + nginxServer + "/pub?id=" + channelName, "another test message 2", function() {
                      pushstream.connect();
                    });
                  });
                }
              });
            }, 1500);
          },

          2000
        );

        waitsForAndRuns(
          function() { return receivedMessage2; },

          function() {
            setTimeout(function() {
              expect(messages[0]).toEqual(["a test message 1", 1, channelName, "", (pushstream.wrapper.type === "LongPolling") ? false : true]);
              expect(messages[1]).toEqual(["a test message 2", 2, channelName, "", true]);
              expect(messages[2]).toEqual(["another test message 1", 3, channelName, "", (pushstream.wrapper.type === "LongPolling") ? false : true]);
              expect(messages[3]).toEqual(["another test message 2", 4, channelName, "", true]);
              finished = true;
            }, 500);
          },

          3000
        );

        waitsForAndRuns(
          function() { return finished; },
          function() { done(); },
          5000
        );
      });

      it("should be possible use backtrack", function(done) {
        var messages = [];
        var receivedMessage = receivedMessage2 = false;
        var finished = false;
        pushstream = new PushStream({
          modes: mode,
          port: port,
          useJSONP: jsonp,
          urlPrefixLongpolling: urlPrefixLongpolling,
          extraParams: function() {
            return {"qs":"on"};
          },
          onmessage: function(text, id, channel, eventid, isLastMessageFromBatch) {
            messages.push([text, id, channel, eventid, isLastMessageFromBatch]);
            if (messages.length == 1) {
              receivedMessage = true;
              pushstream.disconnect();
            }
            if (messages.length >= 2) {
              receivedMessage2 = true;
              pushstream.disconnect();
            }
          }
        });
        pushstream.addChannel(channelName, {backtrack: 1});

        $.post("http://" + nginxServer + "/pub?id=" + channelName, "a test message 1", function() {
          $.post("http://" + nginxServer + "/pub?id=" + channelName, "a test message 2", function() {
            pushstream.connect();
          });
        });

        waitsForAndRuns(
          function() { return receivedMessage; },

          function() {
            setTimeout(function() {
              $.ajax({
                url: "http://" + nginxServer + "/pub?id=" + channelName,
                success: function(data) {
                  expect(data.subscribers).toBe(0);
                  $.post("http://" + nginxServer + "/pub?id=" + channelName, "another test message 1", function() {
                    $.post("http://" + nginxServer + "/pub?id=" + channelName, "another test message 2", function() {
                      pushstream.connect();
                    });
                  });
                }
              });
            }, 1500);
          },

          2000
        );

        waitsForAndRuns(
          function() { return receivedMessage2; },

          function() {
            setTimeout(function() {
              expect(messages[0]).toEqual(["a test message 2", 2, channelName, "", true]);
              if (jsonp) {
                expect(messages[1]).toEqual(["another test message 1", 3, channelName, "", false]);
                expect(messages[2]).toEqual(["another test message 2", 4, channelName, "", true]);
              } else {
                expect(messages[1]).toEqual(["another test message 2", 4, channelName, "", true]);
              }
              finished = true;
            }, 500);
          },

          3000
        );

        waitsForAndRuns(
          function() { return finished; },
          function() { done(); },
          5000
        );
      });

      it("should be possible use event_id", function(done) {
        var messages = [];
        var receivedMessage = receivedMessage2 = false;
        var finished = false;
        pushstream = new PushStream({
          messagesControlByArgument: true,
          lastEventId: "some_event_id",
          modes: mode,
          port: port,
          useJSONP: jsonp,
          urlPrefixLongpolling: urlPrefixLongpolling,
          extraParams: function() {
            return {"qs":"on"};
          },
          onmessage: function(text, id, channel, eventid, isLastMessageFromBatch) {
            messages.push([text, id, channel, eventid, isLastMessageFromBatch]);
            if (messages.length == 1) {
              receivedMessage = true;
              pushstream.disconnect();
            }
            if (messages.length >= 3) {
              receivedMessage2 = true;
              pushstream.disconnect();
            }
          }
        });
        pushstream.addChannel(channelName);

        $.post("http://" + nginxServer + "/pub?id=" + channelName, "a test message 1", function() {
          $.ajax({ url: "http://" + nginxServer + "/pub?id=" + channelName,
            type: "POST", data: "a test message 2",
            beforeSend: function(req) { req.setRequestHeader("Event-Id", "some_event_id"); },
            success: function() {
              $.ajax({ url: "http://" + nginxServer + "/pub?id=" + channelName,
                type: "POST", data: "a test message 3",
                beforeSend: function(req) { req.setRequestHeader("Event-Id", "some_event_id_2"); },
                success: function() {
                  pushstream.connect();
                }
              });
            }
          });
        });

        waitsForAndRuns(
          function() { return receivedMessage; },

          function() {
            setTimeout(function() {
              $.ajax({
                url: "http://" + nginxServer + "/pub?id=" + channelName,
                success: function(data) {
                  expect(data.subscribers).toBe(0);
                  $.post("http://" + nginxServer + "/pub?id=" + channelName, "another test message 1", function() {
                    $.ajax({
                      url: "http://" + nginxServer + "/pub?id=" + channelName,
                      type: "post",
                      data: "another test message 2",
                      beforeSend: function(req) { req.setRequestHeader("Event-Id", "some_other_event_id"); },
                      success: function() {
                        pushstream.connect();
                      }
                    });
                  });
                }
              });
            }, 1500);
          },

          2000
        );

        waitsForAndRuns(
          function() { return receivedMessage2; },

          function() {
            setTimeout(function() {
              expect(messages[0]).toEqual(["a test message 3", 3, channelName, "some_event_id_2", true]);
              expect(messages[1]).toEqual(["another test message 1", 4, channelName, "", (pushstream.wrapper.type !== "LongPolling")]);
              expect(messages[2]).toEqual(["another test message 2", 5, channelName, "some_other_event_id", true]);
              finished = true;
            }, 500);
          },

          3000
        );

        waitsForAndRuns(
          function() { return finished; },
          function() { done(); },
          5000
        );
      });

      it("should be possible mix backtrack and time", function(done) {
        var messages = [];
        var receivedMessage = receivedMessage2 = false;
        var finished = false;
        pushstream = new PushStream({
          messagesControlByArgument: true,
          modes: mode,
          port: port,
          useJSONP: jsonp,
          urlPrefixLongpolling: urlPrefixLongpolling,
          extraParams: function() {
            return {"qs":"on"};
          },
          onmessage: function(text, id, channel, eventid, isLastMessageFromBatch) {
            messages.push([text, id, channel, eventid, isLastMessageFromBatch]);
            if (messages.length >= 3) {
              receivedMessage2 = true;
              pushstream.disconnect();
            }
            if (messages.length == 1) {
              receivedMessage = true;
              pushstream.disconnect();
            }
          }
        });
        pushstream.addChannel(channelName, {backtrack: 1});

        $.post("http://" + nginxServer + "/pub?id=" + channelName, "a test message 1", function() {
          $.post("http://" + nginxServer + "/pub?id=" + channelName, "a test message 2", function() {
            pushstream.connect();
          });
        });

        waitsForAndRuns(
          function() { return receivedMessage; },

          function() {
            setTimeout(function() {
              $.ajax({
                url: "http://" + nginxServer + "/pub?id=" + channelName,
                success: function(data) {
                  expect(data.subscribers).toBe(0);
                  $.post("http://" + nginxServer + "/pub?id=" + channelName, "another test message 1", function() {
                    $.post("http://" + nginxServer + "/pub?id=" + channelName, "another test message 2", function() {
                      pushstream.connect();
                    });
                  });
                }
              });
            }, 1500);
          },

          2000
        );

        waitsForAndRuns(
          function() { return receivedMessage2; },

          function() {
            setTimeout(function() {
              expect(messages[0]).toEqual(["a test message 2", 2, channelName, "", true]);
              expect(messages[1]).toEqual(["another test message 1", 3, channelName, "", (pushstream.wrapper.type !== "LongPolling")]);
              expect(messages[2]).toEqual(["another test message 2", 4, channelName, "", true]);
              finished = true;
            }, 500);
          },

          3000
        );

        waitsForAndRuns(
          function() { return finished; },
          function() { done(); },
          5000
        );
      });
    });
  };

  describe("on Stream mode", function() {
    itShouldHaveCommonBehavior('stream');
  });

  describe("on EventSource mode", function() {
    if (window.EventSource) {
      itShouldHaveCommonBehavior('eventsource');
    }
  });

  describe("on WebSocket mode", function() {
    if (window.WebSocket || window.MozWebSocket) {
      itShouldHaveCommonBehavior('websocket');
    }
  });

  describe("on LongPolling mode", function() {
    itShouldHaveCommonBehavior('longpolling');
  });

  describe("on JSONP mode", function() {
    itShouldHaveCommonBehavior('longpolling', true);
  });
});
