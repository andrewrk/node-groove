# node-groove

Node.js bindings to [libgroove](https://github.com/andrewrk/libgroove) -
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
 * example/replaygain.js - compute replaygain values for media files
 * example/transcode.js - convert and splice several files together
 * example/fingerprint.js - create an acoustid fingerprint for media files

## API Documentation

### globals

#### groove.setLogging(level)

`level` can be:

 * `groove.LOG_QUIET`
 * `groove.LOG_ERROR`
 * `groove.LOG_WARNING`
 * `groove.LOG_INFO`

#### groove.loudnessToReplayGain(loudness)

Converts a loudness value which is in LUFS to the ReplayGain-suggested dB
adjustment.

#### groove.dBToFloat(dB)

Converts dB format volume adjustment to a floating point gain format.

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

### GroovePlaylist

#### groove.createPlaylist()

A playlist managers keeping an audio buffer full. To send the buffer
to your speakers, use `playlist.createPlayer()`.

#### playlist.items()

Returns a read-only array of playlist items.
Use `playlist.insert` and `playlist.remove` to modify.

`[playlistItem1, playlistItem2, ...]`

#### playlist.play()

#### playlist.pause()

#### playlist.seek(playlistItem, position)

Seek to `playlistItem`, `position` seconds into the song.

#### playlist.insert(file, gain, peak, nextPlaylistItem)

Creates a new playlist item with file and puts it in the playlist before
`nextPlaylistItem`. If `nextPlaylistItem` is `null`, appends the new
item to the playlist.

`gain` is a float format volume adjustment that applies only to this item.
defaults to 1.0

`peak` is float format, see `item.peak`.
defaults to 1.0

Returns the newly added playlist item.

Once you add a file to the playlist, you must not `file.close()` it until
you first remove it from the playlist.

#### playlist.remove(playlistItem)

Remove `playlistItem` from the playlist.

Note that you are responsible for calling `file.close()` on every file
that you open with `groove.open`. `playlist.remove` will not close files.

#### playlist.position()

Returns `{item, pos}` where `item` is the playlist item currently being
decoded and `pos` is how many seconds into the song the decode head is.

Note that typically you are more interested in the position of the play head,
not the decode head. Example methods which return the play head are
`player.position()` and `encoder.position()`.

#### playlist.playing()

Returns `true` or `false`.

#### playlist.clear()

Remove all playlist items.

#### playlist.count()

How many items are on the playlist.

#### playlist.gain

#### playlist.setGain(value)

Between 0.0 and 1.0. You probably want to leave this at 1.0, since using
replaygain will typically lower your volume a significant amount.

#### playlist.setItemGain(playlistItem, gain)

`gain` is a float that affects the volume of the specified playlist item only.
To convert from dB to float, use exp(log(10) * 0.05 * dBValue).

#### playlist.setItemPeak(playlistItem, peak)

See `item.peak`

### GroovePlaylistItem

These are not instantiated directly; instead they are returned from
`playlist.items()`.

#### item.file

#### item.gain

A volume adjustment in float format to apply to the file when it plays.
This is typically used for loudness compensation, for example ReplayGain.
To convert from dB to float, use `groove.dBToFloat`

#### item.peak

The sample peak of this playlist item is assumed to be 1.0 in float
format. If you know for certain that the peak is less than 1.0, you
may set this value which may allow the volume adjustment to use
a pure amplifier rather than a compressor. This results in slightly
better audio quality.

#### item.id

Every time you obtain a playlist item from groove, you will get a fresh
JavaScript object, but it might point to the same underlying libgroove pointer
as another. The `id` field is a way to check if two playlist items reference
the same one.

### GroovePlayer

#### groove.getDevices()

Returns an array of device names which are the devices you can send audio
to.

#### groove.createPlayer()

Creates a GroovePlayer instance which you can then configure by setting
properties.

#### player.deviceIndex

Before calling `attach()`, set this to the index of one of the devices
returned from `groove.getDevices()` or `null` to represent the default device.
Use `groove.DUMMY_DEVICE` to represent a dummy audio player.

#### player.targetAudioFormat

The desired audio format settings with which to open the device.
`groove.createPlayer()` defaults these to 44100 Hz,
signed 16-bit int, stereo.
These are preferences; if a setting cannot be used, a substitute will
be used instead. In this case, actualAudioFormat will be updated to reflect
the substituted values.

Properties:

 * `sampleRate`
 * `channelLayout`
 * `sampleFormat`

#### player.actualAudioFormat

groove sets this to the actual format you get when you open the device.
Ideally will be the same as targetAudioFormat but might not be.

Properties:

 * `sampleRate`
 * `channelLayout`
 * `sampleFormat`

#### player.deviceBufferSize

how big the device buffer should be, in sample frames.
must be a power of 2.
`groove.createPlayer()` defaults this to 1024

#### player.sinkBufferSize

How big the sink buffer should be, in sample frames.
`groove.createPlayer()` defaults this to 8192

#### player.attach(playlist, callback)

Sends audio to sound device.

`callback(err)`

#### player.detach(callback)

`callback(err)`

#### player.position()

Returns `{item, pos}` where `item` is the playlist item currently being
played and `pos` is how many seconds into the song the play head is.

#### player.on('nowplaying', handler)

Fires when the item that is now playing changes. It can be `null`.

`handler()`

#### player.on('bufferunderrun', handler)

Fires when a buffer underrun occurs. Ideally you'll never see this.

`handler()`

### GrooveEncoder

#### groove.createEncoder()

#### encoder.bitRate

select encoding quality by choosing a target bit rate

#### encoder.formatShortName

optional - help libgroove guess which format to use.
`avconv -formats` to get a list of possibilities.

#### encoder.codecShortName

optional - help libgroove guess which codec to use.
`avconv-codecs` to get a list of possibilities.

#### encoder.filename

optional - provide an example filename to help libgroove guess
which format/codec to use.

#### encoder.mimeType

optional - provide a mime type string to help libgrooove guess
which format/codec to use.

#### encoder.targetAudioFormat

The desired audio format settings with which to encode.
`groove.createEncoder()` defaults these to 44100 Hz,
signed 16-bit int, stereo.
These are preferences; if a setting cannot be used, a substitute will
be used instead. In this case, actualAudioFormat will be updated to reflect
the substituted values.

Properties:

 * `sampleRate`
 * `channelLayout`
 * `sampleFormat`

#### encoder.actualAudioFormat

groove sets this to the actual format you get when you attach the encoder.
Ideally will be the same as targetAudioFormat but might not be.

Properties:

 * `sampleRate`
 * `channelLayout`
 * `sampleFormat`


#### encoder.attach(playlist, callback)

`callback(err)`

#### encoder.detach(callback)

`callback(err)`

#### encoder.getBuffer()

Returns `null` if no buffer available, or an object with these properties:

 * `buffer` - a node `Buffer` instance which is the encoded data for this chunk
   this can be `null` in which case this buffer is actually the end of
   playlist sentinel.
 * `item` - the GroovePlaylistItem of which this buffer is encoded data for
 * `pos` - position in seconds that this buffer represents in into the item

#### encoder.on('buffer', handler)

`handler()`

Emitted when there is a buffer available to get. You still need to get the
buffer with `getBuffer()`.

#### encoder.position()

Returns `{item, pos}` where `item` is the playlist item currently being
encoded and `pos` is how many seconds into the song the encode head is.

### GrooveLoudnessDetector

#### groove.createLoudnessDetector()

returns a GrooveLoudnessDetector

#### detector.infoQueueSize

Set this to determine how far ahead into the playlist to look.

#### detector.sinkBufferSize

How big the sink buffer should be, in sample frames.
`groove.createLoudnessDetector()` defaults this to 8192

#### detector.disableAlbum

Set to `true` to only compute track loudness. This is faster and requires less
memory than computing both.

#### detector.attach(playlist, callback)

`callback(err)`

#### detector.detach(callback)

`callback(err)`

#### detector.getInfo()

Returns `null` if no info available, or an object with these properties:

 * `loudness` - loudness in LUFS
 * `peak` - sample peak in float format of the file
 * `duration` - duration in seconds of the track
 * `item` - the GroovePlaylistItem that this applies to, or `null` if it applies
   to the entire album.

#### detector.position()

Returns `{item, pos}` where `item` is the playlist item currently being
detected and `pos` is how many seconds into the song the detect head is.

#### detector.on('info', handler)

`handler()`

Emitted when there is info available to get. You still need to get the info
with `getInfo()`.

### GrooveFingerprinter

#### groove.createFingerprinter()

returns a GrooveFingerprinter

#### groove.encodeFingerprint(rawFingerprint)

Given an Array of integers which is the raw fingerprint, encode it into a
string which can be submitted to acoustid.org.

#### groove.decodeFingerprint(fingerprint)

Given the fingerprint string, returns a list of integers which is the raw
fingerprint data.

#### printer.infoQueueSize

Set this to determine how far ahead into the playlist to look.

#### printer.sinkBufferSize

How big the sink buffer should be, in sample frames.
`groove.createFingerprinter()` defaults this to 8192

#### printer.attach(playlist, callback)

`callback(err)`

#### printer.detach(callback)

`callback(err)`

#### printer.getInfo()

Returns `null` if no info available, or an object with these properties:

 * `fingerprint` - integer array which is the raw fingerprint
 * `duration` - duration in seconds of the track
 * `item` - the GroovePlaylistItem that this applies to, or `null` if it applies
   to the entire album.

#### printer.position()

Returns `{item, pos}` where `item` is the playlist item currently being
fingerprinted and `pos` is how many seconds into the song the printer head is.

#### printer.on('info', handler)

`handler()`

Emitted when there is info available to get. You still need to get the info
with `getInfo()`.
