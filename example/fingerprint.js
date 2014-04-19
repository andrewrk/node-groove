/* generate the acoustid fingerprint of songs */

var groove = require('../');
var assert = require('assert');
var Batch = require('batch'); // npm install batch

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

printer.attach(playlist, function(err) {
  assert.ifError(err);

  var batch = new Batch();
  for (var i = 2; i < process.argv.length; i += 1) {
    batch.push(openFileFn(process.argv[i]));
  }
  batch.end(function(err, files) {
    files.forEach(function(file) {
      if (file) {
        playlist.insert(file, null);
      }
    });
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
    printer.detach(function(err) {
      if (err) console.error(err.stack);
    });
  });
}

function usage() {
  console.error("Usage: node fingerprint.js file1 file2 ...");
  process.exit(1);
}

