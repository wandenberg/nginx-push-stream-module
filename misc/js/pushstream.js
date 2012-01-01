/*global PushStream */
/**
 * Copyright (C) 2010-2011 Wandenberg Peixoto <wandenberg@gmail.com>, Rogério Carvalho Schneider <stockrt@gmail.com>
 *
 * This file is part of Nginx Push Stream Module.
 *
 * Nginx Push Stream Module is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Nginx Push Stream Module is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Nginx Push Stream Module.  If not, see <http://www.gnu.org/licenses/>.
 *
 *
 * pushstream.js
 *
 * Created: Nov 01, 2011
 * Authors: Wandenberg Peixoto <wandenberg@gmail.com>, Rogério Carvalho Schneider <stockrt@gmail.com>
 */
(function (window, undefined) {
  /* prevent duplicate declaration */
  if (window.PushStream) { return; }

  var PATTERN_MESSAGE = /\{"id":([\-\d]*),"channel":"([^"]*)","text":"(.*)"\}/;
  var PATTERN_MESSAGE_WITH_EVENT_ID = /\{"id":([\-\d]*),"channel":"([^"]*)","text":"(.*)","eventid":"(.*)"\}/;

  var Log4js = {
    debug : function() { if  (PushStream.LOG_LEVEL === 'debug')                                         Log4js._log.apply(Log4js._log, arguments); },
    info  : function() { if ((PushStream.LOG_LEVEL === 'info')  || (PushStream.LOG_LEVEL === 'debug'))  Log4js._log.apply(Log4js._log, arguments); },
    error : function() {                                                                                Log4js._log.apply(Log4js._log, arguments); },
    _log  : function() {
      if (window.console && window.console.log && window.console.log.apply) {
        window.console.log.apply(window.console, arguments);
      }

      var logElement = document.getElementById(PushStream.LOG_OUTPUT_ELEMENT_ID);
      if (logElement) {
        var str = '';
        for (var i = 0; i < arguments.length; i++) {
          str += arguments[i] + " ";
        }
        logElement.innerHTML += str + '\n';

        var lines = logElement.innerHTML.split('\n');
        if (lines.length > 100) {
          logElement.innerHTML = lines.slice(-100).join('\n');
        }
      }
    }
  };

  var Ajax = {
    _getXHRObject : function() {
      var xhr = false;
      try { xhr = new window.XMLHttpRequest(); }
      catch (e1) {
        try { xhr = new window.ActiveXObject("Msxml2.XMLHTTP"); }
        catch (e2) {
          try { xhr = new window.ActiveXObject("Microsoft.XMLHTTP"); }
          catch (e3) {
            xhr = false;
          }
        }
      }
      return xhr;
    },
    _send : function(settings, post) {
      settings = settings || {};
      var cache = settings.cache || true;
      var xhr = Ajax._getXHRObject();
      if (!xhr||!settings.url) return;

      var url = settings.url;
      if (!(cache || post)) {
        var now = new Date();
        url += ((settings.url.indexOf("?")+1) ? "&" : "?") + "_=" + now.getTime();
      }

      xhr.open(((post) ? "POST" : "GET"), url, true);

      xhr.onreadystatechange = function () {
        if (xhr.readyState == 4) {
          if (settings.afterReceive) settings.afterReceive(xhr);
          if(xhr.status == 200) {
            if (settings.success) settings.success(xhr.responseText);
          } else {
            if (settings.error) settings.error(xhr.status);
          }
        }
      }
      if (settings.beforeSend) settings.beforeSend(xhr);

      if (post) {
        xhr.setRequestHeader("Accept", "application/json");
        xhr.setRequestHeader("Content-type", "application/x-www-form-urlencoded");
      }
      xhr.send((post) ? settings.data : null);
      return xhr;
    },
    load : function(settings) {
      return Ajax._send(settings, false);
    },
    post : function(settings) {
      return Ajax._send(settings, true);
    }
  };

  var getBacktrack = function(options) {
    return (options.backtrack) ? ".b" + Number(options.backtrack) : "";
  };

  var getChannelsPath = function(channels) {
    var path = '';
    for (var channelName in channels) {
      if (!channels.hasOwnProperty || channels.hasOwnProperty(channelName)) {
        path += "/" + channelName + getBacktrack(channels[channelName]);
      }
    }
    return path;
  };

  var getSubscriberUrl = function(pushstream, prefix, websocket) {
    var url = (websocket) ? ((pushstream.useSSL) ? "wss://" : "ws://") : ((pushstream.useSSL) ? "https://" : "http://");
    url += pushstream.host;
    url += ((pushstream.port != 80) && (pushstream.port != 443)) ? (":" + pushstream.port) : "";
    url += prefix;
    url += getChannelsPath(pushstream.channels);
    url += "?_=" + (new Date()).getTime();
    return url;
  };

  var getPublisherUrl = function(pushstream) {
    var channel = "";
    var url = (pushstream.useSSL) ? "https://" : "http://";
    url += pushstream.host;
    url += ((pushstream.port != 80) && (pushstream.port != 443)) ? (":" + pushstream.port) : "";
    url += pushstream.urlPrefixPublisher;
    for (var channelName in pushstream.channels) {
      if (!pushstream.channels.hasOwnProperty || pushstream.channels.hasOwnProperty(channelName)) {
        channel = channelName;
        break;
      }
    }
    url += "?id=" + channel;
    return url;
  };

  var extract_xss_domain = function(domain) {
    // if domain is a ip address return it, else return the last two parts of it
    return (domain.match(/^(\d{1,3}\.){3}\d{1,3}$/)) ? domain : domain.split('.').slice(-2).join('.');
  };

  var linker = function(method, instance) {
    return function() {
      return method.apply(instance, arguments);
    };
  };

  var clearTimer = function(timer) {
    if (timer) {
      clearTimeout(timer);
    }
    return null;
  };

  /* common callbacks */
  var onmessageCallback = function(event) {
    Log4js.info("[" + this.type + "] message received", arguments);
    var match = event.data.match((event.data.indexOf('"eventid":"') > 0) ? PATTERN_MESSAGE_WITH_EVENT_ID : PATTERN_MESSAGE);
    this.pushstream._onmessage(match[3], match[1], match[2], match[4]);
  };

  var onopenCallback = function() {
    this.pushstream._onopen();
    Log4js.info("[" + this.type + "] connection opened");
  };

  var onerrorCallback = function(event) {
    Log4js.info("[" + this.type + "] error (disconnected by server):", event);
    this._closeCurrentConnection();
    this.pushstream._onerror({type: ((event && (event.type === "load")) || (this.pushstream.readyState === PushStream.CONNECTING)) ? "load" : "timeout"});
  }

  /* wrappers */

  var WebSocketWrapper = function(pushstream) {
    if (!window.WebSocket && !window.MozWebSocket) throw "WebSocket not supported";
    this.type = WebSocketWrapper.TYPE;
    this.pushstream = pushstream;
    this.connection = null;
  };

  WebSocketWrapper.TYPE = "WebSocket";

  WebSocketWrapper.prototype = {
    connect: function() {
      this._closeCurrentConnection();
      var url = getSubscriberUrl(this.pushstream, this.pushstream.urlPrefixWebsocket, true);
      this.connection = (window.WebSocket) ? new window.WebSocket(url) : new window.MozWebSocket(url);
      this.connection.onerror   = linker(onerrorCallback, this);
      this.connection.onclose   = linker(onerrorCallback, this);
      this.connection.onopen    = linker(onopenCallback, this);
      this.connection.onmessage = linker(onmessageCallback, this);
      Log4js.info("[WebSocket] connecting to:", url);
    },

    disconnect: function() {
      if (this.connection) {
        Log4js.debug("[WebSocket] closing connection to:", this.connection.URL);
        this._closeCurrentConnection();
        this.pushstream._onclose();
      }
    },

    _closeCurrentConnection: function() {
      if (this.connection) {
        try { this.connection.close(); } catch (e) { /* ignore error on closing */ }
        this.connection = null;
      }
    },

    sendMessage: function(message) {
      if (this.connection) { this.connection.send(message); }
    }
  };

  var EventSourceWrapper = function(pushstream) {
    if (!window.EventSource) throw "EventSource not supported";
    this.type = EventSourceWrapper.TYPE;
    this.pushstream = pushstream;
    this.connection = null;
  };

  EventSourceWrapper.TYPE = "EventSource";

  EventSourceWrapper.prototype = {
    connect: function() {
      this._closeCurrentConnection();
      var url = getSubscriberUrl(this.pushstream, this.pushstream.urlPrefixEventsource);
      this.connection = new window.EventSource(url);
      this.connection.onerror   = linker(onerrorCallback, this);
      this.connection.onopen    = linker(onopenCallback, this);
      this.connection.onmessage = linker(onmessageCallback, this);
      Log4js.info("[EventSource] connecting to:", url);
    },

    disconnect: function() {
      if (this.connection) {
        Log4js.debug("[EventSource] closing connection to:", this.connection.URL);
        this._closeCurrentConnection();
        this.pushstream._onclose();
      }
    },

    _closeCurrentConnection: function() {
      if (this.connection) {
        try { this.connection.close(); } catch (e) { /* ignore error on closing */ }
        this.connection = null;
      }
    }
  };

  var StreamWrapper = function(pushstream) {
    this.type = StreamWrapper.TYPE;
    this.pushstream = pushstream;
    this.connection = null;
    this.url = null;
    this.frameloadtimer = null;
    this.pingtimer = null;
  };

  StreamWrapper.TYPE = "Stream";

  StreamWrapper.prototype = {
    connect: function() {
      this._closeCurrentConnection();
      var domain = extract_xss_domain(this.pushstream.host);
      try {
        document.domain = domain;
      } catch(e) {
        Log4js.error("[Stream] (warning) problem setting document.domain = " + domain + " (OBS: IE8 does not support set IP numbers as domain)");
      }
      this.url = getSubscriberUrl(this.pushstream, this.pushstream.urlPrefixStream) + "&streamid=" + this.pushstream.id;
      Log4js.debug("[Stream] connecting to:", this.url);
      this.loadFrame(this.url);
    },

    disconnect: function() {
      if (this.connection) {
        Log4js.debug("[Stream] closing connection to:", this.url);
        this._closeCurrentConnection();
        this.pushstream._onclose();
      }
    },

    _closeCurrentConnection: function() {
      if (this.connection) {
        try { this.connection.onload = null; this.connection.setAttribute("src", ""); } catch (e) { /* ignore error on closing */ }
        this.pingtimer = clearTimer(this.pingtimer);
        this.frameloadtimer = clearTimer(this.frameloadtimer);
        this.connection = null;
        this.transferDoc = null;
        if (typeof window.CollectGarbage === 'function') window.CollectGarbage();
      }
    },

    loadFrame: function(url) {
      try {
        var transferDoc = new window.ActiveXObject("htmlfile");
        transferDoc.open();
        transferDoc.write("<html><script>document.domain=\""+(document.domain)+"\";</script></html>");
        transferDoc.parentWindow.PushStream = PushStream;
        transferDoc.close();
        var ifrDiv = transferDoc.createElement("div");
        transferDoc.appendChild(ifrDiv);
        ifrDiv.innerHTML = "<iframe src=\""+url+"\"></iframe>";
        this.connection = ifrDiv.getElementsByTagName("IFRAME")[0];
        this.connection.onload = linker(onerrorCallback, this);
        this.transferDoc = transferDoc;
      } catch (e) {
        var ifr = document.createElement("IFRAME");
        ifr.style.width = "1px";
        ifr.style.height = "1px";
        ifr.style.border = "none";
        ifr.style.position = "absolute";
        ifr.style.top = "-10px";
        ifr.style.marginTop = "-10px";
        ifr.style.zIndex = "-20";
        ifr.PushStream = PushStream;
        document.body.appendChild(ifr);
        ifr.setAttribute("src", url);
        ifr.onload = linker(onerrorCallback, this);
        this.connection = ifr;
      }
      this.frameloadtimer = setTimeout(linker(onerrorCallback, this), this.pushstream.timeout);
    },

    register: function(iframeWindow) {
      this.frameloadtimer = clearTimer(this.frameloadtimer);
      iframeWindow.p = linker(this.process, this);
      this.connection.onload =  linker(this._onframeloaded, this);
      this.pushstream._onopen();
      this.setPingTimer();
      Log4js.info("[Stream] frame registered");
    },

    process: function(id, channel, data, eventid) {
      this.pingtimer = clearTimer(this.pingtimer);
      Log4js.info("[Stream] message received", arguments);
      this.pushstream._onmessage(data, id, channel, eventid);
      this.setPingTimer();
    },

    _onframeloaded: function() {
      Log4js.info("[Stream] frame loaded (disconnected by server)");
      this.connection.onload = null;
      this.disconnect();
    },

    setPingTimer: function() {
      if (this.pingtimer) clearTimer(this.pingtimer);
      this.pingtimer = setTimeout(linker(onerrorCallback, this), this.pushstream.pingtimeout);
    }
  };

  var LongPollingWrapper = function(pushstream) {
    this.type = LongPollingWrapper.TYPE;
    this.pushstream = pushstream;
    this.connection = null;
    this.lastModified = null;
    this.etag = 0;
    this.connectionEnabled = false;
    this.opentimer = null;
    this.messagesQueue = [];
    this.xhrSettings = {
        url: null,
        success: linker(this.onmessage, this),
        error: linker(this.onerror, this),
        beforeSend: linker(this.beforeSend, this),
        afterReceive: linker(this.afterReceive, this)
    }
  };

  LongPollingWrapper.TYPE = "LongPolling";

  LongPollingWrapper.prototype = {
    connect: function() {
      this.messagesQueue = [];
      this._closeCurrentConnection();
      this.connectionEnabled = true;
      this.xhrSettings.url = getSubscriberUrl(this.pushstream, this.pushstream.urlPrefixLongpolling);
      this._listen();
      this.opentimer = setTimeout(linker(onopenCallback, this), 5000);
      Log4js.info("[LongPolling] connecting to:", this.xhrSettings.url);
    },

    _listen: function() {
      if (this.connectionEnabled && !this.connection) {
        this.connection = Ajax.load(this.xhrSettings);
      }
    },

    disconnect: function() {
      this.connectionEnabled = false;
      if (this.connection) {
        Log4js.debug("[LongPolling] closing connection to:", this.xhrSettings.url);
        this._closeCurrentConnection();
        this.pushstream._onclose();
      }
    },

    _closeCurrentConnection: function() {
      this.opentimer = clearTimer(this.opentimer);
      if (this.connection) {
        try { this.connection.abort(); } catch (e) { /* ignore error on closing */ }
        this.connection = null;
        this.lastModified = null;
        this.xhrSettings.url = null;
      }
    },

    beforeSend: function(xhr) {
      if (this.lastModified == null) {
        var date = new Date();
        if (this.pushstream.secondsAgo) { date.setTime(date.getTime() - (this.pushstream.secondsAgo * 1000)); }
        this.lastModified = date.toUTCString();
      }
      xhr.setRequestHeader("If-None-Match", this.etag);
      xhr.setRequestHeader("If-Modified-Since", this.lastModified);
    },

    afterReceive: function(xhr) {
      this.etag = xhr.getResponseHeader('Etag');
      this.lastModified = xhr.getResponseHeader('Last-Modified');
      this.connection = null;
    },

    onerror: function(status) {
      if (this.connectionEnabled) { /* abort(), called by disconnect(), call this callback, but should be ignored */
        if (status === 304) {
          this._listen();
        } else {
          Log4js.info("[LongPolling] error (disconnected by server):", status);
          this._closeCurrentConnection();
          this.pushstream._onerror({type: (status === 403) ? "load" : "timeout"});
        }
      }
    },

    onmessage: function(responseText) {
      Log4js.info("[LongPolling] message received", responseText);
      var messages = responseText.split("\r\n");
      for (var i = 0; i < messages.length; i++) {
        if (messages[i]) {
          this.messagesQueue.push(messages[i]);
        }
      }

      this._listen();

      while (this.messagesQueue.length > 0) {
        var message = this.messagesQueue.shift();
        var match = message.match((message.indexOf('"eventid":"') > 0) ? PATTERN_MESSAGE_WITH_EVENT_ID : PATTERN_MESSAGE);
        this.pushstream._onmessage(match[3], match[1], match[2], match[4]);
      }
    }
  };

  /* mains class */

  var PushStreamManager = [];

  var PushStream = function(settings) {
    settings = settings || {};

    this.id = PushStreamManager.push(this) - 1;

    this.useSSL = settings.useSSL || false;
    this.host = settings.host || window.location.hostname;
    this.port = settings.port || (this.useSSL ? 443 : 80);

    this.timeout = settings.timeout || 15000;
    this.pingtimeout = settings.pingtimeout || 30000;
    this.reconnecttimeout = settings.reconnecttimeout || 3000;
    this.checkChannelAvailabilityInterval = settings.checkChannelAvailabilityInterval || 60000;

    this.secondsAgo = Number(settings.secondsAgo);

    this.reconnecttimer = null;

    this.urlPrefixPublisher   = settings.urlPrefixPublisher   || '/pub';
    this.urlPrefixStream      = settings.urlPrefixStream      || '/sub';
    this.urlPrefixEventsource = settings.urlPrefixEventsource || '/ev';
    this.urlPrefixLongpolling = settings.urlPrefixLongpolling || '/lp';
    this.urlPrefixWebsocket   = settings.urlPrefixWebsocket   || '/ws';

    this.modes = (settings.modes || 'eventsource|websocket|stream|longpolling').split('|');
    this.wrappers = [];

    for ( var i = 0; i < this.modes.length; i++) {
      try {
        var wrapper = null;
        switch (this.modes[i]) {
        case "websocket"  : wrapper = new WebSocketWrapper(this);   break;
        case "eventsource": wrapper = new EventSourceWrapper(this); break;
        case "longpolling": wrapper = new LongPollingWrapper(this); break;
        case "stream"     : wrapper = new StreamWrapper(this);      break;
        }
        this.wrappers[this.wrappers.length] = wrapper;
      } catch(e) { Log4js.info(e); }
    }

    this.wrapper = null; //TODO test

    this.onopen = null;
    this.onmessage = null;
    this.onerror = null;
    this.onstatuschange = null;

    this.channels = {};
    this.channelsCount = 0;

    this._setState(0);
  }

  /* constants */
  PushStream.LOG_LEVEL = 'error'; /* debug, info, error */
  PushStream.LOG_OUTPUT_ELEMENT_ID = 'Log4jsLogOutput';

  /* status codes */
  PushStream.CLOSED        = 0; //TODO test
  PushStream.CONNECTING    = 1;
  PushStream.OPEN          = 2; //TODO test

  /* main code */
  PushStream.prototype = {
    addChannel: function(channel, options) {
      Log4js.debug("entering addChannel");
      if (typeof(this.channels[channel]) !== "undefined") throw "Cannot add channel " + channel + ": already subscribed";
      options = options || {};
      Log4js.info("adding channel", channel, options);
      this.channels[channel] = options;
      this.channelsCount++;
      if (this.readyState != PushStream.CLOSED) this.connect();
      Log4js.debug("leaving addChannel");
    },

    removeChannel: function(channel) {
      if (this.channels[channel]) {
        Log4js.info("removing channel", channel);
        delete this.channels[channel];
        this.channelsCount--;
      }
    },

    removeAllChannels: function() {
      Log4js.info("removing all channels");
      this.channels = {};
      this.channelsCount = 0;
    },

    _setState: function(state) { //TODO test
      if (this.readyState != state) {
        Log4js.info("status changed", state);
        this.readyState = state;
        if (this.onstatuschange) {
          this.onstatuschange(this.readyState);
        }
      }
    },

    connect: function() { //TODO test
      Log4js.debug("entering connect");
      if (!this.host)                 throw "PushStream host not specified";
      if (isNaN(this.port))           throw "PushStream port not specified";
      if (!this.channelsCount)        throw "No channels specified";
      if (this.wrappers.length === 0) throw "No available support for this browser";

      this._keepConnected = true;
      this._lastUsedMode = 0;
      this._connect();

      Log4js.debug("leaving connect");
    },

    disconnect: function() { //TODO test
      Log4js.debug("entering disconnect");
      this._keepConnected = false;
      this._disconnect();
      this._setState(PushStream.CLOSED);
      Log4js.info("disconnected");
      Log4js.debug("leaving disconnect");
    },

    _connect: function() { //TODO test
      this._disconnect();
      this._setState(PushStream.CONNECTING);
      this.wrapper = this.wrappers[this._lastUsedMode++ % this.wrappers.length];

      try {
        this.wrapper.connect();
      } catch (e) {
        //each wrapper has a cleanup routine at disconnect method
        this.wrapper.disconnect();
      }
    },

    _disconnect: function() {
      this.reconnecttimer = clearTimer(this.reconnecttimer);
      if (this.wrapper) {
        this.wrapper.disconnect();
      }
    },

    _onopen: function() {
      this.reconnecttimer = clearTimer(this.reconnecttimer);
      this._setState(PushStream.OPEN);
      this._lastUsedMode--; //use same mode on next connection
    },

    _onclose: function() {
      this.reconnecttimer = clearTimer(this.reconnecttimer);
      this._setState(PushStream.CLOSED);
      this._reconnect(this.reconnecttimeout);
    },

    _onmessage: function(data, id, channel, eventid) {
      Log4js.debug("message", data, id, channel, eventid);
      if (id == -2) {
        if (this.onchanneldeleted) { this.onchanneldeleted(channel); }
      } else if ((id > 0) && (typeof(this.channels[channel]) !== "undefined")) {
        if (this.onmessage) { this.onmessage(data, id, channel, eventid); }
      }
    },

    _onerror: function(error) {
      this._setState(PushStream.CLOSED);
      this._reconnect((error.type == "timeout") ? this.reconnecttimeout : this.checkChannelAvailabilityInterval);
      if (this.onerror) { this.onerror(error); }
    },

    _reconnect: function(timeout) {
      if (this._keepConnected && !this.reconnecttimer && (this.readyState != PushStream.CONNECTING)) {
        Log4js.info("trying to reconnect in", timeout);
        this.reconnecttimer = setTimeout(linker(this._connect, this), timeout);
      }
    },

    sendMessage: function(message, successCallback, errorCallback) {
      if (this.wrapper.type === WebSocketWrapper.TYPE) {
        this.wrapper.sendMessage(message);
        if (successCallback) successCallback();
      } else {
        Ajax.post({url: getPublisherUrl(this), data: message, success: successCallback, error: errorCallback});
      }
    }
  };

  PushStream.sendMessage = function(url, message, successCallback, errorCallback) {
    Ajax.post({url: url, data: message, success: successCallback, error: errorCallback});
  };

  // to make server header template more clear, it calls register and
  // by a url parameter we find the stream wrapper instance
  PushStream.register = function(iframe) {
    var matcher = iframe.window.location.href.match(/streamid=(.*)&?$/);
    if (matcher[1] && PushStreamManager[matcher[1]]) {
      PushStreamManager[matcher[1]].wrapper.register(iframe);
    }
  };

  /* make class public */
  window.PushStream = PushStream;
  window.PushStreamManager = PushStreamManager;

})(window);
