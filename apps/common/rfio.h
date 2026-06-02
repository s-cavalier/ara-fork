#ifndef ARA_RFIO_H_
#define ARA_RFIO_H_

#include <stdint.h>

#define RFIO_RX_DATA_ADDR 0xC0000100UL
#define RFIO_TX_DATA_ADDR 0xC0000104UL
#define RFIO_STATUS_ADDR  0xC0000108UL

#define RFIO_STATUS_RX_VALID  0x1u
#define RFIO_STATUS_TX_READY  0x2u
#define RFIO_STATUS_CONNECTED 0x4u

#define RFIO_RX_DATA (*(volatile uint32_t *)RFIO_RX_DATA_ADDR)
#define RFIO_TX_DATA (*(volatile uint32_t *)RFIO_TX_DATA_ADDR)
#define RFIO_STATUS  (*(volatile uint32_t *)RFIO_STATUS_ADDR)

static inline uint32_t rfio_status(void) { return RFIO_STATUS; }

static inline uint32_t rfio_read_u32(void) {
  while ((rfio_status() & RFIO_STATUS_RX_VALID) == 0) {
  }
  return RFIO_RX_DATA;
}

static inline void rfio_write_u32(uint32_t value) {
  while ((rfio_status() & RFIO_STATUS_TX_READY) == 0) {
  }
  RFIO_TX_DATA = value;
}

static inline float rfio_read_f32(void) {
  uint32_t bits = rfio_read_u32();
  float value;
  __builtin_memcpy(&value, &bits, sizeof(value));
  return value;
}

static inline void rfio_write_f32(float value) {
  uint32_t bits;
  __builtin_memcpy(&bits, &value, sizeof(bits));
  rfio_write_u32(bits);
}

#endif  // ARA_RFIO_H_
