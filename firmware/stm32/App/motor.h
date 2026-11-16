#ifndef CAR_BUDDY_MOTOR_H_
#define CAR_BUDDY_MOTOR_H_

#include <stdbool.h>
#include <stdint.h>

void Motor_Init(void);

// Targets are permille in [-1000, 1000]. Positive = forward.
void Motor_SetTargets(int16_t left, int16_t right);

// Runs one control tick. Call every MOTOR_CONTROL_PERIOD_MS.
void Motor_Update(uint32_t dt_ms);

// Immediate safe stop: PWM off, direction low, TB6612 STBY low.
void Motor_Stop(void);

bool Motor_HasFault(void);
int16_t Motor_LeftSpeedMmS(void);
int16_t Motor_RightSpeedMmS(void);

void Motor_RefreshWatchdog(void);

#endif  // CAR_BUDDY_MOTOR_H_
