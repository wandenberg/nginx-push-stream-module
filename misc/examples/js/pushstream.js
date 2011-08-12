PushStream = {
	callbacks: {
		process: function() {},
		reset: function() {},
		eof: function() {},
		statuschanged: function() {}
	},
	host: null,
	port: 80,
	hostid: null,
	status: 0,
	channelcount: 0,
	channels: {},
	lastrequest: 0,
	frameref: null,
	frameloadtimer: null,
	frameloadingtimeout: 15000,
	pingtimer: null,
	pingingtimeout: 30000,
	reconnecttimer: null,
	reconnecttimeout: 3000,
	checkChannelAvailabilityInterval: 60000,
	backtrackDefault: 10,
	mode: 'iframe',

	connect: function() {
		Log4js.debug('entering connect');
		if (!PushStream.host) throw "PushStream host not specified";
		if (isNaN(PushStream.port)) throw "PushStream port not specified";
		if (!PushStream.channelcount) throw "No channels specified";
		if (!PushStream.hostid) PushStream.hostid = t+""+Math.floor(Math.random()*1000000);
		document.domain = PushStream.extract_xss_domain(document.domain);

		if (PushStream.status) PushStream.disconnect();
		PushStream.setStatus(1);
		var now = new Date();
		var t = now.getTime();
		PushStream.loadFrame(PushStream.getSubsUrl());
		PushStream.lastrequest = t;
		Log4js.debug('leaving connect');
	},

	reconnect: function(interval) {
		if (PushStream.status != 6) {
			PushStream.reconnecttimer = setTimeout(PushStream.connect, interval || PushStream.reconnecttimeout);
		}
	},

	disconnect: function() {
		Log4js.debug('entering disconnect');
		if (PushStream.status) {
			PushStream.clearPingtimer();
			PushStream.clearFrameloadtimer();
			PushStream.clearReconnecttimer();
			if (typeof CollectGarbage == 'function') CollectGarbage();
			if (PushStream.status != 6) PushStream.setStatus(0);
			Log4js.info("Disconnected");
		}
		Log4js.debug('leaving disconnect');
	},

	joinChannel: function(channelname, backtrack) {
		Log4js.debug('entering joinChannel');
		if (typeof(PushStream.channels[channelname]) != "undefined") throw "Cannot join channel "+channelname+": already subscribed";
		PushStream.channels[channelname] = {backtrack:backtrack, lastmsgreceived:-1};
		Log4js.info("Joined channel " + channelname);
		PushStream.channelcount++;
		if (PushStream.status != 0) PushStream.connect();
		Log4js.debug('leaving joinChannel');
	},

	loadFrame: function(url) {
		try {
			var transferDoc = (!PushStream.frameref) ? new ActiveXObject("htmlfile") : PushStream.frameref;
			transferDoc.open();
			transferDoc.write("<html><script>document.domain=\""+(document.domain)+"\";</script></html>");
			transferDoc.parentWindow.PushStream = PushStream;
			transferDoc.close();
			var ifrDiv = transferDoc.createElement("div");
			transferDoc.appendChild(ifrDiv);
			ifrDiv.innerHTML = "<iframe src=\""+url+"\" onload=\"PushStream.frameload();\"></iframe>";
			PushStream.frameref = transferDoc;
		} catch (e) {
			if (!PushStream.frameref) {
				var ifr = document.createElement("IFRAME");
				ifr.style.width = "10px";
				ifr.style.height = "10px";
				ifr.style.border = "none";
				ifr.style.position = "absolute";
				ifr.style.top = "-10px";
				ifr.style.marginTop = "-10px";
				ifr.style.zIndex = "-20";
				ifr.PushStream = PushStream;
				ifr.onload = PushStream.frameload;
				document.body.appendChild(ifr);
				PushStream.frameref = ifr;
			}
			PushStream.frameref.setAttribute("src", url);
		}
		Log4js.info("Loading URL '" + url + "' into frame...");
		PushStream.frameloadtimer = setTimeout(PushStream.frameloadtimeout, PushStream.frameloadingtimeout);
	},

	frameload: function() {
		Log4js.info("Frame loaded whitout streaming");
		PushStream.clearFrameloadtimer();
		PushStream.setStatus(8);
		PushStream.reconnect(PushStream.checkChannelAvailabilityInterval);
	},

	frameloadtimeout: function() {
		Log4js.info("Frame load timeout");
		PushStream.clearFrameloadtimer();
		PushStream.setStatus(3);
		PushStream.reconnect(PushStream.frameloadingtimeout);
	},

	register: function(ifr) {
		PushStream.clearFrameloadtimer();
		ifr.p = PushStream.process;
		ifr.r = PushStream.reset;
		ifr.eof = PushStream.eof;
		PushStream.setStatus(4);
		PushStream.setPingtimer();
		Log4js.info("Frame registered");
	},

	pingtimeout: function() {
		Log4js.info("Ping timeout");
		PushStream.setStatus(7);
		PushStream.clearPingtimer();
		PushStream.reconnect();
	},

	process: function(id, channel, data) {
		Log4js.info("Message received");
		PushStream.setStatus(5);
		PushStream.clearPingtimer();
		if (id == -1) {
			Log4js.debug("Ping");
		} else if (typeof(PushStream.channels[channel]) != "undefined") {
			Log4js.debug("Message " + id + " received on channel " + channel + " (last id on channel: " + PushStream.channels[channel].lastmsgreceived + ")\n" + data);
			PushStream.callbacks["process"](data);
			PushStream.channels[channel].lastmsgreceived = id;
		}
		PushStream.setPingtimer();
	},

	reset: function() {
		if (PushStream.status != 6) {
			Log4js.info("Stream reset");
			PushStream.callbacks["reset"]();
			PushStream.reconnect();
		}
	},

	eof: function() {
		Log4js.info("Received end of stream, will not reconnect");
		PushStream.callbacks["eof"]();
		PushStream.setStatus(6);
		PushStream.disconnect();
	},

	setStatus: function(newstatus) {
		// Statuses:	0 = Uninitialised,
		//				1 = Loading stream,
		//				2 = Loading controller frame,
		//				3 = Controller frame timeout, retrying.
		//				4 = Controller frame loaded and ready
		//				5 = Receiving data
		//				6 = End of stream, will not reconnect
		//				7 = Ping Timeout
		//				8 = Frame loaded whitout streaming, channel problably empty or not exists

		if (PushStream.status != newstatus) {
			Log4js.info('PushStream.status ' + newstatus);
			PushStream.status = newstatus;
			PushStream.callbacks["statuschanged"](newstatus);
		}
	},

	registerEventCallback: function(evt, funcRef) {
		Function.prototype.andThen=function(g) {
			var f=this;
			var a=PushStream.arguments
			return function(args) {
				f(a);g(args);
			}
		};
		if (typeof PushStream.callbacks[evt] == "function") {
			PushStream.callbacks[evt] = (PushStream.callbacks[evt]).andThen(funcRef);
		} else {
			PushStream.callbacks[evt] = funcRef;
		}
	},

	extract_xss_domain: function(old_domain) {
		if (old_domain.match(/^(\d{1,3}\.){3}\d{1,3}$/)) return old_domain;
		domain_pieces = old_domain.split('.');
		return domain_pieces.slice(-2, domain_pieces.length).join(".");
	},

	getSubsUrl: function() {
		var surl = "http://" + PushStream.host + ((PushStream.port==80)?"":":"+PushStream.port) + "/sub";
		for (var c in PushStream.channels) {
			var channelinfo = "/" + c + PushStream.getBacktrack(c);
			surl += channelinfo;
		}
		var now = new Date();
		surl += "?nc="+now.getTime();
		return surl;
	},

	getBacktrack: function(channelName) {
		var channel = PushStream.channels[channelName];

		if (channel.backtrack != 0) {
			var backtrack = ".b"

			if (channel.backtrack > 0) {
				backtrack += channel.backtrack
			} else {
				backtrack += PushStream.backtrackDefault;
			}

			return backtrack;
		} else return "";
	},

	clearPingtimer: function() {
		if (PushStream.pingtimer) {
			clearTimeout(PushStream.pingtimer);
			PushStream.pingtimer = null;
		}
	},

	setPingtimer: function() {
		PushStream.clearPingtimer();
		PushStream.pingtimer = setTimeout(PushStream.pingtimeout, PushStream.pingingtimeout);
	},

	clearFrameloadtimer: function() {
		if (PushStream.frameloadtimer) {
			if (PushStream.frameloadtimer) clearTimeout(PushStream.frameloadtimer);
			PushStream.frameloadtimer = null;
		}
	},

	clearReconnecttimer: function() {
		if (PushStream.reconnecttimer) {
			if (PushStream.reconnecttimer) clearTimeout(PushStream.reconnecttimer);
			PushStream.reconnecttimer = null;
		}
	}
};
