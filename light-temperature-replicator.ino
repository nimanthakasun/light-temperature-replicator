#include <Preferences.h>
#include "DFRobot_ColorTemperature.h"
#if defined(ARDUINO_AVR_UNO)||defined(ESP8266)
#include <SoftwareSerial.h>
#endif

DFRobot_ColorTemperature CT(/*pWire = */&Wire);

// PWM Configuration
const int LEDW = 33;
const int LEDY = 32;
const int freq = 15000;
const int resolution = 8;

// Input configuratio
const int saveSwitch = 14;

//Senssor Values
int mappedTemp = 0;
int mappedTempComp = 0;

unsigned long previousMillis = 0;
unsigned long switchDuration = 0;

/* -------------------- BUTTON CONFIG -------------------- */
#define LONG_PRESS_TIME 5000   // 5 seconds

/* -------------------- SENSOR CONFIG -------------------- */
#define SENSOR_SAMPLE_COUNT 20

/* -------------------- STORAGE -------------------- */
Preferences preferences;
const char* PREF_NAMESPACE = "sensor";
const char* PREF_KEY = "avgValue";

/* -------------------- VARIABLES -------------------- */
unsigned int storedSensorValue = 0;

bool buttonPressed = false;
unsigned long buttonPressStart = 0;
 
void setup(){
  // configure LED PWM
  ledcAttach(LEDW, freq, resolution);
  ledcAttach(LEDY, freq, resolution);

  // Initialize Serial
  Serial.begin(115200);

  //Initialize Color sensor
  while(CT.begin() != 0){
    Serial.println(" Sensor initialize failed!!");
    delay(1000);
  }
  Serial.println(" Sensor  initialize success!!");

  //Initialize save switch pin
  pinMode(saveSwitch, INPUT_PULLUP); 

  storedSensorValue = loadFromEEPROM();
  applyPWM(storedSensorValue);
}
 
void loop(){   
  handleButton();
  delay(50);
}

void handleButton()
{
  bool currentState = digitalRead(saveSwitch) == LOW;

  if (currentState && !buttonPressed)
  {
    // Button just pressed
    buttonPressed = true;
    buttonPressStart = millis();
  }

  if (!currentState && buttonPressed)
  {
    // Button released
    buttonPressed = false;

    unsigned long pressDuration = millis() - buttonPressStart;

    if (pressDuration >= LONG_PRESS_TIME)
    {
      //showMessage("Long Press Detected", "Capturing...");
      unsigned int avg = captureAverageSensor();
      storedSensorValue = avg;
      //showMessage("Saving to EEPROM", String(avg));
      saveToEEPROM(avg);
      delay(1000);
      applyPWM(storedSensorValue);
    }
  }

  // Optional progress feedback
  // if (buttonPressed)
  // {
  //   unsigned long heldTime = millis() - buttonPressStart;
  //   if (heldTime < LONG_PRESS_TIME)
  //   {
  //         //showMessage("Hold Button...",String((LONG_PRESS_TIME - heldTime) / 1000.0, 1) + "s left");
  //   }
  // }
}

uint16_t retrieveSensorData(){
   return CT.readCCT();
}

uint32_t calculatePWM(uint16_t sensorValue){
    return map(sensorValue, 2700, 6500, 0, 255);
}

void applyPWM(uint16_t sensorValue){
    uint32_t pwm = calculatePWM(sensorValue);
    uint32_t pwmI = 255 - pwm;
    ledcWrite(LEDW, pwm);
    ledcWrite(LEDY, pwmI);
    //showMessage("Output PWM", "Value: " + String(pwm));
}

unsigned int captureAverageSensor(){
    //showMessage("Reading Sensor...", "Please wait");

    unsigned long sum = 0;

    for (int i = 0; i < SENSOR_SAMPLE_COUNT; i++)
    {
        unsigned int val = retrieveSensorData();
        sum += val;
        //showMessage("Reading Sensor...","Sample " + String(i + 1) + "/" + String(SENSOR_SAMPLE_COUNT));
        delay(150);
    }

    unsigned int avg = sum / SENSOR_SAMPLE_COUNT;
    return avg;
}

void saveToEEPROM(unsigned int value){
    preferences.begin(PREF_NAMESPACE, false);
    preferences.putUInt(PREF_KEY, value);
    preferences.end();
}

unsigned int loadFromEEPROM(){
    preferences.begin(PREF_NAMESPACE, true);
    unsigned int val = preferences.getUInt(PREF_KEY, 0);
    preferences.end();

    return val;
}