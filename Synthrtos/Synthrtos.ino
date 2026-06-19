#include "Config.h"

void setup() {
  // Initialize peripherals and I2C devices
  setupHardware();
  printMemoryDiagnostics();
  
  // Initialize ADC filtering for the potentiometers
  initPots();
  
  // Initialize the AMY Synthesizer library
  setupAmy();
  printMemoryDiagnostics();
  
  // Start the input polling task (Core 0)
  startInputTask();
  
  // Start the OLED rendering task (Core 0)
  startDisplayTask();
  
  Serial.println("Synthesizer ready.");
}

void loop() {
  // 1. Process Key State Events from the Queue
  KeyEvent keyEvt;
  if (xQueueReceive(keyEventQueue, &keyEvt, 0) == pdTRUE) {
    if (keyEvt.isPressed) {
      playNote(keyEvt.keyIndex);
    } else {
      noteOff(keyEvt.keyIndex);
    }
  }

  // 2. Process Encoder Rotation Events (Scroll Navigation)
  int8_t encDir;
  if (xQueueReceive(encoderQueue, &encDir, 0) == pdTRUE) {
    handleScreenNavigation(encDir);
  }

  // 3. Process Encoder Button Click/Hold Events (Menu Navigation)
  EncoderButtonEvent btnEvt;
  if (xQueueReceive(encoderButtonQueue, &btnEvt, 0) == pdTRUE) {
    if (btnEvt.action == EVT_CLICK) {
      handleScreenClick();
    } else if (btnEvt.action == EVT_HOLD) {
      handleScreenHold();
    }
  }

  // 4. Process Potentiometer Value Changes
  if (potsUpdatedFlag) {
    
    updateSynthesizerControls(); // <-- Simplified to a single function call
    updateKnobs();
    if (checkADSRChanged()) {
      updateEnvelope();
    }
    potsUpdatedFlag = false;
  }

  delay(1);
}