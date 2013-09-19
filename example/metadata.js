/* read or update metadata in a media file */

var groove = require('../');

if (process.argv.length < 3) usage();

groove.setLogging(groove.LOG_INFO);

var filename = process.argv[2];
groove.open(filename, function(err, file) {
  if (err) {
    console.error("error opening file:", err.stack);
    process.exit(1);
  }
  var key, value;
  for (var i = 3; i < process.argv.length; i += 1) {
    var arg = process.argv[i];
    if (arg === '--update') {
      if (i + 2 >= process.argv.length) {
        console.error("--update requires 2 arguments");
        cleanup(file, usage);
        return;
      }
      key = process.argv[++i];
      value = process.argv[++i];
      file.setMetadata(key, value);
    } else if (arg === '--delete') {
      if (i + 1 >= process.argv.length) {
        console.error("--delete requires 1 argument");
        cleanup(file, usage);
        return;
      }
      key = process.argv[++i];
      file.setMetadata(key, null);
    } else {
      cleanup(file, usage);
      return;
    }
  }

  console.log("duration", "=", file.duration());
  for (key in file.metadata) {
    value = file.metadata[key];
    console.log(key, "=", value);
  }
  if (file.dirty) {
    file.save(handleSaveErr);
  } else {
    cleanup(file);
  }
  function handleSaveErr(err) {
    if (err) console.error("Error saving:", err.stack);
    cleanup(file);
  }
});

function usage() {
  console.error("Usage:", process.argv[0], process.argv[1],
      "<file> [--update key value] [--delete key]");
  process.exit(1);
}

function cleanup(file, cb) {
  file.close(function(err) {
    if (err) console.error("Error closing file:", err.stack);
    if (cb) cb();
  });
}
