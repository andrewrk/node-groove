/* transcode a file into ogg vorbis */

var groove = require('../');
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
  if (err) throw err;

  groove.open(process.argv[2], function(err, file) {
    if (err) throw err;
    playlist.insert(file, null);
  });
});

function cleanup() {
  var file = playlist.items()[0].file;
  playlist.clear();
  file.close(function(err) {
    if (err) throw err;
    encoder.detach(function(err) {
      if (err) throw err;
      playlist.destroy();
    });
  });
}

function usage() {
  console.error("Usage: node transcode.js inputfile outputfile");
  process.exit(1);
}
