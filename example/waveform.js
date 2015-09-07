/* calculate a waveformjs compatible representation of a media file */

var groove = require('../');

main();

function main() {
  var inputFilename = null;

  for (var i = 2; i < process.argv.length; i += 1) {
    var arg = process.argv[i];
    var overrideDuration = null;
    if (arg[0] === "-" && arg[1] === "-") {
      if (++i < process.argv.length) {
        if (arg === "--override-duration") {
          overrideDuration = parseFloat(process.argv[i]);
        } else {
          usageAndExit();
        }
      } else {
        usageAndExit();
      }
    } else if (!inputFilename) {
      inputFilename = arg;
    } else {
      usageAndExit();
    }
  }

  if (!inputFilename) usageAndExit();

  var playlist = groove.createPlaylist();
  var waveform = groove.createWaveformBuilder();
  var file = null;

  waveform.on('info', function() {
    var info = waveform.getInfo();
    if (!info.item) {
      cleanup();
      return;
    }
    if (Math.abs(info.expectedDuration - info.actualDuration) > 0.1) {
      console.error("invalid duration. re-run with --override-duration " + info.actualDuration);
      process.exit(1);
      return;
    }
    var data = {
      duration: info.actualDuration,
      waveformjs: Array.prototype.slice.call(info.buffer, 0),
    };
    console.log(JSON.stringify(data));
  });

  groove.open(inputFilename, function(err, openedFile) {
    if (err) throw err;

    file = openedFile;
    if (overrideDuration) {
      file.overrideDuration(overrideDuration);
    }

    playlist.insert(file, null);
    waveform.attach(playlist, function(err) {
      if (err) throw err;
    });
  });

  function cleanup() {
    playlist.clear();
    waveform.detach(function(err) {
      if (err) throw err;
      playlist.destroy();
      file.close(function(err) {
        if (err) throw err;
      });
    });
  }
}

function usageAndExit() {
  console.error("Usage: node waveform.js [--override-duration seconds] file");
  process.exit(1);
}

