beforeEach(function() {
  this.addMatchers({
  });
});


(function() {
  var D = new Date('2011-06-02T09:34:29+02:00');
  if (!D || +D !== 1307000069000) {
    Date.fromISO = function(s) {
      var day, tz, rx = /^(\d{4}\-\d\d\-\d\d([tT ][\d:\.]*)?)([zZ]|([+\-])(\d\d):(\d\d))?$/, p = rx.exec(s) || [];
      if (p[1]) {
        day = p[1].split(/\D/);
        for (var i = 0, L = day.length; i < L; i++) {
          day[i] = parseInt(day[i], 10) || 0;
        };
        day[1] -= 1;
        day = new Date(Date.UTC.apply(Date, day));
        if (!day.getDate())
          return NaN;
        if (p[5]) {
          tz = (parseInt(p[5], 10) * 60);
          if (p[6])
            tz += parseInt(p[6], 10);
          if (p[4] == '+')
            tz *= -1;
          if (tz)
            day.setUTCMinutes(day.getUTCMinutes() + tz);
        }
        return day;
      }
      return NaN;
    };
  } else {
    Date.fromISO = function(s) {
      return new Date(s);
    };
  }
})();
