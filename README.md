# node-groove

Node.js bindings to [libgroove](https://github.com/superjoe30/libgroove) -
generic music player backend library.

## Usage

1. Install libgroove to your system.
2. `npm install --save groove`

```js
var groove = require('groove');

groove.open("danse-macabre.ogg", function(err, file) {
  if (err) throw err;
  console.log(file.metadata);
  console.log("duration:", file.duration());
  file.close();
});
```

## API Documentation

### GrooveFile

#### groove.open(filename, callback)

`callback(err, file)`

#### file.close()

#### file.duration()

In seconds.

#### file.shortNames()

A comma-separated list of short names for the format.

#### file.getMetadata()

#### file.setMetadata()

#### file.metadata

For convenience, this is an object with all the metadata filled in.
You can still query individually with `getMetadata`. In order
to update metadata you must call `setMetadata` and then `save`.

#### file.dirty

Boolean whether `save` will do anything.

#### file.save(callback)

`callback(err)`

### GroovePlayer

### GrooveReplayGainScan
