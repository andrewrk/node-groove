/* play several files in a row and then exit */

var groove = require('../');
var Pend = require('pend');

if (process.argv.length < 3) usage();

var playlist = groove.createPlaylist();
var player = groove.createPlayer();

groove.connectSoundBackend();
var devices = groove.getDevices();
var defaultDevice = devices.list[devices.defaultIndex];
player.device = defaultDevice;

player.on('nowplaying', function() {
  var current = player.position();
  if (!current.item) {
    cleanup();
    return;
  }
  var artist = current.item.file.getMetadata('artist');
  var title = current.item.file.getMetadata('title');
  console.log("Now playing:", artist, "-", title);
});

var files = [];

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
    playlist.insert(o.file);
  });
  player.attach(playlist, function(err) {
    if (err) throw err;
  });
});

function openFileFn(o, filename) {
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
    player.detach(function(err) {
      if (err) throw err;
      playlist.destroy();
    });
  });
}

function usage() {
  console.error("Usage: playlist file1 file2 ...");
  process.exit(1);
}
