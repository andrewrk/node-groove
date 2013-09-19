var groove = require('../');
var assert = require('assert');
var path = require('path');
var fs = require('fs');
var ncp = require('ncp').ncp;
var testOgg = path.join(__dirname, "danse.ogg");
var bogusFile = __filename;
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
    file.close(function(err) {
        if (err) throw err;
        copyRwTestOgg();
    });
});

groove.open(bogusFile, function(err, file) {
    assert.strictEqual(err.message, "open file failed");
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
    file.save(function(err) {
        if (err) throw err;
        file.close(function(err) {
            if (err) throw err;
            cleanup();
        });
    });
}

function cleanup() {
    fs.unlinkSync(rwTestOgg);
    console.log("all tests passed. (one test tried to open that bogus file)");
}
