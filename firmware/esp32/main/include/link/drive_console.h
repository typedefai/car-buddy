#ifndef CAR_BUDDY_DRIVE_CONSOLE_H_
#define CAR_BUDDY_DRIVE_CONSOLE_H_

// Simple UART0 console for bench-driving the car without the server:
//   w/forward, x/reverse, a/left, d/right, s/stop,
//   +/- speed step, e/estop, c/clear, h/help.
void DriveConsole_Start();

#endif  // CAR_BUDDY_DRIVE_CONSOLE_H_
