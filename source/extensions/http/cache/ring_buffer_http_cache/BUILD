load(
  "//bazel:envoy_build_system.bzl",
  "envoy_extension_package",
  "envoy_cc_extension",
)

envoy_extension_package()

envoy_cc_extension(
  name = "config",
  copts = [
    "-Wno-error",
  ],
  srcs = [
    "ring_buffer_http_cache.cc",
  ],
  hdrs = [
    "ring_buffer_http_cache.h",
  ],
  deps = [
    "//source/extensions/filters/http/cache:http_cache_lib",
  ],
)
