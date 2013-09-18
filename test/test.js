var groove = require('../build/Release/groove');
var assert = require('assert');

assert.strictEqual(groove.LOG_ERROR, 16);
groove.setLogging(groove.LOG_INFO);

