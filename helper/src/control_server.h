#pragma once

#include <functional>
#include <string>

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
  int listen_fd_ = -1;
  int port_ = 0;
  bool running_ = false;
};

}  // namespace sc_cef
