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

bindings.createPlayer = jsCreatePlayer;

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

  player._event = function(id, obj) {
    switch (id) {
      case bindings._PLAYER_EVENT_NOWPLAYING:
        player.emit('nowplaying', obj);
        break;
      case bindings._PLAYER_EVENT_BUFFERUNDERRUN:
        player.emit('bufferunderrun', obj);
        break;
    }
  };

  return player;
}

function postHocInherit(baseInstance, Super) {
  var baseProto = Object.getPrototypeOf(baseInstance);
  var superProto = Super.prototype;
  Object.keys(superProto).forEach(function(method) {
    if (!baseProto[method]) baseProto[method] = superProto[method];
  });
}
