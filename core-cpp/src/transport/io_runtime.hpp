// Library-internal io_context singleton. One thread, one context,
// started lazily on first access and joined at process exit.
//
// Internal header — NOT installed. The asio types leak through this
// header, which is fine because it's only #included by other files
// under src/transport/.
#ifndef OIGTL_TRANSPORT_IO_RUNTIME_HPP
#define OIGTL_TRANSPORT_IO_RUNTIME_HPP

#include <asio.hpp>

namespace oigtl::transport::detail {

// Access the shared io_context. Always valid; never null.
asio::io_context& io_ctx();

}  // namespace oigtl::transport::detail

#endif  // OIGTL_TRANSPORT_IO_RUNTIME_HPP
