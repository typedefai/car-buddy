#include "motor.h"

#include <stdlib.h>

#include "board_config.h"
#include "main.h"
#include "stm32f1xx_hal.h"

typedef struct {
  GPIO_TypeDef *port;
  uint16_t in1;
  uint16_t in2;
  uint32_t channel;
  int8_t invert;
} motor_channel_t;

static TIM_HandleTypeDef g_pwm_timer;
static TIM_HandleTypeDef g_encoder_timers[4];

static const motor_channel_t g_channels[4] = {
    {MOTOR_DIR_PORT, MOTOR_LF_IN1, MOTOR_LF_IN2, TIM_CHANNEL_1,
     MOTOR_LF_INVERT ? -1 : 1},
    {MOTOR_DIR_PORT, MOTOR_LR_IN1, MOTOR_LR_IN2, TIM_CHANNEL_2,
     MOTOR_LR_INVERT ? -1 : 1},
    {MOTOR_DIR_PORT, MOTOR_RF_IN1, MOTOR_RF_IN2, TIM_CHANNEL_3,
     MOTOR_RF_INVERT ? -1 : 1},
    {MOTOR_DIR_PORT, MOTOR_RR_IN1, MOTOR_RR_IN2, TIM_CHANNEL_4,
     MOTOR_RR_INVERT ? -1 : 1},
};

static const int8_t kEncoderSign[4] = {
    ENCODER_LF_SIGN < 0 ? -1 : 1,
    ENCODER_LR_SIGN < 0 ? -1 : 1,
    ENCODER_RF_SIGN < 0 ? -1 : 1,
    ENCODER_RR_SIGN < 0 ? -1 : 1,
};

static int16_t g_target_left = 0;
static int16_t g_target_right = 0;
static int16_t g_actual_left = 0;
static int16_t g_actual_right = 0;
static int16_t g_speed_left_mm_s = 0;
static int16_t g_speed_right_mm_s = 0;
static uint32_t g_last_count[4] = {0, 0, 0, 0};
static bool g_fault = false;
static uint32_t g_stall_ms = 0;

static int16_t clamp_speed(int32_t value) {
  if (value > 32767) {
    return 32767;
  }
  if (value < -32768) {
    return -32768;
  }
  return (int16_t)value;
}

static int16_t clamp_permille(int32_t value) {
  if (value > MOTOR_PERMILLE_MAX) {
    return MOTOR_PERMILLE_MAX;
  }
  if (value < -MOTOR_PERMILLE_MAX) {
    return -MOTOR_PERMILLE_MAX;
  }
  return (int16_t)value;
}

static void set_direction(const motor_channel_t *channel, int16_t permille) {
  if (permille > 0) {
    HAL_GPIO_WritePin(channel->port, channel->in1, GPIO_PIN_SET);
    HAL_GPIO_WritePin(channel->port, channel->in2, GPIO_PIN_RESET);
  } else if (permille < 0) {
    HAL_GPIO_WritePin(channel->port, channel->in1, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(channel->port, channel->in2, GPIO_PIN_SET);
  } else {
    HAL_GPIO_WritePin(channel->port, channel->in1, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(channel->port, channel->in2, GPIO_PIN_RESET);
  }
}

static void apply_channel(const motor_channel_t *channel, int16_t permille) {
  int16_t clamped = clamp_permille(permille);
  if (channel->invert < 0) {
    clamped = (int16_t)-clamped;
  }
  const uint32_t compare =
      (uint32_t)abs((int)clamped) * (MOTOR_PWM_PERIOD + 1u) / 1000u;
  set_direction(channel, clamped);
  __HAL_TIM_SET_COMPARE(&g_pwm_timer, channel->channel, compare);
}

static void set_standby(bool enabled) {
  HAL_GPIO_WritePin(MOTOR_STBY_PORT, MOTOR_STBY_LEFT,
                    enabled ? GPIO_PIN_SET : GPIO_PIN_RESET);
  HAL_GPIO_WritePin(MOTOR_STBY_PORT, MOTOR_STBY_RIGHT,
                    enabled ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

static int16_t slew(int16_t current, int16_t target) {
  if (target > current + MOTOR_SLEW_PERMILLE_PER_TICK) {
    return (int16_t)(current + MOTOR_SLEW_PERMILLE_PER_TICK);
  }
  if (target < current - MOTOR_SLEW_PERMILLE_PER_TICK) {
    return (int16_t)(current - MOTOR_SLEW_PERMILLE_PER_TICK);
  }
  return target;
}

static void update_speeds(uint32_t dt_ms) {
  if (dt_ms == 0) {
    return;
  }

  const uint32_t counts[4] = {
      __HAL_TIM_GET_COUNTER(&g_encoder_timers[0]),
      __HAL_TIM_GET_COUNTER(&g_encoder_timers[1]),
      __HAL_TIM_GET_COUNTER(&g_encoder_timers[2]),
      __HAL_TIM_GET_COUNTER(&g_encoder_timers[3]),
  };

  int32_t delta_left = 0;
  int32_t delta_right = 0;
  for (int i = 0; i < 4; ++i) {
    int16_t delta = (int16_t)(counts[i] - g_last_count[i]);
    delta = (int16_t)(delta * kEncoderSign[i]);
    g_last_count[i] = counts[i];
    if (i < 2) {
      delta_left += delta;
    } else {
      delta_right += delta;
    }
  }

#if CAR_ENCODER_COUNT == 4
  const int32_t left_denom =
      (int32_t)ENCODER_COUNTS_PER_REV * (int32_t)dt_ms * 2;
  const int32_t right_denom = left_denom;
#else
  const int32_t left_denom = (int32_t)ENCODER_COUNTS_PER_REV * (int32_t)dt_ms;
  const int32_t right_denom = left_denom;
#endif

  const int32_t left =
      (delta_left * (int32_t)WHEEL_CIRCUMFERENCE_MM * 1000) / left_denom;
  const int32_t right =
      (delta_right * (int32_t)WHEEL_CIRCUMFERENCE_MM * 1000) / right_denom;

  g_speed_left_mm_s = clamp_speed(left);
  g_speed_right_mm_s = clamp_speed(right);
}

static void configure_encoder(TIM_HandleTypeDef *timer, TIM_TypeDef *instance) {
  timer->Instance = instance;
  timer->Init.Prescaler = 0;
  timer->Init.CounterMode = TIM_COUNTERMODE_UP;
  timer->Init.Period = 0xFFFFu;
  timer->Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  timer->Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;

  TIM_Encoder_InitTypeDef encoder = {0};
  encoder.EncoderMode = TIM_ENCODERMODE_TI12;
  encoder.IC1Polarity = TIM_ICPOLARITY_RISING;
  encoder.IC1Selection = TIM_ICSELECTION_DIRECTTI;
  encoder.IC1Prescaler = TIM_ICPSC_DIV1;
  encoder.IC1Filter = 10;
  encoder.IC2Polarity = TIM_ICPOLARITY_RISING;
  encoder.IC2Selection = TIM_ICSELECTION_DIRECTTI;
  encoder.IC2Prescaler = TIM_ICPSC_DIV1;
  encoder.IC2Filter = 10;

  if (HAL_TIM_Encoder_Init(timer, &encoder) != HAL_OK) {
    Error_Handler();
  }
  if (HAL_TIM_Encoder_Start(timer, TIM_CHANNEL_ALL) != HAL_OK) {
    Error_Handler();
  }
}

static void configure_pwm(void) {
  g_pwm_timer.Instance = TIM1;
  g_pwm_timer.Init.Prescaler = 0;
  g_pwm_timer.Init.CounterMode = TIM_COUNTERMODE_UP;
  g_pwm_timer.Init.Period = MOTOR_PWM_PERIOD;
  g_pwm_timer.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  g_pwm_timer.Init.RepetitionCounter = 0;
  g_pwm_timer.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;

  if (HAL_TIM_PWM_Init(&g_pwm_timer) != HAL_OK) {
    Error_Handler();
  }

  TIM_OC_InitTypeDef oc = {0};
  oc.OCMode = TIM_OCMODE_PWM1;
  oc.Pulse = 0;
  oc.OCPolarity = TIM_OCPOLARITY_HIGH;
  oc.OCNPolarity = TIM_OCNPOLARITY_HIGH;
  oc.OCFastMode = TIM_OCFAST_DISABLE;
  oc.OCIdleState = TIM_OCIDLESTATE_RESET;
  oc.OCNIdleState = TIM_OCNIDLESTATE_RESET;

  for (uint32_t i = 0; i < 4; ++i) {
    const uint32_t channel = g_channels[i].channel;
    if (HAL_TIM_PWM_ConfigChannel(&g_pwm_timer, &oc, channel) != HAL_OK) {
      Error_Handler();
    }
    if (HAL_TIM_PWM_Start(&g_pwm_timer, channel) != HAL_OK) {
      Error_Handler();
    }
  }
}

void Motor_Init(void) {
  __HAL_RCC_GPIOD_CLK_ENABLE();

  GPIO_InitTypeDef gpio = {0};
  gpio.Pin = MOTOR_STBY_LEFT | MOTOR_STBY_RIGHT | MOTOR_LF_IN1 |
             MOTOR_LF_IN2 | MOTOR_LR_IN1 | MOTOR_LR_IN2 | MOTOR_RF_IN1 |
             MOTOR_RF_IN2 | MOTOR_RR_IN1 | MOTOR_RR_IN2;
  gpio.Mode = GPIO_MODE_OUTPUT_PP;
  gpio.Pull = GPIO_NOPULL;
  gpio.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(MOTOR_DIR_PORT, &gpio);

  set_standby(false);
  for (int i = 0; i < 4; ++i) {
    set_direction(&g_channels[i], 0);
  }

  configure_pwm();
  configure_encoder(&g_encoder_timers[0], TIM2);
  configure_encoder(&g_encoder_timers[1], TIM3);
  configure_encoder(&g_encoder_timers[2], TIM4);
  configure_encoder(&g_encoder_timers[3], TIM8);

  for (int i = 0; i < 4; ++i) {
    g_last_count[i] = __HAL_TIM_GET_COUNTER(&g_encoder_timers[i]);
  }

  g_target_left = 0;
  g_target_right = 0;
  g_actual_left = 0;
  g_actual_right = 0;
  g_speed_left_mm_s = 0;
  g_speed_right_mm_s = 0;
  g_fault = false;
  g_stall_ms = 0;

  // Start the independent watchdog last, after all init has completed.
  IWDG->KR = 0x5555u;
  IWDG->PR = IWDG_PRESCALER;
  IWDG->RLR = IWDG_RELOAD;
  while (IWDG->SR != 0u) {
  }
  IWDG->KR = 0xCCCCu;
  IWDG->KR = 0xAAAAu;
}

void Motor_SetTargets(int16_t left, int16_t right) {
  if (g_fault) {
    g_target_left = 0;
    g_target_right = 0;
    return;
  }
  g_target_left = clamp_permille(left);
  g_target_right = clamp_permille(right);
}

void Motor_Update(uint32_t dt_ms) {
  update_speeds(dt_ms);

  if (g_fault) {
    g_actual_left = 0;
    g_actual_right = 0;
    apply_channel(&g_channels[0], 0);
    apply_channel(&g_channels[1], 0);
    apply_channel(&g_channels[2], 0);
    apply_channel(&g_channels[3], 0);
    set_standby(false);
    return;
  }

  g_actual_left = slew(g_actual_left, g_target_left);
  g_actual_right = slew(g_actual_right, g_target_right);

  const bool moving = g_actual_left != 0 || g_actual_right != 0;
  set_standby(moving);

  apply_channel(&g_channels[0], g_actual_left);
  apply_channel(&g_channels[1], g_actual_left);
  apply_channel(&g_channels[2], g_actual_right);
  apply_channel(&g_channels[3], g_actual_right);

  const bool commanded =
      abs((int)g_actual_left) > STALL_DUTY_THRESHOLD ||
      abs((int)g_actual_right) > STALL_DUTY_THRESHOLD;
  const bool turning =
      abs((int)g_speed_left_mm_s) > STALL_SPEED_THRESHOLD_MM_S ||
      abs((int)g_speed_right_mm_s) > STALL_SPEED_THRESHOLD_MM_S;

  if (commanded && !turning) {
    g_stall_ms += dt_ms;
    if (g_stall_ms >= STALL_TIME_MS) {
      g_fault = true;
      Motor_Stop();
    }
  } else {
    g_stall_ms = 0;
  }
}

void Motor_Stop(void) {
  g_target_left = 0;
  g_target_right = 0;
  g_actual_left = 0;
  g_actual_right = 0;
  set_standby(false);
  for (int i = 0; i < 4; ++i) {
    set_direction(&g_channels[i], 0);
    __HAL_TIM_SET_COMPARE(&g_pwm_timer, g_channels[i].channel, 0);
  }
}

bool Motor_HasFault(void) { return g_fault; }

int16_t Motor_LeftSpeedMmS(void) { return g_speed_left_mm_s; }

int16_t Motor_RightSpeedMmS(void) { return g_speed_right_mm_s; }

void Motor_RefreshWatchdog(void) { IWDG->KR = 0xAAAAu; }

void HAL_TIM_PWM_MspInit(TIM_HandleTypeDef *htim) {
  if (htim->Instance != TIM1) {
    return;
  }

  __HAL_RCC_TIM1_CLK_ENABLE();
  __HAL_RCC_GPIOE_CLK_ENABLE();
  __HAL_RCC_AFIO_CLK_ENABLE();

  // TIM1 full remap to PE9/PE11/PE13/PE14. AFIO_MAPR's SWJ_CFG bits are
  // write-only, so set them explicitly (SWD on, JTAG off) in the same write.
  uint32_t mapr = AFIO->MAPR;
  mapr &= ~(AFIO_MAPR_SWJ_CFG | AFIO_MAPR_TIM1_REMAP);
  mapr |= AFIO_MAPR_SWJ_CFG_JTAGDISABLE | AFIO_MAPR_TIM1_REMAP_FULLREMAP;
  AFIO->MAPR = mapr;

  GPIO_InitTypeDef gpio = {0};
  gpio.Pin = GPIO_PIN_9 | GPIO_PIN_11 | GPIO_PIN_13 | GPIO_PIN_14;
  gpio.Mode = GPIO_MODE_AF_PP;
  gpio.Pull = GPIO_NOPULL;
  gpio.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(GPIOE, &gpio);
}

void HAL_TIM_Encoder_MspInit(TIM_HandleTypeDef *htim) {
  GPIO_InitTypeDef gpio = {0};
  gpio.Mode = GPIO_MODE_INPUT;
  gpio.Pull = GPIO_PULLUP;
  gpio.Speed = GPIO_SPEED_FREQ_HIGH;

  if (htim->Instance == TIM2) {
    __HAL_RCC_TIM2_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
    gpio.Pin = GPIO_PIN_0 | GPIO_PIN_1;
    HAL_GPIO_Init(GPIOA, &gpio);
  } else if (htim->Instance == TIM3) {
    __HAL_RCC_TIM3_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
    gpio.Pin = GPIO_PIN_6 | GPIO_PIN_7;
    HAL_GPIO_Init(GPIOA, &gpio);
  } else if (htim->Instance == TIM4) {
    __HAL_RCC_TIM4_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    gpio.Pin = GPIO_PIN_6 | GPIO_PIN_7;
    HAL_GPIO_Init(GPIOB, &gpio);
  } else if (htim->Instance == TIM8) {
    __HAL_RCC_TIM8_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    gpio.Pin = GPIO_PIN_6 | GPIO_PIN_7;
    HAL_GPIO_Init(GPIOC, &gpio);
  }
}
