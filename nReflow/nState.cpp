
#include "nState.h"


unsigned long ssr_previous_millis = 0;
unsigned long peak_duration_millis = 0;
unsigned long soak_duration_millis = 0;
unsigned long heater_1_update_millis = 0;
unsigned long heater_2_update_millis = 0;

//State Machine
State currentState  = Idle;
State previousState  = Idle;
bool stateChanged = false;

//PID setup in nReflow.ino
extern PID *PID_1;
extern PID_t heaterPID;

//Reflow settings
static ReflowProfile reflowProfile;//our only running profile

void SetupReflowProfile()
{
    //Leaded
    reflowProfile.soakTemp = 150;
    reflowProfile.soakDuration = 90;
    reflowProfile.peakTemp = 235;
    reflowProfile.peakDuration = 60;
    reflowProfile.rampUpRate = 2;
    reflowProfile.rampDownRate = -5;
}

//increase or decrease the Target Temp
//by the rate per second
void updateSetpoint(double ratePerSecond)
{
    unsigned long currentMillis = millis();
    
    if(currentMillis - ssr_previous_millis >= SSR_UPDATE_RATE)
    {
        unsigned long tick = currentMillis - ssr_previous_millis;
      
        ssr_previous_millis = currentMillis;
        
        Setpoint += ratePerSecond * tick;        
    }
}

//heat the oven to our soaking tempurature
void StateRampToSoak()
{
#ifdef DEBUG
    Serial.println("Ramp To Soak");
#endif
    if(stateChanged)
    {
        stateChanged = false;
        
        double degC = max6675->readCelsius();
        
        Output = 80;
        PID_1->SetMode(AUTOMATIC);
        PID_1->SetControllerDirection(DIRECT);
        PID_1->SetTunings(heaterPID.Kp, heaterPID.Ki, heaterPID.Kd);
        Setpoint = degC;
		
#ifdef SERVO_DOOR
		//close the door
		doorServo->write(SERVO_START_POSITION);
#endif
    }
    
    updateSetpoint(reflowProfile.rampUpRate);
    
    //transition
    if (Setpoint >= reflowProfile.soakTemp - 1)
    {
        currentState = Soak;
    }
       
}

//Soak our pcbs at a constant tempurature
//set by refloaw duration
void StateSoak()
{
#ifdef DEBUG
    Serial.println("Soak");
#endif
    if (stateChanged)
    {
        stateChanged = false;
        Setpoint = reflowProfile.soakTemp;
    }

    //check duration at peak
    unsigned long currentMillis = millis();

    if(currentMillis - soak_duration_millis >= reflowProfile.soakDuration)
    {
      soak_duration_millis = currentMillis;
      
      //done cooking now we cool
      currentState = RampUp;
    } 
}

//Get to peak temp quick as we can
void StateRampUp()
{
#ifdef DEBUG
    Serial.println("Ramp Up");
#endif
    if (stateChanged)
    {
        stateChanged = false;
    }
    
    //TODO
    updateSetpoint(reflowProfile.rampUpRate);
    
    if (Setpoint >= reflowProfile.peakTemp - 1)
    {
        Setpoint = reflowProfile.peakTemp;
        currentState = Peak;
    }
}

void StatePeak()
{
#ifdef DEBUG
    Serial.println("Peak");
#endif
    if (stateChanged)
    {
        stateChanged = false;
        Setpoint = reflowProfile.peakTemp;
        peak_duration_millis = millis();
    }

    //check duration at peak
    unsigned long currentMillis = millis();

    if(currentMillis - peak_duration_millis >= reflowProfile.peakDuration)
    {
      peak_duration_millis = currentMillis;
      
      //done cooking now we cool
      currentState = RampDown;
    } 
}

void StateRampDown()
{
    if (stateChanged)
    {
        stateChanged = false;
        PID_1->SetControllerDirection(REVERSE);
        //PID->SetTunings(fanPID.Kp, fanPID.Ki, fanPID.Kd);
        Setpoint = reflowProfile.peakTemp - 15; // get it all going with a bit of a kick! v sluggish here otherwise, too hot too long
		
#ifdef SERVO_DOOR
		//open the door - speed up cooldown 
		//user will need to configure how far to get the right tempurature gradient
		//to fast is bad
		//to slow is not good either
		doorServo->write(SERVO_END_POSITION);
#endif
    }
    
    //todo
    updateSetpoint(reflowProfile.rampDownRate);
    
    if (Setpoint <= SAFE_COOL_TEMP) 
    {
        currentState = CoolDown;
    }
}

void StateCoolDown()
{
#ifdef DEBUG
    Serial.println("Cool Down");
#endif
    if (stateChanged) {
        stateChanged = false;
        PID_1->SetControllerDirection(REVERSE);
        //PID->SetTunings(fanPID.Kp, fanPID.Ki, fanPID.Kd);
        Setpoint = SAFE_COOL_TEMP;
    }
    
    if (Input < (SAFE_COOL_TEMP + 5)) {
        currentState = Complete;
        PID_1->SetMode(MANUAL);
        Output = 0;
    }
}

void SetHeater()
{
    static uint8_t state = 0;
    static bool heater = false;
    
  static bool isHeating = false;
  
    //every 100ms
    //check duration at peak
    unsigned long currentMillis = millis();

    if(currentMillis - heater_1_update_millis >= SSR_UPDATE_RATE)
    {
        heater_1_update_millis = currentMillis;
      
        state += Output;
    
        if(state >= 100)
        {
            state -= 100;
            heater = false;
        }
        else
        {
          heater = true;
        }
    
        digitalWrite(HEATER_PIN_1, heater);   
    }

    if(currentMillis - heater_2_update_millis >= (SSR_UPDATE_RATE + SSR_OFFSET))
    {
        heater_2_update_millis = currentMillis;
        digitalWrite(HEATER_PIN_2, heater);
    }
  
  //if either heaters are on, set the led to on
  if(digitalRead(HEATER_PIN_2) | digitalRead(HEATER_PIN_1))
  {
    digitalWrite(HEATING_PIN,1);
  }
  else
  {
    digitalWrite(HEATING_PIN,0);
  }
}
