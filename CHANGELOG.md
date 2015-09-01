# 3.0.0 (UNRELEASED)

 * `player.setItemGain` and `player.setItemPeak` are gone in favor of
   `player.setItemGainPeak`
 * `groove.getDevices()` returns an object instead of an array.
 * `ANY_SINK_FULL` is now default instead of `EVERY_SINK_FULL`
 * `player.targetAudioFormat`, `player.actualAudioFormat`,
   `encoder.targetAudioFormat`, `encoder.actualAudioFormat`:
   - `channelLayout` - instead of a number it is an array of channel ids
   * `sampleFormat` - sample format enum values are different
 * `player.deviceBufferSize` - removed. This functionality no longer exists.
 * `player.sinkBufferSize` - removed. This functionality no longer exists.
 * `detector.sinkBufferSize` - removed. This functionality no longer exists.
 * `printer.sinkBufferSize` - removed. This functionality no longer exists.
 * `encoder.sinkBufferSize` - removed. This functionality no longer exists.
 * `player.deviceIndex` - removed in favor of `player.device`.
 * `player.device` is mandatory, and you must get a device reference by calling
   `groove.getDevices()`. You must call `groove.connectSoundBackend()` before
   calling `groove.getDevices()`.
 * After calling `playlist.create` you must call `playlist.destroy` when finished
   with the playlist.
