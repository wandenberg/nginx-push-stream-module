describe("Utils", function() {
  var jsonKeys = {
    jsonIdKey      : 'id',
    jsonChannelKey : 'channel',
    jsonTextKey    : 'text',
    jsonTagKey     : 'tag',
    jsonTimeKey    : 'time',
    jsonEventIdKey : 'eventid'
  };

  beforeEach(function() {
  });

  describe("when formatting dates to UTC string", function() {
    it("should return the string with two digits for day", function () {
      expect(Utils.dateToUTCString(Date.fromISO("2012-11-09T12:00:00-03:00"))).toBe("Fri, 09 Nov 2012 15:00:00 GMT");
      expect(Utils.dateToUTCString(Date.fromISO("2012-11-10T12:00:00-03:00"))).toBe("Sat, 10 Nov 2012 15:00:00 GMT");
    });

    it("should return the string with two digits for hour", function () {
      expect(Utils.dateToUTCString(Date.fromISO("2012-11-09T06:00:00-03:00"))).toBe("Fri, 09 Nov 2012 09:00:00 GMT");
      expect(Utils.dateToUTCString(Date.fromISO("2012-11-10T07:00:00-03:00"))).toBe("Sat, 10 Nov 2012 10:00:00 GMT");
    });

    it("should return the string with two digits for minute", function () {
      expect(Utils.dateToUTCString(Date.fromISO("2012-11-09T06:09:00-03:00"))).toBe("Fri, 09 Nov 2012 09:09:00 GMT");
      expect(Utils.dateToUTCString(Date.fromISO("2012-11-10T07:10:00-03:00"))).toBe("Sat, 10 Nov 2012 10:10:00 GMT");
    });

    it("should return the string with two digits for second", function () {
      expect(Utils.dateToUTCString(Date.fromISO("2012-11-09T06:09:09-03:00"))).toBe("Fri, 09 Nov 2012 09:09:09 GMT");
      expect(Utils.dateToUTCString(Date.fromISO("2012-11-10T07:10:10-03:00"))).toBe("Sat, 10 Nov 2012 10:10:10 GMT");
    });

    it("should return the right text for months", function () {
      expect(Utils.dateToUTCString(Date.fromISO("2012-01-09T06:09:09-03:00"))).toBe("Mon, 09 Jan 2012 09:09:09 GMT");
      expect(Utils.dateToUTCString(Date.fromISO("2012-02-09T06:09:09-03:00"))).toBe("Thu, 09 Feb 2012 09:09:09 GMT");
      expect(Utils.dateToUTCString(Date.fromISO("2012-03-09T06:09:09-03:00"))).toBe("Fri, 09 Mar 2012 09:09:09 GMT");
      expect(Utils.dateToUTCString(Date.fromISO("2012-04-09T06:09:09-03:00"))).toBe("Mon, 09 Apr 2012 09:09:09 GMT");
      expect(Utils.dateToUTCString(Date.fromISO("2012-05-09T06:09:09-03:00"))).toBe("Wed, 09 May 2012 09:09:09 GMT");
      expect(Utils.dateToUTCString(Date.fromISO("2012-06-09T06:09:09-03:00"))).toBe("Sat, 09 Jun 2012 09:09:09 GMT");
      expect(Utils.dateToUTCString(Date.fromISO("2012-07-09T06:09:09-03:00"))).toBe("Mon, 09 Jul 2012 09:09:09 GMT");
      expect(Utils.dateToUTCString(Date.fromISO("2012-08-09T06:09:09-03:00"))).toBe("Thu, 09 Aug 2012 09:09:09 GMT");
      expect(Utils.dateToUTCString(Date.fromISO("2012-09-09T06:09:09-03:00"))).toBe("Sun, 09 Sep 2012 09:09:09 GMT");
      expect(Utils.dateToUTCString(Date.fromISO("2012-10-09T06:09:09-03:00"))).toBe("Tue, 09 Oct 2012 09:09:09 GMT");
      expect(Utils.dateToUTCString(Date.fromISO("2012-11-09T06:09:09-03:00"))).toBe("Fri, 09 Nov 2012 09:09:09 GMT");
      expect(Utils.dateToUTCString(Date.fromISO("2012-12-09T06:09:09-03:00"))).toBe("Sun, 09 Dec 2012 09:09:09 GMT");
    });

    it("should return the right text for days", function () {
      expect(Utils.dateToUTCString(Date.fromISO("2012-01-01T06:09:09-03:00"))).toBe("Sun, 01 Jan 2012 09:09:09 GMT");
      expect(Utils.dateToUTCString(Date.fromISO("2012-01-02T06:09:09-03:00"))).toBe("Mon, 02 Jan 2012 09:09:09 GMT");
      expect(Utils.dateToUTCString(Date.fromISO("2012-01-03T06:09:09-03:00"))).toBe("Tue, 03 Jan 2012 09:09:09 GMT");
      expect(Utils.dateToUTCString(Date.fromISO("2012-01-04T06:09:09-03:00"))).toBe("Wed, 04 Jan 2012 09:09:09 GMT");
      expect(Utils.dateToUTCString(Date.fromISO("2012-01-05T06:09:09-03:00"))).toBe("Thu, 05 Jan 2012 09:09:09 GMT");
      expect(Utils.dateToUTCString(Date.fromISO("2012-01-06T06:09:09-03:00"))).toBe("Fri, 06 Jan 2012 09:09:09 GMT");
      expect(Utils.dateToUTCString(Date.fromISO("2012-01-07T06:09:09-03:00"))).toBe("Sat, 07 Jan 2012 09:09:09 GMT");
    });
  });

  describe("when parse JSON", function() {
    it("should return null when data is null", function () {
      expect(Utils.parseJSON(null)).toBe(null);
    });

    it("should return null when data is undefined", function () {
      expect(Utils.parseJSON(undefined)).toBe(null);
    });

    it("should return null when data is not a string", function () {
      expect(Utils.parseJSON({})).toBe(null);
    });

    if (window.JSON) {
      describe("when have a default implementation for JSON.parse", function () {
        var jsonImplementation = null;
        beforeEach(function() {
          jsonImplementation = window.JSON;
          // window.JSON = null;
        });

        afterEach(function() {
          window.JSON = jsonImplementation;
        });

        it("should use the browser default implementation when available", function () {
          spyOn(window.JSON, "parse");
          Utils.parseJSON('{"a":1}');
          expect(window.JSON.parse).toHaveBeenCalledWith('{"a":1}');
        });

        it("should parse a well formed json string", function () {
          expect(Utils.parseJSON('{"a":1}')["a"]).toBe(1);
        });

        it("should parse when the string has leading spaces", function () {
          expect(Utils.parseJSON('  {"a":1}')["a"]).toBe(1);
        });

        it("should parse when the string has trailing spaces", function () {
          expect(Utils.parseJSON('{"a":1}  ')["a"]).toBe(1);
        });

        it("should raise error when string is a invalid json", function () {
          expect(function () { Utils.parseJSON('{"a":1[]}'); }).toThrow('Invalid JSON: {"a":1[]}');
        });
      });
    }

    describe("when do not have a default implementation for JSON.parse", function () {
      var jsonImplementation = null;
      beforeEach(function() {
        jsonImplementation = window.JSON;
        window.JSON = null;
      });

      afterEach(function() {
        window.JSON = jsonImplementation;
      });

      it("should parse a well formed json string", function () {
        expect(Utils.parseJSON('{"a":1}')["a"]).toBe(1);
      });

      it("should parse when the string has leading spaces", function () {
        expect(Utils.parseJSON('  {"a":1}')["a"]).toBe(1);
      });

      it("should parse when the string has trailing spaces", function () {
        expect(Utils.parseJSON('{"a":1}  ')["a"]).toBe(1);
      });

      it("should raise error when string is a invalid json", function () {
        expect(function () { Utils.parseJSON('{"a":1[]}'); }).toThrow('Invalid JSON: {"a":1[]}');
      });
    });
  });

  describe("when extract xss domain", function() {
    it("should return the ip address when domain is only an ip", function() {
      expect(Utils.extract_xss_domain("201.10.32.52")).toBe("201.10.32.52");
    });

    it("should return the full domain when it has only two parts", function() {
      expect(Utils.extract_xss_domain("domain.com")).toBe("domain.com");
    });

    it("should return the last two parts when domain has three parts", function() {
      expect(Utils.extract_xss_domain("example.domain.com")).toBe("domain.com");
    });

    it("should return all parts minus the first one when domain has more than three parts", function() {
      expect(Utils.extract_xss_domain("another.example.domain.com")).toBe("example.domain.com");
    });
  });

  describe("when parsing a message", function() {
    it("should accept a simple string as text", function() {
      var message = Utils.parseMessage('{"id":31,"channel":"54x19","text":"some simple string"}', jsonKeys);
      expect(message.text).toBe("some simple string");
    });

    it("should accept a json as text", function() {
      var message = Utils.parseMessage('{"id":31,"channel":"54x19","text":{"id":"500516b7639e4029b8000001","type":"Player","change":{"loc":[54.34772390000001,18.5610535],"version":7}}}', jsonKeys);
      expect(message.text.id).toBe("500516b7639e4029b8000001");
      expect(message.text.type).toBe("Player");
      expect(message.text.change.loc[0]).toBe(54.34772390000001);
      expect(message.text.change.loc[1]).toBe(18.5610535);
      expect(message.text.change.version).toBe(7);
    });

    it("should accept an escaped json as text", function() {
      var message = Utils.parseMessage('{"id":31,"channel":"54x19","text":"%7B%22id%22%3A%22500516b7639e4029b8000001%22%2C%22type%22%3A%22Player%22%2C%22change%22%3A%7B%22loc%22%3A%5B54.34772390000001%2C18.5610535%5D%2C%22version%22%3A7%7D%7D"}', jsonKeys);
      expect(message.text).toBe('{"id":"500516b7639e4029b8000001","type":"Player","change":{"loc":[54.34772390000001,18.5610535],"version":7}}');
    });
  });
});
