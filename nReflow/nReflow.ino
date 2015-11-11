//TODO add safties
// - check if tempurature actually got near our targets
// - PID tune for individual oven

#include <PID_v1.h>
#include <max6675.h>

#include "config.h"
#include "nTypes.h"
#include "nState.h"

/*
* 
* SPI for MAX6675 Temp sensor
* PWM x2 for SSR
* SPI for LCD	- TODO
* Button for Start/Stop - ADD DEBOUNCE
* SD Card or Wifi? - TODO
*
*/
const char* version = "v0.1";

MAX6675 *max6675;

volatile bool IsRunning;

extern State currentState;
extern State previousState;
extern bool stateChanged;


unsigned long ui_previous_millis = 0;
unsigned long button_last_micros = 0;

//PID
uint8_t HeaterValue;
double Setpoint;
double Input;
double Output;


PID_t heaterPID = { 4.00, 0.05,  2.00 };

PID *PID_1;



void setup() {
    
    pinMode(BUTTON_PIN, INPUT);
	//button should be pulled low with a 10k
    attachInterrupt(BUTTON_PIN, runModeToggle, RISING);
    
	//make sure we have the elements turned off
    pinMode(HEATER_PIN_1, OUTPUT);
	digitalWrite(HEATER_PIN_1, 0);
    pinMode(HEATER_PIN_2, OUTPUT);
    digitalWrite(HEATER_PIN_2, 0);
	
	//LEDS
	pinMode(IDLE_PIN, OUTPUT);
	pinMode(HEATING_PIN, OUTPUT);
	pinMode(RUNNING_PIN, OUTPUT);
	pinMode(ERROR_PIN, OUTPUT);
	pinMode(HOT_PIN, OUTPUT);
	
	//turn them all off
	digitalWrite(IDLE_PIN, 0);
	digitalWrite(HEATING_PIN, 0);
	digitalWrite(RUNNING_PIN, 0);
	digitalWrite(ERROR_PIN, 0);
	digitalWrite(HOT_PIN, 0);
	
#ifdef SERVO_DOOR
	pinMode(SERVO_PIN, OUTPUT);
	doorServo = new Servo();
	doorServo->attach(SERVO_PIN);
#endif
	
    max6675 = new MAX6675(D6, TEMP_CS, D5);
    IsRunning = false;
    
    PID_1 = new PID(&Input, &Output, &Setpoint, heaterPID.Kp, heaterPID.Ki, heaterPID.Kd, DIRECT);
    
    PID_1->SetOutputLimits(0, 100); // max output 100%
    PID_1->SetMode(AUTOMATIC);

    SetupReflowProfile();
    
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
    
    //if we are in a mode that requires heating 
	//set the value, otherwise turn off the heaters
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
	
	Serial.print("Input:");
    Serial.println(Input);
	
	Serial.print("Output:");
    Serial.println(Output);
	
	Serial.print("Heater Value:");
    Serial.println(HeaterValue);
	
    #endif
	
	//Status LEDS

	//ERROR_PIN
	if(degC >SAFE_COOL_TEMP)
	{
		//hot
		digitalWrite(HOT_PIN,1);
	}

	switch(currentState)
	{
		case Idle:
			digitalWrite(IDLE_PIN, 1);
#ifdef SERVO_DOOR
		//keep door servo in closed position
		doorServo->write(SERVO_START_POSITION);
#endif
			break;
		case RampToSoak:
		case Soak:
		case RampUp:
		case Peak:
		case RampDown:
			digitalWrite(RUNNING_PIN,1);
			break;
		default:
			digitalWrite(IDLE_PIN,0);
			digitalWrite(RUNNING_PIN,0);
	}
}

void runModeToggle(void)
{
	if((long)(micros() - button_last_micros) >= 15 * 1000) {
		IsRunning != IsRunning;//toggle
		
		if(IsRunning)
		{
			currentState = RampToSoak;
		}
		else
		{
			currentState = Idle;
		}
		button_last_micros = micros();
	}	
}



