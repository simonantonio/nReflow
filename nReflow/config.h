
#ifndef _CONFIG_H_
#define _CONFIG_H_


#define DEBUG

//#define SERVO_DOOR 
#ifdef SERVO_DOOR

#include <Servo.h>

#define SERVO_PIN D17	
#define SERVO_START_POSITION 0
//halfway
#define SERVO_END_POSITION 90

Servo *doorServo;

#endif

#define HEATER_PIN_1 D10
#define HEATER_PIN_2 D11

#define IDLE_PIN D19
#define HEATING_PIN	D20
#define RUNNING_PIN D21
#define ERROR_PIN D22
#define HOT_PIN D18

#define TEMP_CS D7

#define BUTTON_PIN D33

//safe temp to leave to cool naturally
#define SAFE_COOL_TEMP 50

//settings
#define UI_UPDATE_RATE 1000

//Update rate for heaters once every 250ms
//as the PID only updates once every 200ms
#define SSR_UPDATE_RATE 250
#define SSR_OFFSET 50

#endif //_CONFIG_H_
