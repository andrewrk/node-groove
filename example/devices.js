/* list available output devices */

var groove = require('../');
var assert = require('assert');

groove.connectSoundBackend();
var devices = groove.getDevices();

console.log(devices);
