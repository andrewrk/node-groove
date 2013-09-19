var groove = require('../build/Release/groove');
var assert = require('assert');
var path = require('path');
var testOgg = path.join(__dirname, "danse.ogg");

assert.strictEqual(groove.LOG_ERROR, 16);
groove.setLogging(groove.LOG_INFO);

groove.open(testOgg, function(err, file) {
    if (err) {
        console.error("Error opening file:", err.stack);
        return;
    }
    file.close();
});
