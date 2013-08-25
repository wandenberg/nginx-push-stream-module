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

    it("should set state as uninitialised", function() {
      expect(pushstream.readyState).toBe(PushStream.CLOSED);
    });

    describe("for operation timeouts", function() {
      it("should has a connection timeout", function() {
        expect(pushstream.timeout).toBe(15000);
      });

      it("should has a ping message timeout", function() {
        expect(pushstream.pingtimeout).toBe(30000);
      });

      it("should has a reconnect interval, to be used when a timeout happens", function() {
        expect(pushstream.reconnecttimeout).toBe(3000);
      });

      it("should has a reconnect interval, to be used when a channel is unavailable", function() {
        expect(pushstream.checkChannelAvailabilityInterval).toBe(60000);
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
        expect(pushstream.jsonDataKey).toBe('text');
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
        expect(pushstream.longPollingTagArgument).toBe('tag');
      });

      it("should has a argument for 'time'", function() {
        expect(pushstream.longPollingTimeArgument).toBe('time');
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
});
