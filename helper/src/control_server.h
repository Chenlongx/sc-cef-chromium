#pragma once

#include <functional>
#include <string>

#ifdef _WIN32
#include <winsock2.h>
#endif

namespace sc_cef {

using LineHandler = std::function<std::string(const std::string& line)>;

/** JSON-lines TCP server on 127.0.0.1; prints CONTROL_PORT=N to stdout. */
class ControlServer {
 public:
  explicit ControlServer(LineHandler handler);
  ~ControlServer();

  bool Start();
  void Stop();
  int port() const { return port_; }

 private:
  LineHandler handler_;
  // POSIX fd vs Windows SOCKET handle.
#ifdef _WIN32
  using SocketHandle = SOCKET;
  static constexpr SocketHandle InvalidSocket = INVALID_SOCKET;
#else
  using SocketHandle = int;
  static constexpr SocketHandle InvalidSocket = -1;
#endif

  SocketHandle listen_fd_ = InvalidSocket;
  int port_ = 0;
  bool running_ = false;
};

}  // namespace sc_cef
