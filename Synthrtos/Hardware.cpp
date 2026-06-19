#include "Config.h"

// 1. Create the hardware objects in memory
// We use the full framebuffer version (indicated by "_F_") for fast, smooth renders
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE); // <-- Changed
Adafruit_MCP23X17 mcp;
SemaphoreHandle_t i2cMutex = NULL;

// 2. Define your key arrays in memory
const int numKeys = 12;
const uint8_t KEY_PINS[12] = {3, 5, 2, 4, 1, 0, 9, 8, 11, 10, 13, 12};
int keyState[12] = {0};
int lastKeyState[12] = {0};


void setupHardware() {
  Serial.begin(115200);

  // Initialize the I2C mutex first so tasks don't attempt to use a null pointer
  i2cMutex = xSemaphoreCreateMutex(); // <-- Added

  // Directly set standard ESP32 input pins
  pinMode(PIN_OCT_UP, INPUT_PULLUP);
  pinMode(PIN_SHIFT, INPUT_PULLUP);
  pinMode(PIN_OCT_DOWN, INPUT_PULLUP);
  pinMode(ENCODER_A, INPUT_PULLUP);
  pinMode(ENCODER_B, INPUT_PULLUP);
  pinMode(ENCODER_SW, INPUT_PULLUP);

  // Initialize I2C
  Wire.begin(40, 39);
  Wire.setClock(400000); 

  // Initialize OLED Display with U8g2
  if (xSemaphoreTake(i2cMutex, portMAX_DELAY) == pdTRUE) {
    u8g2.begin(); // <-- Initialize U8g2
    u8g2.clearDisplay();
    xSemaphoreGive(i2cMutex);
  }
  
  delay(2000);
  Serial.println("AMY_Synth");

  // Initialize MCP23017 (Protected by mutex)
  if (xSemaphoreTake(i2cMutex, portMAX_DELAY) == pdTRUE) {
    if (!mcp.begin_I2C(0x20)) {
      Serial.println("MCP23017 Error.");
      while (1);
    }

    // Setup MCP LEDs
    for (uint8_t pin : { LPLED, BPLED, HPLED }) {
      mcp.pinMode(pin, OUTPUT);
    }
    mcp.digitalWrite(LPLED, HIGH);
    mcp.digitalWrite(BPLED, LOW);
    mcp.digitalWrite(HPLED, LOW);

    // Setup MCP Button
    mcp.pinMode(FILTERTYPE_BTN, INPUT_PULLUP);

    // Setup MCP Keyboard Pins & Initial States
    for (int i = 0; i < numKeys; i++) {
      mcp.pinMode(KEY_PINS[i], INPUT_PULLUP);
      keyState[i] = mcp.digitalRead(KEY_PINS[i]);
      lastKeyState[i] = keyState[i];
    }
    xSemaphoreGive(i2cMutex);
  }
}

// U8G2 Display Task
void displayTask(void *pvParameters) {
  for (;;) {
    if (xSemaphoreTake(i2cMutex, portMAX_DELAY) == pdTRUE) {
      u8g2.clearBuffer();

      const unsigned char* bmp = getCurrentScreenBitmap();
      if (bmp != nullptr) {
        // Draw main level background bitmap (Preset, Seq, Settings, User)
        u8g2.drawXBMP(0, 0, 128, 64, bmp);
      } else {
        // Fallback UI wrapper for text-based sub-screens
        u8g2.setFont(u8g2_font_6x12_tf);
        u8g2.setDrawColor(1);
        u8g2.drawFrame(0, 0, 128, 64); // Screen border
        
        // 1. Draw Title
        u8g2.setCursor(10, 18);
        u8g2.print(getScreenName(currentScreen));
        u8g2.drawHLine(5, 22, 118); // Divider line
        
        // 2. Draw Values based on active screen
        u8g2.setCursor(10, 38);
        if (currentScreen == SCREEN_PRESET_SUB_EDIT) {
          u8g2.print(F("Preset: #"));
          u8g2.print(presetNumber);
        } 
        else if (currentScreen == SCREEN_SEQ_TEMPO) {
          u8g2.print(F("Tempo: "));
          u8g2.print(seqTempo);
          u8g2.print(F(" BPM"));
        }
        else if (currentScreen == SCREEN_SEQ_LENGTH) {
          u8g2.print(F("Length: "));
          u8g2.print(seqLength);
          u8g2.print(F(" Steps"));
        }
        else {
          u8g2.print(F("[No Parameter]"));
        }
        
        // 3. Draw Editing State Instruction Footer
        u8g2.setCursor(10, 53);
        if (isEditingValue) {
          // Visual highlight when actively editing
          u8g2.print(F("> EDITING - Turn Knob <"));
        } else {
          u8g2.print(F("[Click to Adjust]"));
        }
      }

      u8g2.sendBuffer();
      xSemaphoreGive(i2cMutex);
    }
    vTaskDelay(pdMS_TO_TICKS(66)); 
  }
}

void startDisplayTask() {
  xTaskCreatePinnedToCore(
    displayTask, 
    "DisplayTask", 
    4096, 
    NULL, 
    1,            // Priority 1
    NULL, 
    0             // Core 0
  );
}