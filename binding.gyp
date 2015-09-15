{
  "targets": [
    {
        "target_name": "groove",
        "sources": [
          "src/player.cc",
          "src/groove.cc",
          "src/file.cc",
          "src/playlist.cc",
          "src/playlist_item.cc",
          "src/waveform_builder.cc",
          "src/loudness_detector.cc",
          "src/fingerprinter.cc",
          "src/encoder.cc",
          "src/device.cc",
        ],
        "libraries": [
            "-lgroove"
        ],
        "include_dirs": [
            "<!(node -e \"require('nan')\")"
        ]
    }
  ]
}
