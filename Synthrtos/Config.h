#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>
#include <Wire.h>
#include <U8g2lib.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_MCP23X17.h>
#include "Screens.h"
#include "Controls.h"
#include <AMY-Arduino.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>

// --- I/O Pin Definitions ---
#define PIN_OCT_UP    10 
#define PIN_OCT_DOWN  11
#define PIN_SHIFT     9
#define ENCODER_A     41
#define ENCODER_B     16
#define ENCODER_SW    42

#define POT_VOL       8  
#define POT_CUTOFF    6  
#define POT_RES       7  
#define POT_A         1  
#define POT_D         2  
#define POT_S         4  
#define POT_R         5  
#define STICK_X       18
#define STICK_Y       17

// --- MCP23017 Pin Definitions ---
const uint8_t LPLED = 15;
const uint8_t BPLED = 6;
const uint8_t HPLED = 7;
const uint8_t FILTERTYPE_BTN = 14;

// --- Key Matrix Settings ---
extern const int numKeys;
extern const uint8_t KEY_PINS[12];
extern int keyState[12];
extern int lastKeyState[12];

// --- OLED Settings ---
#define SCREEN_WIDTH 128 
#define SCREEN_HEIGHT 64 
#define OLED_RESET    -1 
#define SCREEN_ADDRESS 0x3C

// --- Pot Filter Settings ---
#define POT_FILTER_SAMPLES 9
#define POT_DEADBAND 15 
#define NUM_ANALOG_INPUTS  9

// Enum for readable array access
enum InputIndex {
  IDX_POT_VOL = 0,
  IDX_POT_CUTOFF,
  IDX_POT_RES,
  IDX_POT_A,
  IDX_POT_D,
  IDX_POT_S,
  IDX_POT_R,
  IDX_STICK_X,
  IDX_STICK_Y
};


struct KeyEvent {
  uint8_t keyIndex;
  bool isPressed;
};

// --- Encoder Switch Event Types ---
enum EncoderButtonAction {
  EVT_CLICK,
  EVT_HOLD
};

struct EncoderButtonEvent {
  EncoderButtonAction action;
};

// --- Extern Variables & Objects ---
// (This lets your main .ino file use these things freely)
extern int finalValues[NUM_ANALOG_INPUTS];
extern U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2;
extern Adafruit_MCP23X17 mcp;
extern int baseNote;
extern QueueHandle_t encoderQueue;
extern QueueHandle_t keyEventQueue;
extern QueueHandle_t encoderButtonQueue;
extern volatile bool potsUpdatedFlag;
extern SemaphoreHandle_t i2cMutex;

// --- Function Prototypes ---
void setupHardware();
void setupAmy();
void initPots();
bool updatePots();
int8_t read_rotary();
void startInputTask();
void updateSynthesizerControls();
void startDisplayTask();                 // <-- Added
void displayTask(void *pvParameters);    // <-- Added
void playNote(int i);
void noteOff(int i);

#endif