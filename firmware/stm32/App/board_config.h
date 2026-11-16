#ifndef CAR_BUDDY_BOARD_CONFIG_H_
#define CAR_BUDDY_BOARD_CONFIG_H_

// STM32F103ZET6 + two TB6612FNG boards (4 driven wheels, skid steer).
//
// Pin map (all avoid USART1/USART2/SWD/HSE):
//   TIM1 partial remap PWM:
//     PE9  TIM1_CH1 -> TB6612 #1 PWMA (left front)
//     PE11 TIM1_CH2 -> TB6612 #1 PWMB (left rear)
//     PE13 TIM1_CH3 -> TB6612 #2 PWMA (right front)
//     PE14 TIM1_CH4 -> TB6612 #2 PWMB (right rear)
//
//   TB6612 #1 (left):  AIN1=PD4 AIN2=PD5  BIN1=PD6 BIN2=PD7  STBY=PD2
//   TB6612 #2 (right): AIN1=PD8 AIN2=PD9  BIN1=PD10 BIN2=PD11 STBY=PD3
//
//   Encoders (quadrature, timer encoder mode):
//     left front  A=PA0 B=PA1 -> TIM2
//     left rear   A=PA6 B=PA7 -> TIM3
//     right front A=PB6 B=PB7 -> TIM4
//     right rear  A=PC6 B=PC7 -> TIM8
//
//   ESP32: PA2 USART2_TX -> ESP32 GPIO21, PA3 USART2_RX <- ESP32 GPIO14
//   Debug: PA9 USART1_TX, PA10 USART1_RX
//   SWD:   PA13 SWDIO, PA14 SWCLK
//   HSE:   OSC_IN / OSC_OUT (8 MHz)

#define MOTOR_DIR_PORT GPIOD
#define MOTOR_STBY_PORT GPIOD

#define MOTOR_LF_IN1 GPIO_PIN_4
#define MOTOR_LF_IN2 GPIO_PIN_5
#define MOTOR_LR_IN1 GPIO_PIN_6
#define MOTOR_LR_IN2 GPIO_PIN_7
#define MOTOR_RF_IN1 GPIO_PIN_8
#define MOTOR_RF_IN2 GPIO_PIN_9
#define MOTOR_RR_IN1 GPIO_PIN_10
#define MOTOR_RR_IN2 GPIO_PIN_11

#define MOTOR_STBY_LEFT GPIO_PIN_2
#define MOTOR_STBY_RIGHT GPIO_PIN_3

#define MOTOR_PWM_PERIOD 3599u  // 72 MHz / (3599 + 1) = 20 kHz
#define MOTOR_PERMILLE_MAX 1000
#define MOTOR_SLEW_PERMILLE_PER_TICK 25
#define MOTOR_CONTROL_PERIOD_MS 10u

// Encoder/mechanics. Adjust these to the actual motors and wheels.
#define CAR_ENCODER_COUNT 4
#define ENCODER_COUNTS_PER_REV 1320  // 11 PPR x 30 gearbox x 4 edges
#define WHEEL_CIRCUMFERENCE_MM 204   // 65 mm wheel
#define STALL_DUTY_THRESHOLD 300
#define STALL_SPEED_THRESHOLD_MM_S 20
#define STALL_TIME_MS 1000u

// Flip these if a motor spins the wrong way or an encoder counts backwards.
#define MOTOR_LF_INVERT 0
#define MOTOR_LR_INVERT 0
#define MOTOR_RF_INVERT 0
#define MOTOR_RR_INVERT 0
#define ENCODER_LF_SIGN 1
#define ENCODER_LR_SIGN 1
#define ENCODER_RF_SIGN 1
#define ENCODER_RR_SIGN 1

// IWDG: LSI ~40 kHz, /64 -> 625 Hz, reload 1250 -> ~2 s.
#define IWDG_PRESCALER 4u
#define IWDG_RELOAD 1250u

#endif  // CAR_BUDDY_BOARD_CONFIG_H_
