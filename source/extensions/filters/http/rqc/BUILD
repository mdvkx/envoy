
load(
    "//bazel:envoy_build_system.bzl",
    "envoy_cc_extension",
    "envoy_cc_library",
    "envoy_extension_package",
    "envoy_proto_library",
)

envoy_extension_package()

envoy_cc_extension(
    name = "config",  # why is it called "config"?
    deps = [
        ":lib"
    ]
)

envoy_cc_library(
    name = "lib",
    srcs = [
        "filter.cc",
    ],
    hdrs = [
        "cache.h",
        "filter.h",
    ],
    deps = [
        ":config_proto_cc_proto",
        "//source/common/http:header_map_lib",
        "//source/extensions/filters/http/common:concurrent_hash_map_lib",
        "//source/extensions/filters/http/common:factory_base_lib",
        "//source/extensions/filters/http/common:pass_through_filter_lib",
    ],
    copts = [
      "-Wno-error"  # temporarily ignore "parameter not used" etc. warnings
    ]
)

envoy_proto_library(
    name = "config_proto",
    srcs = [
        "config.proto"
    ]
)
