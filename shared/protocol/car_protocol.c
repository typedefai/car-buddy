#include "car_protocol.h"

#include <string.h>

enum {
  PARSER_WAIT_MAGIC0 = 0,
  PARSER_WAIT_MAGIC1 = 1,
  PARSER_HEADER = 2,
  PARSER_PAYLOAD = 3,
  PARSER_CRC_LOW = 4,
  PARSER_CRC_HIGH = 5,
};

static void parser_reset(car_parser_t *parser) {
  parser->state = PARSER_WAIT_MAGIC0;
  parser->index = 0;
  parser->payload_len = 0;
}

uint16_t car_protocol_crc16(const uint8_t *data, size_t len) {
  uint16_t crc = 0xFFFFu;
  for (size_t i = 0; i < len; ++i) {
    crc ^= (uint16_t)data[i] << 8;
    for (int bit = 0; bit < 8; ++bit) {
      if (crc & 0x8000u) {
        crc = (uint16_t)((crc << 1) ^ 0x1021u);
      } else {
        crc = (uint16_t)(crc << 1);
      }
    }
  }
  return crc;
}

void car_parser_init(car_parser_t *parser) {
  if (parser == NULL) {
    return;
  }
  parser_reset(parser);
}

static bool parser_finish(car_parser_t *parser, car_frame_t *out) {
  const uint8_t payload_len = parser->payload_len;
  const uint16_t expected_crc = car_protocol_crc16(
      &parser->buffer[2], (CAR_PROTOCOL_HEADER_SIZE - 2u) + payload_len);
  const uint16_t received_crc =
      (uint16_t)(parser->buffer[parser->index - 2u] |
                 ((uint16_t)parser->buffer[parser->index - 1u] << 8));

  if (expected_crc != received_crc) {
    parser_reset(parser);
    return false;
  }

  out->version = parser->buffer[2];
  out->type = parser->buffer[3];
  out->seq = (uint16_t)(parser->buffer[4] | (parser->buffer[5] << 8));
  out->timestamp_ms =
      (uint32_t)parser->buffer[6] |
      ((uint32_t)parser->buffer[7] << 8) |
      ((uint32_t)parser->buffer[8] << 16) |
      ((uint32_t)parser->buffer[9] << 24);
  out->payload_len = payload_len;
  if (payload_len > 0) {
    memcpy(out->payload, &parser->buffer[CAR_PROTOCOL_HEADER_SIZE],
           payload_len);
  }
  parser_reset(parser);
  return true;
}

bool car_parser_push(car_parser_t *parser, uint8_t byte, car_frame_t *out) {
  if (parser == NULL || out == NULL) {
    return false;
  }

  switch (parser->state) {
    case PARSER_WAIT_MAGIC0:
      if (byte == CAR_PROTOCOL_MAGIC0) {
        parser->buffer[0] = byte;
        parser->index = 1;
        parser->state = PARSER_WAIT_MAGIC1;
      }
      break;

    case PARSER_WAIT_MAGIC1:
      if (byte == CAR_PROTOCOL_MAGIC1) {
        parser->buffer[1] = byte;
        parser->index = 2;
        parser->state = PARSER_HEADER;
      } else if (byte == CAR_PROTOCOL_MAGIC0) {
        parser->buffer[0] = byte;
        parser->index = 1;
      } else {
        parser_reset(parser);
      }
      break;

    case PARSER_HEADER:
      if (parser->index >= CAR_PROTOCOL_MAX_FRAME) {
        parser_reset(parser);
        break;
      }
      parser->buffer[parser->index++] = byte;
      if (parser->index == CAR_PROTOCOL_HEADER_SIZE) {
        if (parser->buffer[2] != CAR_PROTOCOL_VERSION) {
          parser_reset(parser);
          break;
        }
        parser->payload_len = parser->buffer[10];
        if (parser->payload_len > CAR_PROTOCOL_MAX_PAYLOAD) {
          parser_reset(parser);
          break;
        }
        parser->state = parser->payload_len == 0 ? PARSER_CRC_LOW
                                                 : PARSER_PAYLOAD;
      }
      break;

    case PARSER_PAYLOAD:
      parser->buffer[parser->index++] = byte;
      if (parser->index ==
          CAR_PROTOCOL_HEADER_SIZE + parser->payload_len) {
        parser->state = PARSER_CRC_LOW;
      }
      break;

    case PARSER_CRC_LOW:
      parser->buffer[parser->index++] = byte;
      parser->state = PARSER_CRC_HIGH;
      break;

    case PARSER_CRC_HIGH:
      parser->buffer[parser->index++] = byte;
      return parser_finish(parser, out);

    default:
      parser_reset(parser);
      break;
  }

  return false;
}

size_t car_protocol_encode(uint8_t type, uint16_t seq, uint32_t timestamp_ms,
                           const void *payload, uint8_t payload_len,
                           uint8_t *out, size_t out_capacity) {
  if (out == NULL || payload_len > CAR_PROTOCOL_MAX_PAYLOAD) {
    return 0;
  }
  if (payload_len > 0 && payload == NULL) {
    return 0;
  }

  const size_t frame_len =
      CAR_PROTOCOL_HEADER_SIZE + payload_len + CAR_PROTOCOL_CRC_SIZE;
  if (out_capacity < frame_len) {
    return 0;
  }

  out[0] = CAR_PROTOCOL_MAGIC0;
  out[1] = CAR_PROTOCOL_MAGIC1;
  out[2] = CAR_PROTOCOL_VERSION;
  out[3] = type;
  out[4] = (uint8_t)(seq & 0xFFu);
  out[5] = (uint8_t)(seq >> 8);
  out[6] = (uint8_t)(timestamp_ms & 0xFFu);
  out[7] = (uint8_t)((timestamp_ms >> 8) & 0xFFu);
  out[8] = (uint8_t)((timestamp_ms >> 16) & 0xFFu);
  out[9] = (uint8_t)((timestamp_ms >> 24) & 0xFFu);
  out[10] = payload_len;
  if (payload_len > 0) {
    memcpy(&out[CAR_PROTOCOL_HEADER_SIZE], payload, payload_len);
  }

  const uint16_t crc = car_protocol_crc16(
      &out[2], (CAR_PROTOCOL_HEADER_SIZE - 2u) + payload_len);
  out[CAR_PROTOCOL_HEADER_SIZE + payload_len] = (uint8_t)(crc & 0xFFu);
  out[CAR_PROTOCOL_HEADER_SIZE + payload_len + 1] = (uint8_t)(crc >> 8);

  return frame_len;
}
