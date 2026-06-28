#include <RotaryEncoder.h>

#include <Preferences.h>
#include "DFRobot_ColorTemperature.h"
#include <OneWire.h>
#include <DallasTemperature.h>
#include <BH1750.h>
// #include <RotaryEncoder.h>
#if defined(ARDUINO_AVR_UNO)||defined(ESP8266)
#include <SoftwareSerial.h>
#endif

#include <display_handler.h>

// Display Handler
#ifndef DISPLAY_HANDLER_H
#define DISPLAY_HANDLER_H

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

void initDisplay();
void showMessage(String line1, String line2);

#endif

DFRobot_ColorTemperature CT(/*pWire = */&Wire);

// PWM Configuration
const int LEDW = 33;
const int LEDY = 32;
const int freq = 15000;
const int resolution = 8;

// Rotary Encoder Configuration
#define DIRECTION_CW  0   // clockwise direction
#define DIRECTION_CCW 1  // counter-clockwise direction
const int CLOCK = 27;
const int DT = 12;
const int SW = 13;

int counter = 0;
int direction = DIRECTION_CW;
int CLK_state;
int prev_CLK_state;

RotaryEncoder encoder(CLOCK, DT, RotaryEncoder::LatchMode::TWO03);

// Input configuratio
const int saveSwitch = 14;
const int brightnessPot = 36;

//Sensor Values
int mappedTemp = 0;
int mappedTempComp = 0;

// Internal tempSensor
const int oneWireBus = 4;     

// Brightness adjustments
int floorVal = 0;
int ceilingVal = 0;
int turns = 0;

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
// Lux Sensor - External
BH1750 lightMeter(0x23);

/* -------------------- STORAGE -------------------- */
Preferences preferences;
const char* PREF_NAMESPACE = "sensor";
const char* PREF_KEY_TEMP = "avgValue";
const char* PREF_KEY_LUX = "avgValue";

/* -------------------- VARIABLES -------------------- */
unsigned int storedSensorValue = 0;

/* -------------------- OLED DISPLAY -------------------- */
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32
#define OLED_RESET -1
#define OLED_ADDR 0x3C

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Function Definitions

// colortempsensor_handler.inoo
void init_colortempSensor();
unsigned int captureAverageSensor();
uint16_t retrieveSensorData();

// ledstrip_handler.ino
uint32_t calculatePWM(uint16_t sensorValue);
void applyPWM(uint32_t pwm);

// memory_handler.ino
void saveToEEPROM(unsigned int value);
unsigned int loadFromEEPROM();

// tempdhutoff_handler.ino
bool internalTempShutoff();

// brightness_handler.ino
void checkAdjusterChange();

// switch_handler.ino
void handleButton();


bool buttonPressed = false;
unsigned long buttonPressStart = 0;


void setup(){
  // configure LED PWM
  ledcAttach(LEDW, freq, resolution);
  ledcAttach(LEDY, freq, resolution);

  // Initialize Serial
  Serial.begin(115200);

  // Initialize Display
  initDisplay();

  // Initialize internal temp sensor
  DS18B20.begin();

  //Initialize Color sensor
  init_colortempSensor();
  Serial.println(" Sensor  initialize success!!");

  //Initialize save switch pin
  pinMode(saveSwitch, INPUT_PULLUP);

  // Initialize Rotary Encoder pins
  pinMode(CLOCK, INPUT);
  pinMode(DT, INPUT);
  pinMode(SW, INPUT);

  storedSensorValue = loadFromEEPROM();
  showMessage("Booting...", "Stored: " + String(storedSensorValue));
  delay(1500);
  applyPWM(calculatePWM(storedSensorValue));

  // Initiate Lux sensor
  if (lightMeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE)) {
    Serial.println(F("BH1750 Advanced begin"));
  } else {
    Serial.println(F("Error initialising BH1750"));
  }

  // Initialize Display

}
 
void loop(){
  if(!internalTempShutoff()){
    handleButton();
    showMessage("Sample Text", "Sample text 2");
    // checkAdjusterChange();
  }
  delay(50);
}

//-------------------- Code Regions --------------------
// Switch Handler
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
      showMessage("Long Press Detected", "Capturing...");
      Serial.print("Long Press Detected. Capturing...");
      unsigned int avg = captureAverageSensor();
      storedSensorValue = avg;
      Serial.print("Color Temperature: ");
      Serial.println(avg);

      if (lightMeter.measurementReady()) {
        float lux = lightMeter.readLightLevel();
        Serial.print("External Lux: ");
        Serial.print(lux);
        Serial.println(" lx");
      }

      showMessage("Saving to EEPROM", String(avg));
      Serial.print("Saving to EEPROM");
      Serial.println(avg);
      saveToEEPROM(PREF_KEY_TEMP, avg);
      saveToEEPROM(PREF_KEY_LUX, avg);
      delay(1000);
      applyPWM(calculatePWM(storedSensorValue));
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

// Brightness Handler
void relTimeBrightnessAdjustment(){

}

void manualBrightnessAdjustment(){
  
}

void checkAdjusterChange(){
  // Prev---------
  // CLK_state = digitalRead(CLOCK);
  // if (CLK_state != prev_CLK_state && CLK_state == HIGH) {
  //   if (digitalRead(DT) == HIGH) {
  //     counter--;
  //     direction = DIRECTION_CCW;
  //     Serial.println("CCW");
  //   } else {
  //     counter++;
  //     direction = DIRECTION_CW;
  //     Serial.println("CW");
  //   }
  // }
  // prev_CLK_state = CLK_state;

  // New ---------
  static int pos = 0;
  encoder.tick();

  int newPos = encoder.getPosition();
  if (pos != newPos) {
    Serial.print("pos:");
    Serial.print(newPos);
    Serial.print(" dir:");
    Serial.println((int)(encoder.getDirection()));
    pos = newPos;
  }
}

// Temperature Shutdown
bool internalTempShutoff(){
  DS18B20.requestTemperatures(); 
  float temperatureC = DS18B20.getTempCByIndex(0);
  Serial.print("Internal Tempearture: ");
  Serial.println(temperatureC);
  if(temperatureC > 50){
    return true;
  }
  return false;
}


// Memory handler
void saveToEEPROM(const char *key, unsigned int value){
    preferences.begin(PREF_NAMESPACE, false);
    preferences.putUInt(key, value);
    preferences.end();
}

unsigned int loadFromEEPROM(){
    preferences.begin(PREF_NAMESPACE, true);
    unsigned int val = preferences.getUInt(PREF_KEY_LUX, 0);
    preferences.end();

    return val;
}

// Color Temp sensor handler
void init_colortempSensor(){
  while(CT.begin() != 0){
    Serial.println(" Sensor initialize failed!!");
    delay(1000);
  }
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

uint16_t retrieveSensorData(){
   return CT.readCCT();
}

// LED strip handler
uint32_t calculatePWM(uint16_t sensorValue){
    return map(sensorValue, 2700, 6500, 0, 255);
}

void applyPWM(uint32_t pwm){
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
      ceilingVal = pwmI;
    }
    showMessage("Output PWM", "Value: " + String(pwm));
}

// Initialize display
void initDisplay()
{
    // Initialize Display
  Wire.begin();
  display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR);
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
}

void showMessage(String line1, String line2 = "")
{
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println(line1);
  display.println(line2);
  display.display();
}