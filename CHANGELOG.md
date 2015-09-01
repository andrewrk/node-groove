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
