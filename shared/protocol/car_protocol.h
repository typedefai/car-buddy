#ifndef CAR_PROTOCOL_H_
#define CAR_PROTOCOL_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Versioned, CRC-protected frames shared by the ESP32, STM32, and server.
//
// Wire layout (little-endian):
//   0      0xAA
//   1      0x55
//   2      version
//   3      type
//   4..5   sequence (u16)
//   6..9   timestamp_ms (u32, monotonic)
//   10     payload_len (0..CAR_PROTOCOL_MAX_PAYLOAD)
//   11..   payload
//   last 2 CRC16-CCITT over version..payload
//
// The parser rejects bad magic, bad version, oversized payloads, and bad CRC.
#define CAR_PROTOCOL_VERSION 1u
#define CAR_PROTOCOL_MAGIC0 0xAAu
#define CAR_PROTOCOL_MAGIC1 0x55u
#define CAR_PROTOCOL_MAX_PAYLOAD 32u
#define CAR_PROTOCOL_HEADER_SIZE 11u
#define CAR_PROTOCOL_CRC_SIZE 2u
#define CAR_PROTOCOL_MAX_FRAME                                       \
  (CAR_PROTOCOL_HEADER_SIZE + CAR_PROTOCOL_MAX_PAYLOAD +             \
   CAR_PROTOCOL_CRC_SIZE)

typedef enum {
  CAR_MSG_HEARTBEAT = 1,
  CAR_MSG_SETPOINT = 2,
  CAR_MSG_TELEMETRY = 3,
  CAR_MSG_ACK = 4,
  CAR_MSG_ERROR = 5,
} car_msg_type_t;

typedef struct {
  uint8_t version;
  uint8_t type;
  uint16_t seq;
  uint32_t timestamp_ms;
  uint8_t payload_len;
  uint8_t payload[CAR_PROTOCOL_MAX_PAYLOAD];
} car_frame_t;

// ESP32 -> STM32 motion command. Values are permille, clamped by the STM32.
typedef struct {
  int16_t throttle;  // -1000..1000
  int16_t steering;  // -1000..1000
  uint8_t flags;
  uint8_t reserved;
} car_setpoint_t;

#define CAR_SETPOINT_ENABLE 0x01u
#define CAR_SETPOINT_ESTOP 0x02u

typedef struct {
  uint32_t uptime_ms;
  int16_t left_speed_mm_s;
  int16_t right_speed_mm_s;
  uint16_t battery_mv;
  uint8_t state;
  uint8_t fault;
} car_telemetry_t;

typedef struct {
  uint8_t state;
  uint8_t index;
  uint8_t payload_len;
  uint8_t buffer[CAR_PROTOCOL_MAX_FRAME];
} car_parser_t;

void car_parser_init(car_parser_t *parser);

// Feeds one byte. Returns true when a complete, valid frame was parsed.
bool car_parser_push(car_parser_t *parser, uint8_t byte, car_frame_t *out);

// Encodes a frame into out. Returns the frame length, or 0 if it does not fit.
size_t car_protocol_encode(uint8_t type, uint16_t seq, uint32_t timestamp_ms,
                           const void *payload, uint8_t payload_len,
                           uint8_t *out, size_t out_capacity);

uint16_t car_protocol_crc16(const uint8_t *data, size_t len);

#ifdef __cplusplus
}
#endif

#endif  // CAR_PROTOCOL_H_
