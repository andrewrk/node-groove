/* play several files in a row and then exit */

var groove = require('../');
var assert = require('assert');
var Batch = require('batch'); // npm install batch

if (process.argv.length < 3) usage();

var playlist = groove.createPlaylist();
var player = groove.createPlayer();

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

var batch = new Batch();
for (var i = 2; i < process.argv.length; i += 1) {
  batch.push(openFileFn(process.argv[i]));
}
batch.end(function(err, files) {
  files.forEach(function(file) {
    if (file) {
      playlist.insert(file);
    }
  });
  player.attach(playlist, function(err) {
    assert.ifError(err);
  });
});
function openFileFn(filename) {
  return function(cb) {
    groove.open(filename, cb);
  };
}

function cleanup() {
  var batch = new Batch();
  var files = playlist.items().map(function(item) { return item.file; });
  playlist.clear();
  files.forEach(function(file) {
    batch.push(function(cb) {
      file.close(cb);
    });
  });
  batch.end(function(err) {
    player.detach(function(err) {
      if (err) console.error(err.stack);
    });
  });
}

function usage() {
  console.error("Usage: playlist file1 file2 ...");
  process.exit(1);
}
