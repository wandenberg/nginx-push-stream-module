Log4js = {
	logLevel: 'error', /* debug, info, error */
	logOutputId: 'Log4jsLogOutput',

	debug : function(logstr) {
		Log4js._log(logstr, 'debug');
	},

	info : function(logstr) {
		Log4js._log(logstr, 'info');
	},

	error : function(logstr) {
		Log4js._log(logstr, 'error');
	},

	_log: function(logstr, level) {
		if ((Log4js.logLevel === level) || ('error' === level) || (Log4js.logLevel === 'debug') ) {
			if (window.console) {
				window.console.log(logstr);
			} else if (document.getElementById(Log4js.logOutputId)) {
				document.getElementById(Log4js.logOutputId).innerHTML += logstr+"<br/>";
			}
		}
	}
};
