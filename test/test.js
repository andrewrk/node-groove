var groove = require('../');
var assert = require('assert');
var path = require('path');
var fs = require('fs');
var ncp = require('ncp').ncp;
var testOgg = path.join(__dirname, "danse.ogg");
var bogusFile = __filename;
var rwTestOgg = path.join(__dirname, "danse-rw.ogg");
var it = global.it;

it("version", function() {
  var ver = groove.getVersion();
  assert.strictEqual(typeof ver.major, 'number');
  assert.strictEqual(typeof ver.minor, 'number');
  assert.strictEqual(typeof ver.patch, 'number');
});

it("logging", function() {
    assert.strictEqual(groove.LOG_ERROR, 16);
    groove.setLogging(groove.LOG_QUIET);
});

it("open fails for bogus file", function(done) {
    groove.open(bogusFile, function(err, file) {
        assert.strictEqual(err.message, "open file failed");
        done();
    });
});

it("open file and read metadata", function(done) {
    groove.open(testOgg, function(err, file) {
        assert.ok(!err);
        assert.ok(file.id);
        assert.strictEqual(file.filename, testOgg);
        assert.strictEqual(file.dirty, false);
        assert.strictEqual(file.metadata().TITLE, 'Danse Macabre');
        assert.strictEqual(file.metadata().ARTIST, 'Kevin MacLeod');
        assert.strictEqual(file.shortNames(), 'ogg');
        assert.strictEqual(file.getMetadata('initial key'), 'C');
        assert.equal(file.getMetadata('bogus nonexisting tag'), null);
        file.close(function(err) {
            assert.ok(!err);
            done();
        });
    });
});

it("update metadata", function(done) {
    ncp(testOgg, rwTestOgg, function(err) {
        assert.ok(!err);
        groove.open(rwTestOgg, doUpdate);
    });
    function doUpdate(err, file) {
        assert.ok(!err);
        file.setMetadata('foo new key', "libgroove rules!");
        assert.strictEqual(file.getMetadata('foo new key'), 'libgroove rules!');
        file.save(function(err) {
            if (err) throw err;
            file.close(checkUpdate);
        });
    }
    function checkUpdate(err) {
        assert.ok(!err);
        groove.open(rwTestOgg, function(err, file) {
            assert.ok(!err);
            assert.strictEqual(file.getMetadata('foo new key'), 'libgroove rules!');
            fs.unlinkSync(rwTestOgg);
            done();
        });
    }
});

it("create empty playlist", function (done) {
    var playlist = groove.createPlaylist();
    assert.ok(playlist.id);
    assert.deepEqual(playlist.items(), []);
    done();
});

it("create empty player", function (done) {
    var player = groove.createPlayer();
    assert.ok(player.id);
    assert.strictEqual(player.targetAudioFormat.sampleRate, 44100);
    done();
});

it("playlist item ids", function(done) {
    var playlist = groove.createPlaylist();
    assert.ok(playlist);
    playlist.pause();
    assert.equal(playlist.playing(), false);
    groove.open(testOgg, function(err, file) {
        assert.ok(!err, "opening file");
        assert.ok(playlist.position);
        assert.strictEqual(playlist.gain, 1.0);
        playlist.setGain(1.0);
        var returned1 = playlist.insert(file, null);
        var returned2 = playlist.insert(file, null);
        var items1 = playlist.items();
        var items2 = playlist.items();
        assert.strictEqual(items1[0].id, items2[0].id);
        assert.strictEqual(items1[0].id, returned1.id);
        assert.strictEqual(items2[1].id, returned2.id);
        done();
    });
});

it("create, attach, detach player", function(done) {
    var playlist = groove.createPlaylist();
    var player = groove.createPlayer();
    player.deviceIndex = groove.DUMMY_DEVICE;
    player.attach(playlist, function(err) {
        assert.ok(!err);
        player.detach(function(err) {
            assert.ok(!err);
            done();
        });
    });
});

it("create, attach, detach loudness detector", function(done) {
  var playlist = groove.createPlaylist();
  var detector = groove.createLoudnessDetector();
  detector.attach(playlist, function(err) {
    assert.ok(!err);
    detector.detach(function(err) {
      assert.ok(!err);
      done();
    });
  });
});

it("create, attach, detach encoder", function(done) {
    var playlist = groove.createPlaylist();
    var encoder = groove.createEncoder();
    encoder.formatShortName = "ogg";
    encoder.codecShortName = "vorbis";
    encoder.attach(playlist, function(err) {
        assert.ok(!err);
        encoder.detach(function(err) {
            assert.ok(!err);
            done();
        });
    });
});

it("create, attach, detach fingerprinter", function(done) {
    var playlist = groove.createPlaylist();
    var fingerprinter = groove.createFingerprinter();
    fingerprinter.attach(playlist, function(err) {
        assert.ok(!err);
        fingerprinter.detach(function(err) {
            assert.ok(!err);
            done();
        });
    });
});

