
load(
  "//bazel:envoy_build_system.bzl"
)
load(
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
  ],
  deps = [
  ],
)

envoy_cc_extension(
  name = "config",
  srcs = [
    "request_coalescing_filter.cc",
  ],
  hdrs = [
    "request_coalescing_filter.h",
  ],
  deps = [
    ":request_coalescing_filter_proto_cc_proto"
  ]
)
