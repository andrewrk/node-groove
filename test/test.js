var groove = require('../build/Release/groove');
var assert = require('assert');
var path = require('path');
var fs = require('fs');
var ncp = require('ncp').ncp;
var testOgg = path.join(__dirname, "danse.ogg");
var rwTestOgg = path.join(__dirname, "danse-rw.ogg");

assert.strictEqual(groove.LOG_ERROR, 16);
groove.setLogging(groove.LOG_INFO);

groove.open(testOgg, function(err, file) {
    if (err) throw err;
    assert.strictEqual(file.filename, testOgg);
    assert.strictEqual(file.dirty, false);
    assert.strictEqual(file.metadata.TITLE, 'Danse Macabre');
    assert.strictEqual(file.metadata.ARTIST, 'Kevin MacLeod');
    assert.strictEqual(file.shortNames(), 'ogg');
    assert.strictEqual(file.getMetadata('initial key'), 'C');
    assert.strictEqual(file.getMetadata('bogus nonexisting tag'), null);
    file.close();
    copyRwTestOgg();
});

function copyRwTestOgg() {
    ncp(testOgg, rwTestOgg, function(err) {
        if (err) throw err;
        groove.open(rwTestOgg, performRwTests);
    });
}

function performRwTests(err, file) {
    if (err) throw err;
    file.setMetadata('foo new key', "libgroove rules!");
    assert.strictEqual(file.getMetadata('foo new key'), 'libgroove rules!');
    cleanup();
}

function cleanup() {
    fs.unlink(rwTestOgg);
}
