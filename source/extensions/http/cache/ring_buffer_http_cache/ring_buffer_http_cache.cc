
#include "source/extensions/http/cache/ring_buffer_http_cache/ring_buffer_http_cache.h"

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

  auto  g_Register  = Registry::RegisterFactory<RingBufferHttpCacheFactory, HttpCacheFactory>  ();

} // namespace

} // namespace Cache
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
