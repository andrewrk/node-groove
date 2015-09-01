/* transcode a file into ogg vorbis */

var groove = require('../');
var assert = require('assert');
var fs = require('fs');

if (process.argv.length < 4) usage();

groove.setLogging(groove.LOG_INFO);

var playlist = groove.createPlaylist();
var encoder = groove.createEncoder();
encoder.formatShortName = "ogg";
encoder.codecShortName = "vorbis";

var outStream = fs.createWriteStream(process.argv[3]);

encoder.on('buffer', function() {
  var buffer;
  while (buffer = encoder.getBuffer()) {
    if (buffer.buffer) {
      outStream.write(buffer.buffer);
    } else {
      cleanup();
      return;
    }
  }
});

encoder.attach(playlist, function(err) {
  assert.ifError(err);

  groove.open(process.argv[2], function(err, file) {
    assert.ifError(err);
    playlist.insert(file, null);
  });
});

function cleanup() {
  var file = playlist.items()[0].file;
  playlist.clear();
  file.close(function(err) {
    assert.ifError(err);
    encoder.detach(function(err) {
      assert.ifError(err);
      playlist.destroy();
    });
  });
}

function usage() {
  console.error("Usage: node transcode.js inputfile outputfile");
  process.exit(1);
}
