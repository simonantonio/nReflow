#include <PID_v1.h>
#include <max6675.h>

#include "nTypes.h"


/*
* Pin Defines
*
* SPI for MAX6675 Temp sensor
* MOSI - D4
* MISO - D5
* SCK - D6
* CS - D7
*
* PWM x2 for SSR
*
* SPI for LCD
*
* Button for Start/Stop
*
* SD Card or Wifi? - TODO
*
*/
const char* version = "v0.1";

#define DEBUG

#define HEATER_PIN_1 D10
#define HEATER_PIN_2 D11

#define TEMP_CS D7

#define BUTTON_PIN D33

//safe temp to leave to cool naturally
#define SAFE_COOL_TEMP 50

MAX6675 *max6675;

volatile bool IsRunning;

//settings
#define UI_UPDATE_RATE 1000
#define SSR_UPDATE_RATE 100
#define SSR_OFFSET 50

unsigned long ui_previous_millis = 0;
unsigned long ssr_previous_millis = 0;
unsigned long peak_duration_millis = 0;
unsigned long soak_duration_millis = 0;
unsigned long heater_1_update_millis = 0;
unsigned long heater_2_update_millis = 0;
//PID
uint8_t HeaterValue;
double Setpoint;
double Input;
double Output;



PID_t heaterPID = { 4.00, 0.05,  2.00 };

PID *PID_1;

//Reflow settings
static ReflowProfile reflowProfile;//our only running profile

//State Machine
State currentState  = Idle;
State previousState  = Idle;
bool stateChanged = false;

void setup() {
    
    pinMode(BUTTON_PIN, INPUT);
    attachInterrupt(BUTTON_PIN, runModeToggle, RISING);
    
    pinMode(HEATER_PIN_1, OUTPUT);
    pinMode(HEATER_PIN_2, OUTPUT);
    
    max6675 = new MAX6675(D6, TEMP_CS, D5);
    IsRunning = false;
    
    PID_1 = new PID(&Input, &Output, &Setpoint, heaterPID.Kp, heaterPID.Ki, heaterPID.Kd, DIRECT);
    
    PID_1->SetOutputLimits(0, 100); // max output 100%
    PID_1->SetMode(AUTOMATIC);
    
    //Leaded
    reflowProfile.soakTemp = 150;
    reflowProfile.soakDuration = 90;
    reflowProfile.peakTemp = 235;
    reflowProfile.peakDuration = 60;
    reflowProfile.rampUpRate = 2;
    reflowProfile.rampDownRate = -5;
    
    delay(2000);//snooze
    
    #ifdef DEBUG
    Serial.print("nReflow version: ");
    Serial.println(version);
    
    #endif
}

void loop() {
    
    if(!IsRunning)
    return;
    
    ProcessUI();
    ProcessStateMachine();

    //Update PID input variable
    //This is our actual oven tempurature
    double degC = max6675->readCelsius();
    Input = degC;

    //Set our Output value
    //How hard to drive our heaters
    PID_1->Compute();
    
    // decides which control signal is fed to the output for this cycle
    if (   currentState != RampDown
        && currentState != CoolDown
        && currentState != Complete
        && currentState != Idle)
    {
        HeaterValue = Output;
    }
    else {
        HeaterValue = 0;
    }
    
    //Set heater state (to SSRs)
    //0 - 100 target 
    SetHeater();
}

void ProcessStateMachine()
{    
    //move to ProcessStateMachine
    switch(currentState)
    {
        case RampToSoak:
          StateRampToSoak();           
          break;
        case Soak:
          StateSoak();        
          break;   
        case RampUp:
          StateRampUp();
          break;
        case Peak:
          StatePeak();
          break;
        case RampDown:
          StateRampDown();
          break;
        case CoolDown:
          StateCoolDown();
          break;
    }

    if (currentState != previousState) 
    {
        stateChanged = true;
        previousState = currentState;
    }
}

void ProcessUI()
{
    unsigned long currentMillis = millis();
    
    if(currentMillis - ui_previous_millis >= UI_UPDATE_RATE)
    {
        ui_previous_millis = currentMillis;
        
        UpdateUI();
    }
}

void UpdateUI()
{
    double degC = max6675->readCelsius();
  
    #ifdef DEBUG
    Serial.println("Updating UI");

    Serial.print("Temp C:");
    Serial.println(degC);

    Serial.print("Setpoint:");
    Serial.println(Setpoint);
    #endif
}

void runModeToggle(void)
{
    //TODO - debounce
    IsRunning != IsRunning;//toggle
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
        digitalWrite(HEATER_PIN_1, heater);
    }
}
