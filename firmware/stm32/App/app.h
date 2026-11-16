#ifndef CAR_BUDDY_APP_H_
#define CAR_BUDDY_APP_H_

#include <stdbool.h>
#include <stdint.h>

#include "car_protocol.h"

typedef enum {
  APP_STATE_BOOT = 0,
  APP_STATE_IDLE = 1,
  APP_STATE_READY = 2,
  APP_STATE_ESTOP = 3,
} app_state_t;

void App_Init(void);
void App_Loop(void);

app_state_t App_GetState(void);
bool App_LinkAlive(void);
const car_setpoint_t *App_GetSetpoint(void);

#endif  // CAR_BUDDY_APP_H_
