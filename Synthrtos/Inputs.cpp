#include "Config.h"
#include <AMY-Arduino.h>

// Hidden from the main file - purely for internal tracking
struct AnalogInput {
  uint8_t pin;
  int samples[POT_FILTER_SAMPLES];
  int sampleIndex;
  long sum;
  int lastReportedValue;
};

// Internal state tracking
AnalogInput inputs[NUM_ANALOG_INPUTS] = {
  {POT_VOL,    {0}, 0, 0, 0},
  {POT_CUTOFF, {0}, 0, 0, 0},
  {POT_RES,    {0}, 0, 0, 0},
  {POT_A,      {0}, 0, 0, 0},
  {POT_D,      {0}, 0, 0, 0},
  {POT_S,      {0}, 0, 0, 0},
  {POT_R,      {0}, 0, 0, 0},
  {STICK_X,    {0}, 0, 0, 0},
  {STICK_Y,    {0}, 0, 0, 0}
};

int finalValues[NUM_ANALOG_INPUTS] = {0};

void initPots() {
  for (int i = 0; i < NUM_ANALOG_INPUTS; i++) {
    int initialVal = analogRead(inputs[i].pin);
    inputs[i].sum = 0;
    
    for (int j = 0; j < POT_FILTER_SAMPLES; j++) {
      inputs[i].samples[j] = initialVal;
      inputs[i].sum += initialVal;
    }
    
    inputs[i].lastReportedValue = initialVal;
    finalValues[i] = initialVal;
  }
}

bool updatePots() {
  bool anyValueUpdated = false;

  for (int i = 0; i < NUM_ANALOG_INPUTS; i++) {
    int rawValue = analogRead(inputs[i].pin);

    // Update moving average sum
    inputs[i].sum -= inputs[i].samples[inputs[i].sampleIndex];
    inputs[i].samples[inputs[i].sampleIndex] = rawValue;
    inputs[i].sum += rawValue;
    
    // Advance index
    inputs[i].sampleIndex = (inputs[i].sampleIndex + 1) % POT_FILTER_SAMPLES;

    int filteredValue = inputs[i].sum / POT_FILTER_SAMPLES;

    // Check deadband threshold
    if (abs(filteredValue - inputs[i].lastReportedValue) >= POT_DEADBAND) {
      inputs[i].lastReportedValue = filteredValue; 
      finalValues[i] = filteredValue;              
      anyValueUpdated = true;                      
    }
  }

  return anyValueUpdated;
}

// --- FreeRTOS Variables ---
QueueHandle_t encoderQueue;
QueueHandle_t keyEventQueue; 
QueueHandle_t encoderButtonQueue; // Allocation
volatile bool potsUpdatedFlag = false;

// --- Filter Button Variables ---
bool filterState = HIGH;
bool lastFilterState = HIGH;
int filterType = 1;
int filterDebounce = 0;

// --- Gray Code Finite State Machine Variables ---
volatile uint8_t encoderState = 0;

const uint8_t ttable[7][4] = {
  {0x0, 0x2, 0x4, 0x0},      
  {0x3, 0x0, 0x1, 0x40},     
  {0x3, 0x2, 0x0, 0x0},      
  {0x3, 0x2, 0x1, 0x0},      
  {0x6, 0x0, 0x4, 0x0},      
  {0x6, 0x5, 0x0, 0x80},     
  {0x6, 0x5, 0x4, 0x0}       
};

void IRAM_ATTR encoderISR() {
  uint8_t pinstate = (digitalRead(ENCODER_B) << 1) | digitalRead(ENCODER_A);
  encoderState = ttable[encoderState & 0x0f][pinstate];
  uint8_t result = encoderState & 0xc0;
  
  if (result != 0) {
    int8_t encDir = (result == 0x40) ? 1 : -1;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xQueueSendFromISR(encoderQueue, &encDir, &xHigherPriorityTaskWoken);
    if (xHigherPriorityTaskWoken) {
      portYIELD_FROM_ISR();
    }
  }
}

// --- FreeRTOS Core 0 Input Task ---
void inputTask(void *pvParameters) {
  static bool lastOctUpState = HIGH;
  static bool lastOctDownState = HIGH;
  static uint32_t lastOctPressTime = 0;
  const uint32_t DEBOUNCE_DELAY = 150; 

  // Local tracking variables for the Encoder Switch
  static bool lastSwState = HIGH;
  static uint32_t swPressStartTime = 0;
  static bool holdTriggered = false;

  for(;;) {
    
    // 1. Read Analog Potentiometers
    if (updatePots()) {
      potsUpdatedFlag = true;
    }

    // 2. Read Octave Buttons
    bool octUpState = digitalRead(PIN_OCT_UP);
    bool octDownState = digitalRead(PIN_OCT_DOWN);

    if (millis() - lastOctPressTime > DEBOUNCE_DELAY) {
      if (octUpState == LOW && lastOctUpState == HIGH) {
        baseNote += 12;
        baseNote = constrain(baseNote, 24, 96);
        Serial.print("Octave Up. New Base Note: ");
        Serial.println(baseNote);
        lastOctPressTime = millis();
      }
      if (octDownState == LOW && lastOctDownState == HIGH) {
        baseNote -= 12;
        baseNote = constrain(baseNote, 24, 96);
        Serial.print("Octave Down. New Base Note: ");
        Serial.println(baseNote);
        lastOctPressTime = millis();
      }
    }
    lastOctUpState = octUpState;
    lastOctDownState = octDownState;

    // 3. Read Encoder Switch (Direct Pin on Core 0)
    bool swState = digitalRead(ENCODER_SW);

    if (swState == LOW && lastSwState == HIGH) {
      // Switch was just pressed down
      swPressStartTime = millis();
      holdTriggered = false;
    } 
    else if (swState == LOW && lastSwState == LOW) {
      // Switch is currently being held down
      if (!holdTriggered && (millis() - swPressStartTime > 800)) { // 800ms Hold Limit
        holdTriggered = true;
        EncoderButtonEvent evt = { EVT_HOLD };
        xQueueSend(encoderButtonQueue, &evt, 0);
      }
    } 
    else if (swState == HIGH && lastSwState == LOW) {
      // Switch was just released
      uint32_t pressDuration = millis() - swPressStartTime;
      if (pressDuration > 30 && !holdTriggered) { // 30ms Minimum Debounce
        EncoderButtonEvent evt = { EVT_CLICK };
        xQueueSend(encoderButtonQueue, &evt, 0);
      }
    }
    lastSwState = swState;

    // 4. Read MCP23017 Keys (I2C)
    if (xSemaphoreTake(i2cMutex, portMAX_DELAY) == pdTRUE) {
      filterState = mcp.digitalRead(FILTERTYPE_BTN);
      if (filterState == LOW && lastFilterState == HIGH && (millis() - filterDebounce) > 10) {
        filterDebounce = millis();
        filterType++;
        if (filterType > 3) filterType = 1;
        mcp.digitalWrite(LPLED, filterType == 1);
        mcp.digitalWrite(BPLED, filterType == 2);
        mcp.digitalWrite(HPLED, filterType == 3);
        amy_event e = amy_default_event();
        e.synth = 1;
        e.filter_type = filterType;
        amy_add_event(&e);
      }
      lastFilterState = filterState;
      for (int i = 0; i < numKeys; i++) {
        keyState[i] = mcp.digitalRead(KEY_PINS[i]);
      }
      xSemaphoreGive(i2cMutex); 
    }

    // Process Key states and send to Queue
    for (int i = 0; i < numKeys; i++) {
      if (keyState[i] != lastKeyState[i]) {
        KeyEvent event;
        event.keyIndex = i;
        event.isPressed = (keyState[i] == LOW);
        
        xQueueSend(keyEventQueue, &event, 0);
        
        lastKeyState[i] = keyState[i]; 
      }
    }

    vTaskDelay(pdMS_TO_TICKS(2)); 
  }
}

void startInputTask() {
  encoderQueue = xQueueCreate(10, sizeof(int8_t));
  keyEventQueue = xQueueCreate(20, sizeof(KeyEvent)); 
  encoderButtonQueue = xQueueCreate(10, sizeof(EncoderButtonEvent)); // Create Button Queue

  pinMode(ENCODER_A, INPUT_PULLUP);
  pinMode(ENCODER_B, INPUT_PULLUP);

  attachInterrupt(digitalPinToInterrupt(ENCODER_A), encoderISR, CHANGE);
  attachInterrupt(digitalPinToInterrupt(ENCODER_B), encoderISR, CHANGE);

  xTaskCreatePinnedToCore(
    inputTask,       
    "InputTask",     
    4096,            
    NULL,            
    2,               
    NULL,            
    0                
  );
}