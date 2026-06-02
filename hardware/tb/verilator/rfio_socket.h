// Copyright 2026
//
// Verilator-side socket bridge for Ara RF I/O.

#ifndef ARA_TB_VERILATOR_RFIO_SOCKET_H_
#define ARA_TB_VERILATOR_RFIO_SOCKET_H_

#include <cstdint>
#include <deque>
#include <string>

#include "sim_ctrl_extension.h"

class RfioSocketBridge : public SimCtrlExtension {
 public:
  RfioSocketBridge();
  ~RfioSocketBridge() override;

  bool ParseCLIArguments(int argc, char **argv, bool &exit_app) override;
  void PreExec() override;
  void PostExec() override;

  uint32_t Status();
  uint32_t RecvU32();
  void SendU32(uint32_t value);
  void LogByte(uint32_t value);

  static void SetGlobal(RfioSocketBridge *bridge);
  static RfioSocketBridge *Global();

 private:
  void StartListening();
  void CloseClient();
  void CloseListen();
  void PollAccept();
  void PollRecv();
  void EnsureClientBlocking();
  void WaitForClientReadable();
  void WaitForClientWritable();
  void SendByteBlocking(uint8_t value);
  void SetNonblocking(int fd);

  std::string bind_addr_;
  uint16_t port_;
  int listen_fd_;
  int client_fd_;
  bool listening_;
  bool printed_waiting_;
  std::deque<uint8_t> rx_queue_;
};

extern "C" uint32_t rfio_dpi_status();
extern "C" uint32_t rfio_dpi_recv_u32();
extern "C" void rfio_dpi_send_u32(uint32_t value);
extern "C" void rfio_dpi_log_byte(uint32_t value);

#endif  // ARA_TB_VERILATOR_RFIO_SOCKET_H_
