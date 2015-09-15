var bindings = require('bindings')('groove.node');
var EventEmitter = require('events').EventEmitter;
var util = require('util');

var DB_SCALE = Math.log(10.0) * 0.05;

/* "C++ modules aren't really for doing complex things that need to be
 * strewn across multiple modules.  Just get your binding done as quick
 * as possible, get out of there, and then wrap it in JS for all the fancy stuff
 *
 * -isaacs
 */

// hi-jack some of the native methods
var bindingsCreatePlayer = bindings.createPlayer;
var bindingsCreateLoudnessDetector = bindings.createLoudnessDetector;
var bindingsCreateFingerprinter = bindings.createFingerprinter;
var bindingsCreateEncoder = bindings.createEncoder;
var bindingsCreateWaveformBuilder = bindings.createWaveformBuilder;

bindings.createPlayer = jsCreatePlayer;
bindings.createEncoder = jsCreateEncoder;
bindings.createLoudnessDetector = jsCreateLoudnessDetector;
bindings.createFingerprinter = jsCreateFingerprinter;
bindings.createWaveformBuilder = jsCreateWaveformBuilder;
bindings.loudnessToReplayGain = loudnessToReplayGain;
bindings.dBToFloat = dBToFloat;

module.exports = bindings;

function jsCreateEncoder() {
  var encoder = bindingsCreateEncoder(eventCb);

  postHocInherit(encoder, EventEmitter);
  EventEmitter.call(encoder);

  return encoder;

  function eventCb() {
    encoder.emit('buffer');
  }
}

function jsCreatePlayer() {
  var player = bindingsCreatePlayer(eventCb);

  postHocInherit(player, EventEmitter);
  EventEmitter.call(player);

  return player;

  function eventCb(id) {
    switch (id) {
    case bindings._EVENT_NOWPLAYING:
      player.emit('nowPlaying');
      break;
    case bindings._EVENT_BUFFERUNDERRUN:
      player.emit('bufferUnderrun');
      break;
    case bindings._EVENT_DEVICE_CLOSED:
      player.emit('deviceClosed');
      break;
    case bindings._EVENT_DEVICE_OPENED:
      player.emit('deviceOpened');
      break;
    case bindings._EVENT_DEVICE_OPEN_ERROR:
      player.emit('deviceOpenError');
      break;
    case bindings._EVENT_END_OF_PLAYLIST:
      player.emit('endOfPlaylist');
      break;
    case bindings._EVENT_WAKEUP:
      player.emit('wakeup');
      break;
    }
  }
}

function jsCreateLoudnessDetector() {
  var detector = bindingsCreateLoudnessDetector(eventCb);

  postHocInherit(detector, EventEmitter);
  EventEmitter.call(detector);

  return detector;

  function eventCb() {
    detector.emit('info');
  }
}

function jsCreateFingerprinter() {
  var printer = bindingsCreateFingerprinter(eventCb);
  postHocInherit(printer, EventEmitter);
  EventEmitter.call(printer);

  return printer;

  function eventCb() {
    printer.emit('info');
  }
}

function jsCreateWaveformBuilder() {
  var waveform = bindingsCreateWaveformBuilder(eventCb);

  postHocInherit(waveform, EventEmitter);
  EventEmitter.call(waveform);

  return waveform;

  function eventCb() {
    waveform.emit('info');
  }
}

function postHocInherit(baseInstance, Super) {
  var baseProto = Object.getPrototypeOf(baseInstance);
  var superProto = Super.prototype;
  Object.keys(superProto).forEach(function(method) {
    if (!baseProto[method]) baseProto[method] = superProto[method];
  });
}

function clamp_rg(x) {
  if (x > 51.0) return 51.0;
  else if (x < -51.0) return -51.0;
  else return x;
}

function loudnessToReplayGain(loudness) {
  return clamp_rg(-18.0 - loudness);
}

function dBToFloat(dB) {
  return Math.exp(dB * DB_SCALE);
}
