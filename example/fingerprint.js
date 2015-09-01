/* generate the acoustid fingerprint of songs */

var groove = require('../');
var Pend = require('pend');

if (process.argv.length < 3) usage();

var playlist = groove.createPlaylist();
var printer = groove.createFingerprinter();

printer.on('info', function() {
  var info = printer.getInfo();
  if (info.item) {
    console.log(info.item.file.filename, "fingerprint:");
    console.log(info.fingerprint);
  } else {
    cleanup();
  }
});

var files = [];
var pend = new Pend();

for (var i = 2; i < process.argv.length; i += 1) {
  var o = {
    file: null,
    filename: process.argv[i],
  };
  files.push(o);
  pend.go(openFileFn(o));
}
pend.wait(function(err) {
  if (err) throw err;
  files.forEach(function(o) {
    playlist.insert(o.file, null);
  });
  printer.attach(playlist, function(err) {
    if (err) throw err;
  });
});

function openFileFn(o) {
  return function(cb) {
    groove.open(o.filename, function(err, file) {
      if (err) return cb(err);
      o.file = file;
      cb();
    });
  };
}

function cleanup() {
  playlist.clear();
  files.forEach(function(o) {
    pend.go(function(cb) {
      o.file.close(cb);
    });
  });
  pend.wait(function(err) {
    if (err) throw err;
    printer.detach(function(err) {
      if (err) throw err;
      playlist.destroy();
    });
  });
}

function usage() {
  console.error("Usage: node fingerprint.js file1 file2 ...");
  process.exit(1);
}

