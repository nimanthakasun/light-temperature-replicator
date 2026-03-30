#include <Preferences.h>
#include "DFRobot_ColorTemperature.h"
#include <OneWire.h>
#include <DallasTemperature.h>
#if defined(ARDUINO_AVR_UNO)||defined(ESP8266)
#include <SoftwareSerial.h>
#endif

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

DFRobot_ColorTemperature CT(/*pWire = */&Wire);

// PWM Configuration
const int LEDW = 33;
const int LEDY = 32;
const int freq = 15000;
const int resolution = 8;

// Input configuratio
const int saveSwitch = 14;

//Sensor Values
int mappedTemp = 0;
int mappedTempComp = 0;

// Internal tempSensor
const int oneWireBus = 4;     

// Brightness adjustments
int floorVal = 0;
int ceilingVal = 0;

// Time sharing variables
unsigned long previousMillis = 0;
unsigned long switchDuration = 0;

/* -------------------- BUTTON CONFIG -------------------- */
#define LONG_PRESS_TIME 5000   // 5 seconds

/* -------------------- SENSOR CONFIG -------------------- */
// Color Temp
#define SENSOR_SAMPLE_COUNT 20
// Internal Temp
OneWire oneWire(oneWireBus);
DallasTemperature DS18B20(&oneWire);

/* -------------------- STORAGE -------------------- */
Preferences preferences;
const char* PREF_NAMESPACE = "sensor";
const char* PREF_KEY = "avgValue";

/* -------------------- VARIABLES -------------------- */
unsigned int storedSensorValue = 0;

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32
#define OLED_RESET -1
#define OLED_ADDR 0x3C

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);


bool buttonPressed = false;
unsigned long buttonPressStart = 0;

void showMessage(String line1, String line2 = "")
{
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println(line1);
  display.println(line2);
  display.display();
}
 
void setup(){
  // configure LED PWM
  ledcAttach(LEDW, freq, resolution);
  ledcAttach(LEDY, freq, resolution);

  // Initialize Serial
  Serial.begin(115200);

  // Initialize Display
  Wire.begin();
  display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR);
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  // Initialize internal temp sensor
  DS18B20.begin();

  //Initialize Color sensor
  while(CT.begin() != 0){
    Serial.println(" Sensor initialize failed!!");
    delay(1000);
  }
  Serial.println(" Sensor  initialize success!!");

  //Initialize save switch pin
  pinMode(saveSwitch, INPUT_PULLUP); 

  storedSensorValue = loadFromEEPROM();
  showMessage("Booting...", "Stored: " + String(storedSensorValue));
  delay(1500);
  applyPWM(storedSensorValue);
}
 
void loop(){
  if(!internalTempShutoff()){
    handleButton();
  }
  delay(50);
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

    if(pwm > pwmI){
      floorVal = pwmI;
      ceilingVal = pwm;
    }
    else if( pwm == pwmI){
      floorVal = pwm;
      ceilingVal = pwm;
    }
    else{
      floorVal = pwm;
      ceilngVal = pwmI;
    }
    showMessage("Output PWM", "Value: " + String(pwm));
}

unsigned int captureAverageSensor(){
    showMessage("Reading Sensor...", "Please wait");

    unsigned long sum = 0;

    for (int i = 0; i < SENSOR_SAMPLE_COUNT; i++)
    {
        unsigned int val = retrieveSensorData();
        sum += val;
        showMessage("Reading Sensor...","Sample " + String(i + 1) + "/" + String(SENSOR_SAMPLE_COUNT));
        delay(150);
    }

    unsigned int avg = sum / SENSOR_SAMPLE_COUNT;
    return avg;
}

void relTimeBrightnessAdjustment(){

}

void manualBrightnessAdjustment(){
  
}

bool internalTempShutoff(){
  DS18B20.requestTemperatures(); 
  float temperatureC = DS18B20.getTempCByIndex(0);
  if(temperatureC > 50){
    return true;
  }
  return false;
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

