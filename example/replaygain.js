/* replaygain scanner */

var groove = require('../');
var Pend = require('pend');

if (process.argv.length < 3) usage();

var playlist = groove.createPlaylist();
var detector = groove.createLoudnessDetector();

detector.on('info', function() {
  var info = detector.getInfo();
  if (info.item) {
    console.log(info.item.file.filename, "gain:",
      groove.loudnessToReplayGain(info.loudness), "peak:", info.peak,
      "duration:", info.duration);
  } else {
    console.log("all files gain:",
      groove.loudnessToReplayGain(info.loudness), "peak:", info.peak,
      "duration:", info.duration);
    cleanup();
  }
});

var files = [];
var pend = new Pend();

detector.attach(playlist, function(err) {
  if (err) throw err;

  var pend = new Pend();
  for (var i = 2; i < process.argv.length; i += 1) {
    var o = {
      filename: process.argv[i],
      file: null,
    };
    files.push(o);
    pend.go(openFileFn(o));
  }
  pend.wait(function(err) {
    if (err) throw err;
    files.forEach(function(o) {
      playlist.insert(o.file, null);
    });
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
    detector.detach(function(err) {
      if (err) throw err;
      playlist.destroy();
    });
  });
}

function usage() {
  console.error("Usage: node replaygain.js file1 file2 ...");
  process.exit(1);
}
