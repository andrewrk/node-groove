var bindings = require('bindings')('groove.node');
var EventEmitter = require('events').EventEmitter;
var util = require('util');

/* "C++ modules aren't really for doing complex things that need to be
 * strewn across multiple modules.  Just get your binding done as quick
 * as possible, get out of there, and then wrap it in JS for all the fancy stuff
 *
 * -isaacs
 */

// hi-jack some of the native methods
var bindingsCreatePlayer = bindings.createPlayer;
var bindingsCreateReplayGainScan = bindings.createReplayGainScan;

bindings.createPlayer = jsCreatePlayer;
bindings.createReplayGainScan = jsCreateReplayGainScan;

module.exports = bindings;

function jsCreatePlayer(cb) {
  var player = bindingsCreatePlayer(eventCb);

  postHocInherit(player, EventEmitter);
  EventEmitter.call(player);

  return player;

  function eventCb(id) {
    switch (id) {
    case bindings._EVENT_NOWPLAYING:
      player.emit('nowplaying');
      break;
    case bindings._EVENT_BUFFERUNDERRUN:
      player.emit('bufferunderrun');
      break;
    }
  }
}

function jsCreateReplayGainScan(fileList, progressInterval) {
  var scan = bindingsCreateReplayGainScan(fileList, progressInterval,
          fileProgressCb, fileCompleteCb, endCb);
  postHocInherit(scan, EventEmitter);
  EventEmitter.call(scan);

  var expectedCbCount = fileList.length + 1;
  var cbCount = 0;
  var endGain, endPeak;

  return scan;

  function fileProgressCb(file, progress) {
    scan.emit('progress', file, progress);
  }

  function fileCompleteCb(file, gain, peak) {
    scan.emit('file', file, gain, peak);
    checkCbCount();
  }

  function endCb(err, gain, peak) {
    if (err) {
      scan.emit('error', err);
      return;
    }
    endGain = gain;
    endPeak = peak;
    checkCbCount();
  }

  function checkCbCount() {
    cbCount += 1;
    if (cbCount === expectedCbCount) {
      scan.emit('end', endGain, endPeak);
    }
  }
}

function postHocInherit(baseInstance, Super) {
  var baseProto = Object.getPrototypeOf(baseInstance);
  var superProto = Super.prototype;
  Object.keys(superProto).forEach(function(method) {
    if (!baseProto[method]) baseProto[method] = superProto[method];
  });
}
