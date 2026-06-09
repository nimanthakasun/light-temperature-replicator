// #include <Arduino.h>
// #include "display_handler.h"

// #define SCREEN_WIDTH 128
// #define SCREEN_HEIGHT 32
// #define OLED_RESET -1
// #define OLED_ADDR 0x3C

// Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// void initDisplay()
// {
//     // Initialize Display
//   Wire.begin();
//   display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR);
//   display.setTextSize(1);
//   display.setTextColor(SSD1306_WHITE);
// }

// void showMessage(String line1, String line2 = "")
// {
//   display.clearDisplay();
//   display.setCursor(0, 0);
//   display.println(line1);
//   display.println(line2);
//   display.display();
// }