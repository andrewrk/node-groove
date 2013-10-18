# node-groove

Node.js bindings to [libgroove](https://github.com/superjoe30/libgroove) -
generic music player backend library.

Live discussion in #libgroove IRC channel on irc.freenode.org.

## Usage

1. Install libgroove to your system.
2. `npm install --save groove`

### Get Metadata from File

```js
var groove = require('groove');

groove.open("danse-macabre.ogg", function(err, file) {
  if (err) throw err;
  console.log(file.metadata());
  console.log("duration:", file.duration());
  file.close(function(err) {
    if (err) throw err;
  });
});
```

#### More Examples

 * example/metadata.js - read or update metadata in a media file
 * example/playlist.js - play several files in a row and then exit

## API Documentation

### globals

#### groove.setLogging(level)

`level` can be:

 * `groove.LOG_QUIET`
 * `groove.LOG_ERROR`
 * `groove.LOG_WARNING`
 * `groove.LOG_INFO`

### GrooveFile

#### groove.open(filename, callback)

`callback(err, file)`

#### file.close(callback)

`callback(err)`

#### file.duration()

In seconds.

#### file.shortNames()

A comma-separated list of short names for the format.

#### file.getMetadata(key, [flags])

Flags:

 * `groove.TAG_MATCH_CASE`
 * `groove.TAG_DONT_OVERWRITE`
 * `groove.TAG_APPEND`

#### file.setMetadata(key, value, [flags])

See `getMetadata` for flags.

Pass `null` for `value` to delete a key.

#### file.metadata()

This returns an object populated with all the metadata.
Updating the object does nothing. Use `setMetadata` to
update metadata and then `save` to write changes to disk.

#### file.dirty

Boolean whether `save` will do anything.

#### file.filename

The string that was passed to `groove.open`

#### file.save(callback)

`callback(err)`

### GroovePlayer

#### groove.createPlayer(callback)

`callback(err, player)`

#### player.destroy(callback)

`callback(err)`

#### player.playlist()

Returns a read-only array of playlist items.
Use `player.insert` and `player.remove` to modify.

`[playlistItem1, playlistItem2, ...]`

#### player.on('nowplaying', handler)

Fires when the item that is now playing changes. It can be `null`.

`handler()`

#### player.on('bufferunderrun', handler)

Fires when a buffer underrun occurs. Ideally you'll never see this.

`handler()`

#### player.play()

#### player.pause()

#### player.seek(playlistItem, position)

Seek to `playlistItem`, `position` seconds into the song.

#### player.insert(file, gain, nextPlaylistItem)

Creates a new playlist item with file and puts it in the playlist before
`nextPlaylistItem`. If `nextPlaylistItem` is `null`, appends the new
item to the playlist.

`gain` is a float format volume adjustment that applies only to this item.
defaults to 1.0

Returns the newly added playlist item.

Once you add a file to the playlist, you must not `file.close()` it until
you first remove it from the playlist.

#### player.remove(playlistItem)

Remove `playlistItem` from the playlist.

Note that you are responsible for calling `file.close()` on every file
that you open with `groove.open`. `player.remove` will not close files.

#### player.position()

Returns `{item, pos}` where `item` is the playlist item currently playing
and `pos` is how many seconds into the song the play head is.

#### player.decodePosition()

Returns `{item, pos}` where `item` is the playlist item currently being
decoded and `pos` is how many seconds into the song the decode head is.

#### player.playing()

Returns `true` or `false`.

#### player.clear()

Remove all playlist items.

#### player.count()

How many items are on the playlist.

#### player.volume

#### player.setVolume(value)

Between 0.0 and 1.0. You probably want to leave this at 1.0, since using
replaygain will typically lower your volume a significant amount.

#### player.setItemGain(playlistItem, gain)

`gain` is a float that affects the volume of the specified playlist item only.
To convert from dB to float, use exp(log(10) * 0.05 * dBValue).

### GroovePlaylistItem

These are not instantiated directly; instead they are returned from
`player.playlist()`.

#### item.file

#### item.gain

#### item.id

Every time you obtain a playlist item from groove, you will get a fresh
JavaScript object, but it might point to the same underlying libgroove pointer
as another. The `id` field is a way to check if two playlist items reference
the same one.

### GrooveReplayGainScan

#### groove.createReplayGainScan(fileList, progressInterval)

returns a GrooveReplayGainScan

`fileList` is an array of GrooveFiles
`progressInterval` is number of seconds to decode before 'progress' is emitted.

#### scan.abort()

Ends a scan early.

#### scan.on('progress', handler)

`handler(file, progress)`

`file` - the GrooveFile that is being scanned
`progress` - float from 0 to 1 how much done it is

#### scan.on('file', handler)

`handler(file, gain, peak)`

`file` - the GrooveFile that was scanned
`gain` - suggested gain adjustment in dB of the file
`peak` - sample peak in float format of the file

#### scan.on('end', handler)

When the scan is complete.

`handler(gain, peak)`

`gain` - suggested gain adjustment in dB of all files scanned
`peak` - sample peak in float format of all files scanned

#### scan.on('error', handler)

