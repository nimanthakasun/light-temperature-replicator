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

#include <Arduino.h>
#include <Wire.h>

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
bool alreadyReduced = false;

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
#define SHUTOFF_THRESHOLD 75
#define DIMMING_THRESHOLD 60
// Lux Sensor - External
BH1750 lightMeter(0x23);

/* -------------------- STORAGE -------------------- */
Preferences preferences;
const char* PREF_NAMESPACE = "sensor";
const char* PREF_KEY_TEMP = "clTempVal";
const char* PREF_KEY_LUX = "luxValue";

/* -------------------- VARIABLES -------------------- */
unsigned int storedSensorValue = 0;
unsigned int storedLuxValue = 0;

/* -------------------- REAL-TIME BRIGHTNESS ADJUSTMENT -------------------- */
// Baseline lux captured on long-press save, used as the reference point.
float baselineLux = 0;

// Hysteresis bands, expressed as a ratio of current lux to baselineLux.
// Below LUX_DROP_RATIO  -> ambient light dropped a lot -> dim.
// Above LUX_RISE_RATIO  -> ambient light rose a lot     -> brighten.
// In between            -> no change (prevents flicker at the edges).
const float LUX_DROP_RATIO  = 0.7f;   // 30% darker than baseline
const float LUX_RISE_RATIO  = 1.3f;   // 30% brighter than baseline

// Fixed-step brightness adjustments (not a full lux->PWM mapping).
const float DIM_STEP    = 0.8f;   // -20%
const float BRIGHT_STEP = 1.2f;   // +20%

// -1 = dimmed one step, 0 = normal, 1 = brightened one step.
int brightnessState = 0;

// How often to poll/re-evaluate lux for the real-time adjustment.
const unsigned long LUX_CHECK_INTERVAL = 2000; // ms

// The "base" white/yellow PWM pair representing the captured color temperature,
// before any real-time brightness scaling is applied.
unsigned int baseWhitePWM = 0;
unsigned int baseYellowPWM = 0;

// Function Definitions

// colortempsensor_handler.inoo
void init_colortempSensor();
unsigned int captureAverageColorTempSensorValue();
uint16_t retrieveSensorData();

// ledstrip_handler.ino
uint32_t calculatePWM(uint16_t sensorValue);
void setBasePWM(uint16_t sensorValue);
void applyScaledPWM(float scale);

// memory_handler.ino
void saveToEEPROM(const char *key, unsigned int value);
unsigned int loadTempFromEEPROM();
unsigned int loadLuxFromEEPROM();

// tempdhutoff_handler.ino
bool internalTempShutoff();

// brightness_handler.ino
void relTimeBrightnessAdjustment();

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
  // initDisplay();

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

  // Initiate Lux sensor
  if (lightMeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE)) {
    Serial.println(F("BH1750 Advanced begin"));
  } else {
    Serial.println(F("Error initialising BH1750"));
  }

  bootSequence();

}
 
void loop(){
  if(!internalTempShutoff()){
    handleButton();
    // Poll ambient lux periodically (not every loop tick) and react if it has
    // moved far enough from the saved baseline.
    unsigned long currentMillis = millis();
    if (currentMillis - previousMillis >= LUX_CHECK_INTERVAL) {
      previousMillis = currentMillis;
      relTimeBrightnessAdjustment();
    }
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
      // showMessage("Long Press Detected", "Capturing...");
      Serial.print("Long Press Detected. Capturing...");
      float lux = 0.0;

      // Capturing Color temperatre
      unsigned int avgColorTemp = captureAverageColorTempSensorValue();
      storedSensorValue = avgColorTemp;
      Serial.print("Color Temperature: ");
      Serial.println(avgColorTemp);

      if (lightMeter.measurementReady()) {
        lux = lightMeter.readLightLevel();
        Serial.print("External Lux: ");
        Serial.print(lux);
        Serial.println(" lx");
      }

      // showMessage("Saving to EEPROM", String(avgColorTemp));
      Serial.print("Saving to EEPROM color temperature: ");
      Serial.println(avgColorTemp);
      Serial.print("Saving to EEPROM lux: ");
      Serial.println(lux);
      saveToEEPROM(PREF_KEY_TEMP, avgColorTemp);
      saveToEEPROM(PREF_KEY_LUX, (unsigned int)lux);
      storedLuxValue = (unsigned int)lux;
      baselineLux = lux;
      brightnessState = 0; // fresh baseline -> reset any prior dim/bright state

      delay(1000);
      setBasePWM(storedSensorValue);
      applyScaledPWM(1.0f);
    }
  }
}

// Brightness Handler
void relTimeBrightnessAdjustment(){
  // Nothing to compare against yet (e.g. no long-press capture has happened).
  if (baselineLux <= 0) return;

  if (!lightMeter.measurementReady()) return;
  float currentLux = lightMeter.readLightLevel();
  if (currentLux < 0) return; // BH1750 read error

  float ratio = currentLux / baselineLux;
  int newState = brightnessState;

  if (ratio < LUX_DROP_RATIO && brightnessState > -1) {
    newState = brightnessState - 1;
  } else if (ratio > LUX_RISE_RATIO && brightnessState < 1) {
    newState = brightnessState + 1;
  } else if (ratio >= LUX_DROP_RATIO && ratio <= LUX_RISE_RATIO) {
    newState = 0; // back within the normal band -> return to baseline brightness
  }

  if (newState != brightnessState) {
    brightnessState = newState;

    float scale = 1.0f;
    if (brightnessState == -1) scale = DIM_STEP;
    else if (brightnessState == 1) scale = BRIGHT_STEP;

    Serial.print("Ambient lux: "); Serial.print(currentLux);
    Serial.print(" | baseline: "); Serial.print(baselineLux);
    Serial.print(" | ratio: "); Serial.print(ratio);
    Serial.print(" | new brightnessState: "); Serial.println(brightnessState);

    applyScaledPWM(scale);
  }
}

void manualBrightnessAdjustment(){
  
}

// Temperature Shutdown
bool internalTempShutoff(){
  DS18B20.requestTemperatures(); 
  float temperatureC = DS18B20.getTempCByIndex(0);
  Serial.print("Internal Tempearture: ");
  Serial.println(temperatureC);
  if(temperatureC > SHUTOFF_THRESHOLD){
    return true;
  }
  else if(temperatureC > DIMMING_THRESHOLD){
    if(!alreadyReduced){
      storedSensorValue = loadTempFromEEPROM();
      storedLuxValue = loadLuxFromEEPROM();
      // showMessage("Higher Internal temperature detected", "Reducing brightness...");
      Serial.println("Higher Internal temperature detected. Reducing brightness...");

      Serial.print("Stored color temperature: ");
      Serial.println(String(storedSensorValue));
      Serial.print("Stored lux value: ");
      Serial.println(String(storedLuxValue));
      delay(1500);
      setBasePWM(storedSensorValue);
      applyScaledPWM(0.5f);
      alreadyReduced = true;
    }
    return true;
  }
  else{
    return false;
  }
}


// Memory handler
void saveToEEPROM(const char *key, unsigned int value){
    preferences.begin(PREF_NAMESPACE, false);
    preferences.putUInt(key, value);
    preferences.end();
}

unsigned int loadTempFromEEPROM(){
    preferences.begin(PREF_NAMESPACE, true);
    unsigned int val = preferences.getUInt(PREF_KEY_TEMP, 0);
    preferences.end();
    return val;
}

unsigned int loadLuxFromEEPROM(){
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

unsigned int captureAverageColorTempSensorValue(){
    // showMessage("Reading Sensor...", "Please wait");

    unsigned long sum = 0;

    for (int i = 0; i < SENSOR_SAMPLE_COUNT; i++)
    {
        unsigned int val = retrieveSensorData();
        sum += val;
        // showMessage("Reading Sensor...","Sample " + String(i + 1) + "/" + String(SENSOR_SAMPLE_COUNT));
        delay(150);
    }

    unsigned int avg = sum / SENSOR_SAMPLE_COUNT;
    return avg;
}

uint16_t retrieveSensorData(){
   return CT.readCCT();
}

// LED strip handler

// Maps a captured color-temperature sensor value to a white-channel PWM duty
// cycle. Yellow is always the complement (255 - white), so this ratio defines
// the white:yellow *mix* only -- it says nothing about overall brightness.
uint32_t calculatePWM(uint16_t sensorValue){
    return map(sensorValue, 2700, 6500, 0, 255);
}

// Call this whenever a new color-temperature value becomes the "current"
// target (on boot, after a long-press capture, or after loading from EEPROM
// for thermal dimming). It fixes the white/yellow mix that all subsequent
// applyScaledPWM() calls will scale up or down.
void setBasePWM(uint16_t sensorValue){
  uint32_t whitePWM = calculatePWM(sensorValue);
  baseWhitePWM = whitePWM;
  baseYellowPWM = 255 - whitePWM;
}

// Applies the current base white/yellow mix scaled by `scale` (e.g. 0.8 for
// -20%, 1.2 for +20%). This preserves the white:yellow ratio (color
// temperature) while changing overall output brightness -- unlike writing
// calculatePWM() directly, which would also shift the color mix.
void applyScaledPWM(float scale){
    int whiteOut  = (int)(baseWhitePWM  * scale);
    int yellowOut = (int)(baseYellowPWM * scale);

    whiteOut  = constrain(whiteOut, 0, 255);
    yellowOut = constrain(yellowOut, 0, 255);

    ledcWrite(LEDW, whiteOut);
    ledcWrite(LEDY, yellowOut);

    if (whiteOut > yellowOut) {
      floorVal = yellowOut;
      ceilingVal = whiteOut;
    } else if (whiteOut == yellowOut) {
      floorVal = whiteOut;
      ceilingVal = whiteOut;
    } else {
      floorVal = whiteOut;
      ceilingVal = yellowOut;
    }

    // showMessage("Output PWM", "W:" + String(whiteOut) + " Y:" + String(yellowOut));
}

// Initialize display
// void initDisplay(){
//   Wire.begin();
//   display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR);
//   display.setTextSize(1);
//   display.setTextColor(SSD1306_WHITE);
// }

// void showMessage(String line1, String line2 = ""){
//   display.clearDisplay();
//   display.setCursor(0, 0);
//   display.println(line1);
//   display.println(line2);
//   display.display();
// }

void bootSequence(){
  storedSensorValue = loadTempFromEEPROM();
  storedLuxValue = loadLuxFromEEPROM();
  baselineLux = storedLuxValue;
  brightnessState = 0;

  // showMessage("Booting...", "Stored color temperature: " + String(storedSensorValue));

  Serial.println("Booting...");
  Serial.print("Stored color temperature: ");
  Serial.println(String(storedSensorValue));
  Serial.print("Stored lux value: ");
  Serial.println(String(storedLuxValue));
  delay(1500);

  setBasePWM(storedSensorValue);
  applyScaledPWM(1.0f);
}
