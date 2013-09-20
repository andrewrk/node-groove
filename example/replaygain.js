/* replaygain scanner */

var groove = require('groove');
var assert = require('assert');
var findit = require('findit'); // npm install findit
var Batch = require('batch'); // npm install pend
var fs = require('fs');
var path = require('path');


if (process.argv.length < 3) usage();

groove.createReplayGainScan(function(err, scan) {
  assert.ifError(err);
  var batch = new Batch();
  for (var i = 2; i < process.argv.length; i += 1) {
    var arg = process.argv[i];
    batch.push(addAllFn(arg));
  }
  batch.end(function(err) {
    scan.on('progress', function(progress) {
      process.stderr.write("\rmetadata " +
        progress.metadataCurrent + "/" + progress.metadataTotal +
        " scanning " + progress.scanningCurrent + "/" + progress.scanningTotal +
        " update " + progress.updateCurrent + "/" + progress.updateTotal +
        "          ");
      fs.fsync(process.stderr.fd);
    });
    scan.on('end', function() {
      scan.destroy();
      console.error("\nscan complete.");
    });
    scan.exec();
  });

  function addAllFn(dir) {
    return function(cb) {
      var finder = findit(dir);
      finder.on('file', function(file, stat) {
        scan.add(path.join(dir, file));
      });
      finder.on('end', function() {
        cb();
      });
    };
  }
});

function usage() {
  console.error("Usage:", process.argv[0], process.argv[1], "dir1 dir2 ...");
  process.exit(1);
}
