// Copyright 2026
//
// Verilator-side socket bridge for Ara RF I/O.

#include "rfio_socket.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <netinet/in.h>
#include <poll.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstdlib>
#include <iostream>
#include <stdexcept>

namespace {
RfioSocketBridge *global_bridge = nullptr;

[[noreturn]] void FatalDpiError(const char *fn, const std::exception &err) {
  std::cerr << "ERROR: " << fn << ": " << err.what() << std::endl;
  std::abort();
}
}  // namespace

RfioSocketBridge::RfioSocketBridge()
    : bind_addr_("127.0.0.1"),
      port_(9090),
      listen_fd_(-1),
      client_fd_(-1),
      listening_(false),
      printed_waiting_(false) {}

RfioSocketBridge::~RfioSocketBridge() {
  CloseClient();
  CloseListen();
}

bool RfioSocketBridge::ParseCLIArguments(int argc, char **argv,
                                         bool &exit_app) {
  const struct option long_options[] = {
      {"rfio-port", required_argument, nullptr, 1000},
      {"rfio-bind", required_argument, nullptr, 1001},
      {"rfio-help", no_argument, nullptr, 1002},
      {nullptr, no_argument, nullptr, 0}};

  optind = 1;
  opterr = 0;

  while (true) {
    int c = getopt_long(argc, argv, "", long_options, nullptr);
    if (c == -1) break;

    switch (c) {
      case 1000: {
        long parsed = strtol(optarg, nullptr, 10);
        if (parsed <= 0 || parsed > 65535) {
          std::cerr << "ERROR: --rfio-port must be in range 1..65535"
                    << std::endl;
          return false;
        }
        port_ = static_cast<uint16_t>(parsed);
        break;
      }
      case 1001:
        bind_addr_ = optarg;
        break;
      case 1002:
        std::cout << "RFIO socket bridge options:\n\n"
                     "  --rfio-port=N\n"
                     "    TCP port to listen on. Default: 9090\n\n"
                     "  --rfio-bind=ADDR\n"
                     "    IPv4 address to bind. Default: 127.0.0.1\n\n";
        exit_app = true;
        return true;
      case '?':
      default:
        break;
    }
  }

  return true;
}

void RfioSocketBridge::PreExec() { StartListening(); }

void RfioSocketBridge::PostExec() {
  CloseClient();
  CloseListen();
}

uint32_t RfioSocketBridge::Status() {
  StartListening();
  PollAccept();
  PollRecv();

  uint32_t status = 0;
  if (rx_queue_.size() >= 4) status |= 1u;
  if (client_fd_ >= 0) status |= 2u;
  if (client_fd_ >= 0) status |= 4u;
  return status;
}

uint32_t RfioSocketBridge::RecvU32() {
  StartListening();

  while (rx_queue_.size() < 4) {
    EnsureClientBlocking();
    WaitForClientReadable();
    PollRecv();
  }

  uint32_t value = 0;
  for (unsigned i = 0; i < 4; ++i) {
    value |= static_cast<uint32_t>(rx_queue_.front()) << (8 * i);
    rx_queue_.pop_front();
  }
  return value;
}

void RfioSocketBridge::SendU32(uint32_t value) {
  StartListening();
  for (unsigned i = 0; i < 4; ++i) {
    SendByteBlocking(static_cast<uint8_t>((value >> (8 * i)) & 0xffu));
  }
}

void RfioSocketBridge::LogByte(uint32_t value) {
  char ch = static_cast<char>(value & 0xffu);
  std::cout.put(ch);
  if (ch == '\n') std::cout.flush();
}

void RfioSocketBridge::SetGlobal(RfioSocketBridge *bridge) {
  global_bridge = bridge;
}

RfioSocketBridge *RfioSocketBridge::Global() { return global_bridge; }

void RfioSocketBridge::StartListening() {
  if (listening_) return;

  listen_fd_ = socket(AF_INET, SOCK_STREAM, 0);
  if (listen_fd_ < 0) {
    throw std::runtime_error(std::string("socket failed: ") + strerror(errno));
  }

  int opt = 1;
  if (setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
    throw std::runtime_error(std::string("setsockopt failed: ") +
                             strerror(errno));
  }

  sockaddr_in addr {};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port_);
  if (inet_pton(AF_INET, bind_addr_.c_str(), &addr.sin_addr) != 1) {
    throw std::runtime_error("invalid --rfio-bind IPv4 address");
  }

  if (bind(listen_fd_, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) < 0) {
    throw std::runtime_error(std::string("bind failed: ") + strerror(errno));
  }

  if (listen(listen_fd_, 1) < 0) {
    throw std::runtime_error(std::string("listen failed: ") + strerror(errno));
  }

  SetNonblocking(listen_fd_);
  listening_ = true;

  std::cout << "RFIO socket bridge listening on " << bind_addr_ << ":"
            << port_ << std::endl;
}

void RfioSocketBridge::CloseClient() {
  if (client_fd_ >= 0) {
    close(client_fd_);
    client_fd_ = -1;
  }
  rx_queue_.clear();
}

void RfioSocketBridge::CloseListen() {
  if (listen_fd_ >= 0) {
    close(listen_fd_);
    listen_fd_ = -1;
  }
  listening_ = false;
}

void RfioSocketBridge::PollAccept() {
  if (client_fd_ >= 0 || listen_fd_ < 0) return;

  sockaddr_in client_addr {};
  socklen_t client_len = sizeof(client_addr);
  int fd = accept(listen_fd_, reinterpret_cast<sockaddr *>(&client_addr),
                  &client_len);
  if (fd < 0) {
    if (errno == EAGAIN || errno == EWOULDBLOCK) return;
    throw std::runtime_error(std::string("accept failed: ") + strerror(errno));
  }

  SetNonblocking(fd);
  client_fd_ = fd;
  printed_waiting_ = false;

  char addr_buf[INET_ADDRSTRLEN] = {};
  inet_ntop(AF_INET, &client_addr.sin_addr, addr_buf, sizeof(addr_buf));
  std::cout << "RFIO client connected from " << addr_buf << ":"
            << ntohs(client_addr.sin_port) << std::endl;
}

void RfioSocketBridge::PollRecv() {
  if (client_fd_ < 0) return;

  uint8_t buf[4096];
  while (true) {
    ssize_t got = recv(client_fd_, buf, sizeof(buf), 0);
    if (got > 0) {
      for (ssize_t i = 0; i < got; ++i) rx_queue_.push_back(buf[i]);
      continue;
    }
    if (got == 0) {
      std::cout << "RFIO client disconnected" << std::endl;
      CloseClient();
      return;
    }
    if (errno == EAGAIN || errno == EWOULDBLOCK) return;
    if (errno == EINTR) continue;
    std::cout << "RFIO recv error: " << strerror(errno) << std::endl;
    CloseClient();
    return;
  }
}

void RfioSocketBridge::EnsureClientBlocking() {
  while (client_fd_ < 0) {
    PollAccept();
    if (client_fd_ >= 0) return;

    if (!printed_waiting_) {
      std::cout << "RFIO waiting for client on " << bind_addr_ << ":" << port_
                << std::endl;
      printed_waiting_ = true;
    }

    pollfd pfd {};
    pfd.fd = listen_fd_;
    pfd.events = POLLIN;
    int rc = poll(&pfd, 1, -1);
    if (rc < 0 && errno != EINTR) {
      throw std::runtime_error(std::string("poll accept failed: ") +
                               strerror(errno));
    }
  }
}

void RfioSocketBridge::WaitForClientReadable() {
  if (client_fd_ < 0) return;

  pollfd pfd {};
  pfd.fd = client_fd_;
  pfd.events = POLLIN;
  while (true) {
    int rc = poll(&pfd, 1, -1);
    if (rc >= 0) return;
    if (errno != EINTR) {
      throw std::runtime_error(std::string("poll read failed: ") +
                               strerror(errno));
    }
  }
}

void RfioSocketBridge::WaitForClientWritable() {
  if (client_fd_ < 0) return;

  pollfd pfd {};
  pfd.fd = client_fd_;
  pfd.events = POLLOUT;
  while (true) {
    int rc = poll(&pfd, 1, -1);
    if (rc >= 0) return;
    if (errno != EINTR) {
      throw std::runtime_error(std::string("poll write failed: ") +
                               strerror(errno));
    }
  }
}

void RfioSocketBridge::SendByteBlocking(uint8_t value) {
  while (true) {
    EnsureClientBlocking();
    ssize_t sent = send(client_fd_, &value, 1, MSG_NOSIGNAL);
    if (sent == 1) return;
    if (sent < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
      WaitForClientWritable();
      continue;
    }
    if (sent < 0 && errno == EINTR) continue;

    std::cout << "RFIO send error/disconnect" << std::endl;
    CloseClient();
  }
}

void RfioSocketBridge::SetNonblocking(int fd) {
  int flags = fcntl(fd, F_GETFL, 0);
  if (flags < 0) {
    throw std::runtime_error(std::string("fcntl F_GETFL failed: ") +
                             strerror(errno));
  }
  if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) {
    throw std::runtime_error(std::string("fcntl F_SETFL failed: ") +
                             strerror(errno));
  }
}

extern "C" uint32_t rfio_dpi_status() {
  try {
    auto *bridge = RfioSocketBridge::Global();
    return bridge ? bridge->Status() : 0;
  } catch (const std::exception &err) {
    FatalDpiError("rfio_dpi_status", err);
  }
}

extern "C" uint32_t rfio_dpi_recv_u32() {
  try {
    auto *bridge = RfioSocketBridge::Global();
    return bridge ? bridge->RecvU32() : 0;
  } catch (const std::exception &err) {
    FatalDpiError("rfio_dpi_recv_u32", err);
  }
}

extern "C" void rfio_dpi_send_u32(uint32_t value) {
  try {
    auto *bridge = RfioSocketBridge::Global();
    if (bridge) bridge->SendU32(value);
  } catch (const std::exception &err) {
    FatalDpiError("rfio_dpi_send_u32", err);
  }
}

extern "C" void rfio_dpi_log_byte(uint32_t value) {
  try {
    auto *bridge = RfioSocketBridge::Global();
    if (bridge) bridge->LogByte(value);
  } catch (const std::exception &err) {
    FatalDpiError("rfio_dpi_log_byte", err);
  }
}
