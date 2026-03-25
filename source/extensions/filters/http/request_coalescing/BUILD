
load(
  "//bazel:envoy_build_system.bzl",
  "envoy_cc_extension",
  "envoy_cc_library",
  "envoy_extension_package",
  "envoy_proto_library"
)

envoy_extension_package()

envoy_proto_library(
  name = "request_coalescing_filter_proto",
  srcs = [
    "request_coalescing_filter.proto"
  ]
)

envoy_cc_extension(
  name = "config",
  copts = [
    "-Wno-error",
  ],
  srcs = [
    "request_coalescing_filter.cc",
  ],
  hdrs = [
    "request_coalescing_filter.h",
  ],
  deps = [
    "//source/common/http:header_map_lib",
    "//source/extensions/filters/http/cache:key_cc_proto",
    "//source/extensions/filters/http/common:factory_base_lib",
    "//source/extensions/filters/http/common:pass_through_filter_lib",
    ":request_coalescing_filter_proto_cc_proto"
  ]
)
