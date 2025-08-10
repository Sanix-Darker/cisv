{
  "targets": [
    {
      "target_name": "cisv",
      "sources": [
        "src/cisv_addon.cc",
        "src/cisv_parser.c",
        "src/cisv_transformer.c"
      ],
      "include_dirs": [
        "<!@(node -p \"require('node-addon-api').include\")",
        "src/"
      ],
      "dependencies": [
        "<!(node -p \"require('node-addon-api').gyp\")"
      ],
      "cflags!": [ "-fno-exceptions" ],
      "cflags": ["-O3", "-mavx2"],
      "cflags_cc!": [ "-fno-exceptions" ],
      "defines": [
        "NAPI_DISABLE_CPP_EXCEPTIONS",
        "NAPI_VERSION=6"
      ],
      "conditions": [
        ["OS=='linux'", {
          "cflags": [
            "-O3",
            "-march=native",
            "-mtune=native",
            "-ffast-math"
          ],
          "cflags_cc": [
            "-O3",
            "-march=native",
            "-mtune=native",
            "-ffast-math"
          ]
        }],
        ["OS=='mac'", {
          "xcode_settings": {
            "GCC_OPTIMIZATION_LEVEL": "3",
            "OTHER_CFLAGS": [
              "-march=native",
              "-mtune=native",
              "-ffast-math"
            ],
            "OTHER_CPLUSPLUSFLAGS": [
              "-march=native",
              "-mtune=native",
              "-ffast-math"
            ]
          }
        }]
      ]
    }
  ]
}
