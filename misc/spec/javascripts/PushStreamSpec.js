describe("PushStream", function() {

  beforeEach(function() {
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

    it("should set seconds ago as NaN", function() {
      expect(isNaN(pushstream.secondsAgo)).toBeTruthy();
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
      var pushstream = new PushStream({modes: "stream|eventsource|longpolling"})
      expect(pushstream.wrappers.length).toBe(2);
      expect(pushstream.wrappers[0].type).toBe("Stream");
      expect(pushstream.wrappers[1].type).toBe("LongPolling");
    });
  });
});
