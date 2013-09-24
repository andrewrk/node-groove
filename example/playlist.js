/* play several files in a row and then exit */

var groove = require('../');
var assert = require('assert');
var Batch = require('batch'); // npm install batch

if (process.argv.length < 3) usage();

groove.createPlayer(function(err, player) {
  assert.ifError(err);

  // detect end of playlist and quit
  player.on('nowplaying', function() {
    var current = player.position();
    if (!current.item) {
      cleanup(player);
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
        player.insert(file, null);
      }
    });
  });
  function openFileFn(filename) {
    return function(cb) {
      groove.open(filename, cb);
    };
  }
});

function cleanup(player) {
  var batch = new Batch();
  var files = player.playlist().map(function(item) { return item.file; });
  player.clear();
  files.forEach(function(file) {
    batch.push(function(cb) {
      file.close(cb);
    });
  });
  batch.end(function(err) {
    player.destroy(function(err) {
      if (err) console.error(err.stack);
    });
  });
}

function usage() {
  console.error("Usage: playlist file1 file2 ...");
  process.exit(1);
}
