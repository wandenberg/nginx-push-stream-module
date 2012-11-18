describe("PushStreamUtils", function() {
  var jsonKeys = {
    jsonIdKey      : 'id',
    jsonChannelKey : 'channel',
    jsonDataKey    : 'text',
    jsonTagKey     : 'tag',
    jsonTimeKey    : 'time',
    jsonEventIdKey : 'eventid'
  };

  beforeEach(function() {
  });

  describe("when parse JSON", function() {
    it("should return null when data is null", function () {
      expect(parseJSON(null)).toBe(null);
    });

    it("should return null when data is undefined", function () {
      expect(parseJSON(undefined)).toBe(null);
    });

    it("should return null when data is not a string", function () {
      expect(parseJSON({})).toBe(null);
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
          parseJSON('{"a":1}')
          expect(window.JSON.parse).toHaveBeenCalledWith('{"a":1}');
        });

        it("should parse a well formed json string", function () {
          expect(parseJSON('{"a":1}')["a"]).toBe(1);
        });

        it("should parse when the string has leading spaces", function () {
          expect(parseJSON('  {"a":1}')["a"]).toBe(1);
        });

        it("should parse when the string has trailing spaces", function () {
          expect(parseJSON('{"a":1}  ')["a"]).toBe(1);
        });

        it("should raise error when string is a invalid json", function () {
          expect(function () { parseJSON('{"a":1[]}') }).toThrow('Invalid JSON: {"a":1[]}');
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
        expect(parseJSON('{"a":1}')["a"]).toBe(1);
      });

      it("should parse when the string has leading spaces", function () {
        expect(parseJSON('  {"a":1}')["a"]).toBe(1);
      });

      it("should parse when the string has trailing spaces", function () {
        expect(parseJSON('{"a":1}  ')["a"]).toBe(1);
      });

      it("should raise error when string is a invalid json", function () {
        expect(function () { parseJSON('{"a":1[]}') }).toThrow('Invalid JSON: {"a":1[]}');
      });
    });
  });

  describe("when extract xss domain", function() {
    it("should return the ip address when domain is only an ip", function() {
      expect(extract_xss_domain("201.10.32.52")).toBe("201.10.32.52");
    });

    it("should return the full domain when it has only two parts", function() {
      expect(extract_xss_domain("domain.com")).toBe("domain.com");
    });

    it("should return the last two parts when domain has three parts", function() {
      expect(extract_xss_domain("example.domain.com")).toBe("domain.com");
    });

    it("should return all parts minus the first one when domain has more than three parts", function() {
      expect(extract_xss_domain("another.example.domain.com")).toBe("example.domain.com");
    });
  });

  describe("when parsing a message", function() {
    it("should accept a simple string as text", function() {
      var message = parseMessage('{"id":31,"channel":"54x19","text":"some simple string"}', jsonKeys);
      expect(message.data).toBe("some simple string");
    });

    it("should accept a json as text", function() {
      var message = parseMessage('{"id":31,"channel":"54x19","text":{"id":"500516b7639e4029b8000001","type":"Player","change":{"loc":[54.34772390000001,18.5610535],"version":7}}}', jsonKeys);
      expect(message.data.id).toBe("500516b7639e4029b8000001");
      expect(message.data.type).toBe("Player");
      expect(message.data.change.loc[0]).toBe(54.34772390000001);
      expect(message.data.change.loc[1]).toBe(18.5610535);
      expect(message.data.change.version).toBe(7);
    });

    it("should accept an escaped json as text", function() {
      var message = parseMessage('{"id":31,"channel":"54x19","text":"%7B%22id%22%3A%22500516b7639e4029b8000001%22%2C%22type%22%3A%22Player%22%2C%22change%22%3A%7B%22loc%22%3A%5B54.34772390000001%2C18.5610535%5D%2C%22version%22%3A7%7D%7D"}', jsonKeys);
      expect(message.data).toBe('{"id":"500516b7639e4029b8000001","type":"Player","change":{"loc":[54.34772390000001,18.5610535],"version":7}}');
    });
  });
});
