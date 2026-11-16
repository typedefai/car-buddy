#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "car_protocol.h"

static void test_round_trip(void) {
  car_setpoint_t setpoint = {
      .throttle = 500,
      .steering = -250,
      .flags = CAR_SETPOINT_ENABLE,
      .reserved = 0,
  };
  uint8_t buffer[CAR_PROTOCOL_MAX_FRAME];
  const size_t heartbeat_len =
      car_protocol_encode(CAR_MSG_HEARTBEAT, 1, 123, NULL, 0, buffer,
                          sizeof(buffer));
  assert(heartbeat_len ==
         CAR_PROTOCOL_HEADER_SIZE + CAR_PROTOCOL_CRC_SIZE);
  assert(car_protocol_encode(CAR_MSG_SETPOINT, 1, 123, NULL, 1, buffer,
                             sizeof(buffer)) == 0);

  const size_t frame_len =
      car_protocol_encode(CAR_MSG_SETPOINT, 42, 123456, &setpoint,
                          sizeof(setpoint), buffer, sizeof(buffer));
  assert(frame_len == CAR_PROTOCOL_HEADER_SIZE + sizeof(setpoint) +
                          CAR_PROTOCOL_CRC_SIZE);

  car_parser_t parser;
  car_parser_init(&parser);
  car_frame_t frame;
  bool complete = false;
  for (size_t i = 0; i < frame_len; ++i) {
    complete = car_parser_push(&parser, buffer[i], &frame);
  }

  assert(complete);
  assert(frame.version == CAR_PROTOCOL_VERSION);
  assert(frame.type == CAR_MSG_SETPOINT);
  assert(frame.seq == 42);
  assert(frame.timestamp_ms == 123456);
  assert(frame.payload_len == sizeof(setpoint));

  car_setpoint_t decoded;
  memcpy(&decoded, frame.payload, sizeof(decoded));
  assert(decoded.throttle == setpoint.throttle);
  assert(decoded.steering == setpoint.steering);
  assert(decoded.flags == setpoint.flags);
}

static void test_rejects_bad_crc(void) {
  car_telemetry_t telemetry = {
      .uptime_ms = 9876,
      .left_speed_mm_s = 100,
      .right_speed_mm_s = -100,
      .battery_mv = 7400,
      .state = 1,
      .fault = 0,
  };
  uint8_t buffer[CAR_PROTOCOL_MAX_FRAME];
  const size_t frame_len =
      car_protocol_encode(CAR_MSG_TELEMETRY, 7, 1000, &telemetry,
                          sizeof(telemetry), buffer, sizeof(buffer));
  assert(frame_len > 0);

  buffer[frame_len - 1] ^= 0xFF;

  car_parser_t parser;
  car_parser_init(&parser);
  car_frame_t frame;
  bool complete = false;
  for (size_t i = 0; i < frame_len; ++i) {
    complete = car_parser_push(&parser, buffer[i], &frame);
  }
  assert(!complete);
}

static void test_ignores_garbage_and_restarts(void) {
  car_setpoint_t setpoint = {
      .throttle = 1,
      .steering = 2,
      .flags = CAR_SETPOINT_ENABLE,
      .reserved = 0,
  };
  uint8_t buffer[CAR_PROTOCOL_MAX_FRAME];
  const size_t frame_len =
      car_protocol_encode(CAR_MSG_SETPOINT, 1, 2, &setpoint,
                          sizeof(setpoint), buffer, sizeof(buffer));

  car_parser_t parser;
  car_parser_init(&parser);
  car_frame_t frame;
  assert(!car_parser_push(&parser, 0x00, &frame));
  assert(!car_parser_push(&parser, 0xAA, &frame));
  assert(!car_parser_push(&parser, 0x00, &frame));

  bool complete = false;
  for (size_t i = 0; i < frame_len; ++i) {
    complete = car_parser_push(&parser, buffer[i], &frame);
  }
  assert(complete);
  assert(frame.type == CAR_MSG_SETPOINT);
}

int main(void) {
  test_round_trip();
  test_rejects_bad_crc();
  test_ignores_garbage_and_restarts();
  puts("car_protocol tests passed");
  return 0;
}
