{
  "targets": [
    {
      "target_name": "cisv",
      "sources": [
        "src/cisv_addon.cc",
        "src/cisv_parser.c"
      ],
      'include_dirs': ["<!@(node -p \"require('node-addon-api').include\")"],
      'dependencies': ["<!(node -p \"require('node-addon-api').gyp\")"],
      "cflags!": [ "-fno-exceptions" ],
      "cflags_cc!": [ "-fno-exceptions" ],
      "defines": [ "NAPI_CPP_EXCEPTIONS" ],
      "xcode_settings": { "GCC_ENABLE_CPP_EXCEPTIONS": "YES" },
      "conditions": [
        ["OS=='linux' or OS=='mac'", {
          "cflags": [ "-O3", "-march=native", "-flto" ],
          "ldflags": [ "-flto" ]
        }]
      ]
    }
  ]
}
