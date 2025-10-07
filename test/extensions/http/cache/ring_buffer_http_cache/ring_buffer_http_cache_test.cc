
#include "envoy/registry/registry.h"
#include "source/extensions/http/cache/ring_buffer_http_cache/ring_buffer_http_cache.h"
#include "gtest/gtest.h"

// --------------------------------------------------------------------------

namespace Envoy
{
namespace Extensions
{
namespace HttpFilters
{
namespace Cache
{
namespace
{

TEST ( Registration, RingBufferTestExample )
{
  auto  x = std::make_shared<RingBufferHttpCache> ();
  EXPECT_EQ ( x -> cacheInfo () . name_, "envoy.extensions.http.cache.ring_buffer" );
}

} // namespace
} // namespace Cache
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
