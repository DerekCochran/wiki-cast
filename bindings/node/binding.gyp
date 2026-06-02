{
  "targets": [
    {
      "target_name": "wiki-cast",
      "defines": [ "NAPI_VERSION=9", "_DEFAULT_SOURCE" ],
      "sources": [
        "src/addon.c"
      ],
      "include_dirs": [ "../../include" ],
      "libraries": [
        "-L../../../build",
        "-lwiki_cast",
        "-lcjson",
        "-licuuc",
        "-licudata"
      ],
      "configurations": {
        "Debug": {
          "cflags": [
            "-g3",
            "-O0",
            "-fno-omit-frame-pointer",
            "-Wall",
            "--debug"
          ],
          "xcode_settings": {
            "OTHER_CFLAGS": ["-std=c17", "-g3", "-O0"],
            "GCC_OPTIMIZATION_LEVEL": "0"
          }
        },
        "Release": {
          "defines": ["NDEBUG"],
          "cflags": [
            "-march=native",
            "-mtune=native",
            "-O3",
            "-flto",
            "-Wall"
          ],
          "xcode_settings": {
            "OTHER_CFLAGS": ["-std=c17", "-O3"],
            "GCC_OPTIMIZATION_LEVEL": "0"
          }
        }
      }
    }
  ]
}
