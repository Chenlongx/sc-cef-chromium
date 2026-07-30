#include "control_server.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstring>
#include <iostream>
#include <thread>

namespace sc_cef {

ControlServer::ControlServer(LineHandler handler) : handler_(std::move(handler)) {}

ControlServer::~ControlServer() { Stop(); }

bool ControlServer::Start() {
  listen_fd_ = ::socket(AF_INET, SOCK_STREAM, 0);
  if (listen_fd_ < 0) return false;

  int yes = 1;
  setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  addr.sin_port = htons(0);

  if (bind(listen_fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
    close(listen_fd_);
    listen_fd_ = -1;
    return false;
  }
  socklen_t len = sizeof(addr);
  if (getsockname(listen_fd_, reinterpret_cast<sockaddr*>(&addr), &len) == 0) {
    port_ = ntohs(addr.sin_port);
  }
  if (listen(listen_fd_, 8) < 0) {
    close(listen_fd_);
    listen_fd_ = -1;
    return false;
  }

  running_ = true;
  std::cout << "CONTROL_PORT=" << port_ << std::endl;
  std::cout.flush();

  std::thread([this]() {
    while (running_) {
      const int client = accept(listen_fd_, nullptr, nullptr);
      if (client < 0) continue;
      std::thread([this, client]() {
        std::string buf;
        char chunk[1024];
        while (running_) {
          const ssize_t n = recv(client, chunk, sizeof(chunk), 0);
          if (n <= 0) break;
          buf.append(chunk, static_cast<size_t>(n));
          size_t pos;
          while ((pos = buf.find('\n')) != std::string::npos) {
            std::string line = buf.substr(0, pos);
            buf.erase(0, pos + 1);
            if (line.empty()) continue;
            std::string reply = handler_ ? handler_(line) : R"({"ok":false,"error":"no handler"})";
            if (!reply.empty() && reply.back() != '\n') reply.push_back('\n');
            send(client, reply.data(), reply.size(), 0);
          }
        }
        close(client);
      }).detach();
    }
  }).detach();

  return true;
}

void ControlServer::Stop() {
  running_ = false;
  if (listen_fd_ >= 0) {
    close(listen_fd_);
    listen_fd_ = -1;
  }
}

}  // namespace sc_cef
