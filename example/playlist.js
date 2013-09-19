/* play several files in a row and then exit */

var groove = require('groove');
var assert = require('assert');
var Pend = require('pend'); // npm install pend

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
    var artist = current.item.file.getMetadata('artist')
    var title = current.item.file.getMetadata('title')
    console.log("Now playing:", artist, "-", title);
  });

  var pend = new Pend();
  for (var i = 2; i < process.argv.length; i += 1) {
    pend.go(addFileFn(process.argv[i]));
  }
  function addFileFn(filename) {
    return function(cb) {
      groove.open(filename, function(err, file) {
        if (err) console.error("unable to open", filename, err.stack);
        player.insert(file, null);
        cb();
      });
    };
  }
});

function cleanup(player) {
  var pend = new Pend();
  player.playlist.forEach(function(playlistItem) {
    pend.go(destroyPlItemFn(playlistItem));
  });
  pend.wait(function() {
    player.destroy(assert.ifError);
  });
}

function destroyPlItemFn(playlistItem) {
  return function(cb) {
    playlistItem.file.close(function(err) {
      if (err) console.error("error closing file:", err.stack);
      cb();
    });
  };
}

function usage() {
  console.error("Usage:", process.argv[0], process.argv[1], "file1 file2 ...");
  process.exit(1);
}
