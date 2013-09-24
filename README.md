# node-groove

Node.js bindings to [libgroove](https://github.com/superjoe30/libgroove) -
generic music player backend library.

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

#### player.insert(file, nextPlaylistItem)

Creates a new playlist item with file and puts it in the playlist before
`nextPlaylistItem`. If `nextPlaylistItem` is `null`, appends the new
item to the playlist.

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

#### player.playing()

Returns `true` or `false`.

#### player.clear()

Remove all playlist items.

#### player.count()

How many items are on the playlist.

#### player.setReplayGainMode(playlistItem, replayGainMode)

`replayGainMode` can be:

 * `groove.REPLAYGAINMODE_OFF`
 * `groove.REPLAYGAINMODE_TRACK`
 * `groove.REPLAYGAINMODE_ALBUM`

#### player.setReplayGainPreamp(value)

`value` - range 0.0 to 1.0 this is essentially a volume control that only
applies when replaygain is turned on.

Defaults to 0.75.

#### player.getReplayGainPreamp()

#### player.setReplayGainDefault(value)

`value` - range 0.0 to 1.0 this is the replaygain adjustment to make
if replaygain tags are missing. Note this library provides replaygain
scanning capabilities but it is up to the music player app to perform
them, because you want to make sure that entire albums are scanned at once.

Defaults to 0.25.

#### player.getReplayGainDefault()

### GroovePlaylistItem

These are not instantiated directly; instead they are returned from
`player.playlist()`.

#### item.file

#### item.replayGainMode

#### item.id

Every time you obtain a playlist item from groove, you will get a fresh
JavaScript object, but it might point to the same underlying libgroove pointer
as another. The `id` field is a way to check if two playlist items reference
the same one.

### GrooveReplayGainScan

**Note**: GrooveReplayGainScan is documented but the bindings are
not yet implemented.

#### groove.createReplayGainScan(callback)

`callback(err, scan)`

#### scan.destroy(callback)

Must be called to cleanup. If you call it during a scan it will cleanly abort.

`callback(err)`

#### scan.add(filename)

Add a file to the scan. Nothing happens until you `exec()`.

#### scan.exec()

Starts the scan. You will receive progress events.

#### scan.on('progress', handler)

`handler(progress)`

`progress` is an object with these properties:

 * `metadataCurrent`
 * `metadataTotal`
 * `scanningCurrent`
 * `scanningTotal`
 * `updateCurrent`
 * `updateTotal`

#### scan.on('end', handler)

When the scan is complete.

`handler()`
