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
  bindingsCreatePlayer(function(err, player) {
    if (err) return cb(err);
    cb(null, newPlayer(player));
  });
}

function newPlayer(player) {
  postHocInherit(player, EventEmitter);
  EventEmitter.call(player);

  var intervalId = setInterval(flushEvents, 10);

  var bindingsDestroy = player.destroy;
  player.destroy = jsDestroy;
  player.pollInterval = 10;

  return player;

  function jsDestroy(cb) {
    clearInterval(intervalId);
    bindingsDestroy.call(player, cb);
  }

  function flushEvents() {
    while (1) {
      var id = player._eventPoll();
      if (id < 0) break;

      switch (id) {
      case bindings._PLAYER_EVENT_NOWPLAYING:
        player.emit('nowplaying');
        break;
      case bindings._PLAYER_EVENT_BUFFERUNDERRUN:
        player.emit('bufferunderrun');
        break;
      }
    }
  }
}

function jsCreateReplayGainScan(fileList, progressInterval) {
  var scan = bindingsCreateReplayGainScan(fileList, progressInterval,
          fileProgressCb, fileCompleteCb, endCb);
  postHocInherit(scan, EventEmitter);
  EventEmitter.call(scan);

  return scan;

  function fileProgressCb(file, progress) {
    scan.emit('progress', file, progress);
  }

  function fileCompleteCb(file, gain, peak) {
     scan.emit('file', file, gain, peak);
  }

  function endCb(err, gain, peak) {
    if (err) {
      scan.emit('error', err);
    } else {
      scan.emit('end', gain, peak);
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
