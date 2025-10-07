load(
  "//bazel:envoy_build_system.bzl",  # envoy_package
  "envoy_package"
)
# multiple load() directives are ok, i guess
load(
  "//test/extensions:extensions_build_system.bzl",  # envoy_extension_cc_test
  "envoy_extension_cc_test",
)

envoy_package()

envoy_extension_cc_test(
  name             = "ring_buffer_http_cache_test",
  srcs             = [ "ring_buffer_http_cache_test.cc" ],
  extension_names  = [ "envoy.extensions.http.cache.ring_buffer" ],
  copts = [ "-Wno-error" ],

  deps  = [
    "//source/extensions/http/cache/ring_buffer_http_cache:config",
  ],

  #rbe_pool = "6gig",  # ???. this didn't help
)