/**
 * sc-cef-helper — CEF runtime control process.
 * Prints CONTROL_PORT=<port> then serves JSON-lines PROTOCOL.
 */

#include "browser_manager.h"
#include "control_server.h"

#include <unistd.h>

#include <atomic>
#include <chrono>
#include <iostream>
#include <string>
#include <thread>

#ifndef SC_CEF_STUB_BUILD
#include "include/cef_app.h"
#include "include/wrapper/cef_library_loader.h"
#endif

namespace {

std::atomic<bool> g_running{true};

std::string HandleLine(const std::string& line) {
  return sc_cef::BrowserManager::Instance().HandleControlLine(line);
}

}  // namespace

int main(int argc, char** argv) {
#ifndef SC_CEF_STUB_BUILD
#if defined(OS_MAC) || defined(__APPLE__)
  CefScopedLibraryLoader library_loader;
  if (!library_loader.LoadInMain()) {
    std::cerr << "[sc-cef-helper] failed to load Chromium Embedded Framework\n";
    return 1;
  }
#endif
  CefMainArgs main_args(argc, argv);
  const int exit_code = CefExecuteProcess(main_args, nullptr, nullptr);
  if (exit_code >= 0) return exit_code;
#else
  (void)argc;
  (void)argv;
#endif

  int cdp_port = 9222;
  for (int i = 1; i < argc; ++i) {
    const std::string a = argv[i];
    if (a.rfind("--remote-debugging-port=", 0) == 0) {
      cdp_port = std::stoi(a.substr(std::string("--remote-debugging-port=").size()));
    }
  }
  sc_cef::BrowserManager::Instance().SetCdpBasePort(cdp_port);

  sc_cef::ControlServer server(HandleLine);
  if (!server.Start()) {
    std::cerr << "[sc-cef-helper] failed to bind control socket\n";
    return 1;
  }

  std::cerr << "[sc-cef-helper] listening on CONTROL_PORT=" << server.port()
#ifdef SC_CEF_STUB_BUILD
            << " (STUB_BUILD)"
#else
            << " (CEF)"
#endif
            << " cdpBase=" << cdp_port << "\n";

#ifndef SC_CEF_STUB_BUILD
  while (g_running) {
    sc_cef::BrowserManager::Instance().PumpMainThreadTasks();
    CefDoMessageLoopWork();
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
  CefShutdown();
#else
  std::thread([]() {
    for (;;) {
      sc_cef::BrowserManager::Instance().PumpMainThreadTasks();
      std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
  }).detach();
  for (;;) {
    pause();
  }
#endif
  return 0;
}
