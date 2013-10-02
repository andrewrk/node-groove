/* replaygain scanner */

var groove = require('../');
var Batch = require('batch'); // npm install batch


var filenames = process.argv.slice(2);
if (filenames.length === 0) usage();

var batch = new Batch();

filenames.forEach(queueOpenFn);
batch.end(function(err, files) {
  if (err) {
    console.error("Error opening file:", err.stack);
    return;
  }
  var scan = groove.createReplayGainScan(files, 10);
  scan.on('error', function(err) {
    console.error("Error scanning:", err.stack);
  });
  scan.on('end', function(gain, peak) {
    console.log("all files gain:", gain, "all files peak:", peak);
  });
  scan.on('file', function(file, gain, peak) {
    console.log(file.filename, "gain:", gain, "peak:", peak);
  });
});

function queueOpenFn(filename) {
  batch.push(function(cb) {
    groove.open(filename, cb);
  });
}

function usage() {
  console.error("Usage: node replaygain.js file1 file2 ...");
  process.exit(1);
}
