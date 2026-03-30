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
      unsigned int avg = captureAverageSensor();
      storedSensorValue = avg;
      showMessage("Saving to EEPROM", String(avg));
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