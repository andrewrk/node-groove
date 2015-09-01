/* list available output devices */

var groove = require('../');
var assert = require('assert');

groove.connectSoundBackend();
var devices = groove.getDevices();

for (var i = 0; i < devices.list.length; i += 1) {
    if (devices.list[i].isRaw) continue;
    var isDefault = (i === devices.defaultIndex);
    var defaultString = isDefault ? "(default) " : "";
    console.log(defaultString + devices.list[i].name);
}
