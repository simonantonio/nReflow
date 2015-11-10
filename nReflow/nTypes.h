
#ifndef _NTYPES_H_
#define _NTYPES_H_

// data type for the values used in the reflow profile
typedef struct ReflowProfileValues {
  int16_t soakTemp;
  int16_t soakDuration;
  int16_t peakTemp;
  int16_t peakDuration;
  double  rampUpRate;
  double  rampDownRate;
  uint8_t checksum;
} ReflowProfile;

typedef enum {
  None     = 0,
  Idle     = 1,
  
  RampToSoak = 10,
  Soak,
  RampUp,
  Peak,
  RampDown,
  CoolDown,

  Complete = 20,
  
} State;

typedef struct {
  double Kp;
  double Ki;
  double Kd;
} PID_t;

#endif
