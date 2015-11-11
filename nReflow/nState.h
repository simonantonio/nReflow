
#ifndef _NSTATE_H_
#define _NSTATE_H_

#include <PID_v1.h>
#include <max6675.h>

#if ARDUINO >= 100
  #include "Arduino.h"
#else
  #include "WProgram.h"
#endif

#include "nTypes.h"
#include "config.h"

void SetupReflowProfile();
void updateSetpoint(double ratePerSecond);
void StateRampToSoak();
void StateSoak();
void StateRampUp();
void StatePeak();
void StateRampDown();
void StateCoolDown();
void SetHeater();

extern MAX6675 *max6675;
extern uint8_t HeaterValue;
extern double Setpoint;
extern double Input;
extern double Output;

#endif //_NSTATE_H_
