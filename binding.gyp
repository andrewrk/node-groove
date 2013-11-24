{
  "targets": [
    {
        "target_name": "groove",
        "sources": [
          "src/groove.cc",
          "src/gn_file.cc",
          "src/gn_playlist.cc",
          "src/gn_player.cc",
          "src/gn_playlist_item.cc",
          "src/gn_loudness_detector.cc",
          "src/gn_encoder.cc"
        ],
        "libraries": [
            "-lgroove",
            "-lgrooveplayer",
            "-lgrooveloudness"
        ]
    }
  ]
}
