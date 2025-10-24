load(
  "//bazel:envoy_build_system.bzl",  # envoy_cc_extension
  "envoy_cc_extension",
  "envoy_extension_package",
)

envoy_extension_package()

envoy_cc_extension(
  name   = "config",
  srcs   = [ "ring_buffer_http_cache.cc" ],
  hdrs   = [ "ring_buffer_http_cache.h", "ring_buffer.h" ],
  copts  = [ "-Wno-error" ],

  deps = [
    "//source/extensions/filters/http/cache:http_cache_lib",
  ],
)