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

#### file.metadata

For convenience, this is an object with all the metadata filled in.
You can still query individually with `getMetadata`. In order
to update metadata you must call `setMetadata` and then `save`.
This property is *not* updated when you call `setMetadata`.

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

#### player.playlist

Read only. Use `player.insert` and `player.remove` to modify.

`[playlistItem1, playlistItem2, ...]`

#### player.destroy(callback)

`callback(err)`

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

Returns {item, pos} where `item` is the playlist item currently playing
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

### GrooveReplayGainScan
