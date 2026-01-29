#include <AMY-Arduino.h>
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_MCP23X17.h>
#include <KeyPins.h>
#include <Envelopes.h>
#include <Pots.h>
#include <SoftPickup.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
Adafruit_MCP23X17 mcp;

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);


static unsigned long lastEnvelopeUpdate = 0;
const unsigned long envelopeInterval = 200; // ms
static char lastEnvelope[50] = "";

//cpu usage
unsigned long lastPrint = 0;
unsigned long loopCounter = 0;

SoftPickup timePickup[NUM_POTS];
SoftPickup levelPickup[NUM_POTS];

bool shiftHeld = false;
int resonancePot = 7;
int resonancePotValue = 0;  // Raw 0–4095
int cutoffPot = 6;
int cutoffPotValue = 0;
//int attackPot = 1;
int attackPotValue = 0;
//int decayPot = 2;
int decayPotValue = 0;
//int sustainPot = 4;
int sustainPotValue = 0;
//int releasePot = 5;
int releasePotValue = 0;

int volumePot = 8;
int volumePotValue = 0;
int lastVolumePot = 0;
float volume = 1.0f;

const int stickX = 18;
const int stickY = 17;
float stickXValue = 0;
float stickYValue = 0;

float cutoff = 0.0f;
float lastCutoff = -1.0f;
float resonance = 0.7f;  // starting resonance
float pitchBend = 0.0f;
float targetPitchBend = 0.0f;
float targetCutoff = 0.0f;
float modCutoff = 0.0f;
float smoothing = 0.1f;

float targetAttack = 0.0f;
float targetDecay = 0.0f;
float targetSustain = 0.0f;
float targetRelease = 0.0f;

int osc1Type = 0;
int osc2Type = 0;
int osc3Type = 0;
int osc4Type = 0;
int osc5Type = 0;
int osc6Type = 0;
int numOscTypes = 9;
const char* oscTypes[] = {"OFF", "SINE", "PULSE", "SAW_DOWN", "SAW_UP", "TRIANGLE", "NOISE", "KS", "PCM"};

const int pinA = 41;   
const int pinB = 16;   
const int pinSW = 42;  // Button (optional)

// float prevStickYValue = 0;
// float prevStickXValue = 0;

long totalX = 0;
float samplesX = 4;
long totalY = 0;
float samplesY = 4;

long totalAttack = 0;
float samplesAttack = 8;
long totalDecay = 0;
float samplesDecay = 8;
long totalSustain = 0;
float samplesSustain = 8;
long totalRelease = 0;
float samplesRelease = 8;
long totalVolume = 0;
float samplesVolume = 8;

static uint8_t prevNextCode = 0;
static int16_t store=0;

const int octUp = 10;
const int shiftKey = 9;
const int octDown = 11;
bool lastOctDownState = HIGH;
bool lastOctUpState   = HIGH;
int baseNote = 60;

const int keyPins[] = {KEY1, KEY2, KEY3, KEY4, KEY5, KEY6, KEY7, KEY8, KEY9, KEY10, KEY11, KEY12};
const int numKeys = 12;
bool keyState[numKeys];
bool lastKeyState[numKeys];

bool filterState = HIGH;
bool lastFilterState = HIGH;
int filterType = 1;
int filterDebounce = 0;

// const char* envelope = "200,1,700,0.5,200,0.4,1000,0";
int a = 200;
int b = 700;
int c = 1000;
int d = 200;
char envelope[50];  // Output buffer

int midiNote = 60;
int currentNote = 0;

// Debounce timing
unsigned long lastButtonPress = 0;
const int debounceDelay = 200;

// Menu state
int currentMenu = 0;
int currentSelection = 0;
int patchNumber = 0;
const int visibleItems = 5;  // how many items fit on screen
int scrollOffset = 0;        // which item index is at the top

int levelToY(float level) {
    level = constrain(level, 0.0f, 1.0f);
    return SCREEN_HEIGHT - 1 - (level * (SCREEN_HEIGHT - 1));
}

struct Menu {
  const char* title;
  const char* items[15];
  int numItems;
  int parent; // index of parent menu (-1 if root)
};

Menu menus[] = {
  {"Main Menu", {"Patch", "User Patch", "Settings"}, 3, -1},         // 0
  {"Settings", {"Brightness", "Volume", "Back"}, 3, 0},               // 1
  {"Patch", {"Patch Number", "Back"}, 2, 0},                                   // 2
  {"User Patch", {"Patch Number", "OSC 1", "OSC 2", "OSC 3", "OSC 4", "OSC 5", "OSC 6", "LFO", "Pitch", "Pan", "Filter Type", "Filter Cutoff", "Resonance", "Save", "Back"}, 15, 0},   // 3
  {"OSC 1", {"Wave", "ENV 1", "ENV 2", "Back"}, 4, 3},             // 4
  {"OSC 2", {"Wave", "ENV 1", "ENV 2", "OSC 1 Chain", "Param", "Back"}, 6, 3},             // 5
  {"OSC 3", {"Wave", "ENV 1", "ENV 2", "OSC 1 Chain", "Param", "Back"}, 6, 3},             // 6
  {"OSC 4", {"Wave", "ENV 1", "ENV 2", "OSC 1 Chain", "Param", "Back"}, 6, 3},             // 7
  {"OSC 5", {"Wave", "ENV 1", "ENV 2", "OSC 1 Chain", "Param", "Back"}, 6, 3},             // 8
  {"OSC 6", {"Wave", "ENV 1", "ENV 2", "OSC 1 Chain", "Param", "Back"}, 6, 3},             // 9
  {"OSC 1 ENV 1", {"a"}, 1, 4},             // 10
  {"OSC 1 ENV 2", {"A", "D", "S", "R", "Param", "Back"}, 6, 4},             // 11
  {"OSC 2 ENV 1", {"A", "D", "S", "R", "Param", "Back"}, 6, 5},             // 12
  {"OSC 2 ENV 2", {"A", "D", "S", "R", "Param", "Back"}, 6, 5},             // 13
  {"OSC 3 ENV 1", {"A", "D", "S", "R", "Param", "Back"}, 6, 6},             // 14
  {"OSC 3 ENV 2", {"A", "D", "S", "R", "Param", "Back"}, 6, 6},             // 15
  {"OSC 4 ENV 1", {"A", "D", "S", "R", "Param", "Back"}, 6, 7},             // 16
  {"OSC 4 ENV 2", {"A", "D", "S", "R", "Param", "Back"}, 6, 7},             // 17
  {"OSC 5 ENV 1", {"A", "D", "S", "R", "Param", "Back"}, 6, 8},             // 18
  {"OSC 5 ENV 2", {"A", "D", "S", "R", "Param", "Back"}, 6, 8},             // 19
  {"OSC 6 ENV 1", {"A", "D", "S", "R", "Param", "Back"}, 6, 9},             // 20
  {"OSC 6 ENV 2", {"A", "D", "S", "R", "Param", "Back"}, 6, 9},             // 21
  {"LFO", {"Type", "Freq", "Control Param", "Param Intensity", "Back"}, 5, 3},    // 22
  {"Filter Cutoff", {"Value", "Control Param", "Param Intensity", "Back"}, 4, 3}, // 23
  {"Resonance", {"Value", "Control Param", "Param Instensity", "Back"}, 4, 3},    // 24
};

bool osc0Chains[6] = {false, false, false, false, false, false};
bool editMode = false;
bool menuNeedsRedraw = true;
int numMenus = sizeof(menus) / sizeof(Menu);

  // e.filter_freq_coefs[0] = 50.0f;  // Constant term
  // e.filter_freq_coefs[1] = 0.0f;   // Note
  // e.filter_freq_coefs[2] = 0.0f;   // Velocity
  // e.filter_freq_coefs[3] = 0.0f;   // Envelope Gen 0 (bp0)
  // e.filter_freq_coefs[4] = 1.0f;
  // strncpy(e.bp1, "0,6.0,1000,3.0,200,0", sizeof(e.bp1));
  // https://github.com/shorepine/amy/blob/main/docs/synth.md#ctrlcoefficients

// A valid CW or CCW move returns 1, invalid returns 0.
int8_t read_rotary() {
  uint8_t state = 0;

  // Build current A/B state
  if (digitalRead(pinA)) state |= 0x02;
  if (digitalRead(pinB)) state |= 0x01;

  // Clockwise transitions
  if ((prevNextCode == 0b00 && state == 0b01) ||
      (prevNextCode == 0b01 && state == 0b11) ||
      (prevNextCode == 0b11 && state == 0b10) ||
      (prevNextCode == 0b10 && state == 0b00)) {
    store++;
  }
  // Counter-clockwise transitions
  else if ((prevNextCode == 0b00 && state == 0b10) ||
           (prevNextCode == 0b10 && state == 0b11) ||
           (prevNextCode == 0b11 && state == 0b01) ||
           (prevNextCode == 0b01 && state == 0b00)) {
    store--;
  }

  prevNextCode = state;

  // KY-040: 4 valid transitions per detent
  if (store >= 2) {
    store = 0;
    return 1;
  }
  else if (store <= -2) {
    store = 0;
    return -1;
  }

  return 0;
}

void playNote(int i) {
  // int baseNote = 60;
  amy_event e = amy_default_event();
  e.synth = 1;
  e.midi_note = baseNote + i;
  e.velocity = 1;
  amy_add_event(&e);
}

void noteOff(int i) {
  // int baseNote = 60;
  amy_event e = amy_default_event();
  e.synth = 1;
  e.midi_note = baseNote + i;
  e.velocity = 0;
  amy_add_event(&e);
}

void updateUserPatch() {
  amy_event e = amy_default_event();
  e.patch_number = patchNumber;
  Serial.println("Patch reset");
  e.reset_osc = RESET_PATCH;
  amy_add_event(&e);
  e = amy_default_event();
  e.patch_number = patchNumber;
  e.osc = 0;
  e.wave = (osc1Type == 0) ? 16 : (osc1Type - 1);
  // strcpy(e.bp0, envelope);
  amy_add_event(&e);
  if (patchNumber < 300) {              //if preset patch, make sure adsr is usable
    for (int i = 0; i < 3; i++) {
      e = amy_default_event();
      e.osc = i;
      e.amp_coefs[COEF_EG0] = 1;
      e.amp_coefs[COEF_EG1] = 0;
      amy_add_event(&e);
    }
  }
  int lastChained = -1;
  for (int i = 1; i < 5; i++) {
    if (osc0Chains[i]) {
      e = amy_default_event();
      e.patch_number = patchNumber;
      if (lastChained != -1) {
        e.osc = lastChained;
        e.chained_osc = i;
        Serial.printf("\nOSC %d chained to %d \n", lastChained, i);
      } else if (lastChained == -1) {
        e.osc = 0;
        e.chained_osc = i;
        Serial.printf("\nOSC 0 chained to %d \n", i);
      }
      amy_add_event(&e);
      lastChained = i;
    }
  }
  e = amy_default_event();
  e.patch_number = patchNumber;
  e.osc = 1;
  e.wave = (osc2Type == 0) ? 16 : (osc2Type - 1);
  // strcpy(e.bp0, envelope);
  amy_add_event(&e);
  e = amy_default_event();
  e.patch_number = patchNumber;
  e.osc = 2;
  e.wave = (osc3Type == 0) ? 16 : (osc3Type - 1);
  // strcpy(e.bp0, envelope);
  amy_add_event(&e);
  e = amy_default_event();
  e.patch_number = patchNumber;
  e.osc = 3;
  e.wave = (osc4Type == 0) ? 16 : (osc4Type - 1);
  // strcpy(e.bp0, envelope);
  amy_add_event(&e);
  e = amy_default_event();
  e.patch_number = patchNumber;
  e.osc = 4;
  e.wave = (osc5Type == 0) ? 16 : (osc5Type - 1);
  // strcpy(e.bp0, envelope);
  amy_add_event(&e);
  e = amy_default_event();
  e.patch_number = patchNumber;
  e.osc = 5;
  e.wave = (osc6Type == 0) ? 16 : (osc6Type - 1);
  // strcpy(e.bp0, envelope);
  amy_add_event(&e);
  e = amy_default_event();
  e.synth = 1;
  e.num_voices = 6;
  e.patch_number = patchNumber;
  amy_add_event(&e);
}

void updateEnvelope() {
  amy_event e = amy_default_event();
  e.synth = 1;
  e.osc = 0;
  strcpy(e.bp0, envelope);
  amy_add_event(&e);
  if (patchNumber < 300) {
    for (int i = 1; i < 5; i++) {
      e = amy_default_event();
      e.synth = 1;
      e.osc = i;
      strcpy(e.bp0, envelope);
      amy_add_event(&e);
    }
  }
  else {
    for (int i = 1; i < 5; i++) {
      if (osc0Chains[i]) {
        e = amy_default_event();
        e.synth = 1;
        e.osc = i;
        strcpy(e.bp0, envelope);
        amy_add_event(&e);
      }
    }
  }
}

void handleEncoderMenu() {
  static int lastPos = 0;
  static unsigned int lastPress = 0;
  const unsigned int debounce = 250;

  static int8_t c, val;
  if (currentMenu >= 10 && currentMenu <= 21) menuNeedsRedraw = true;
  
  // --- Handle rotation ---
  if (val = read_rotary()) {
    // Serial.print(val);
    if (editMode && currentMenu == 2 && currentSelection == 0) {
      // In edit mode: adjust patch number
      patchNumber += val;
      if (patchNumber < 0) patchNumber = 0;
      if (patchNumber > 256) patchNumber = 256;
    } else if (editMode && currentMenu == 3 && currentSelection == 0) {
      patchNumber += val;
      if (patchNumber < 1024) patchNumber = 1024;
      if (patchNumber > 1055) patchNumber = 1055;
    } else if (editMode && currentMenu == 4 && currentSelection == 0) {
      osc1Type += val;
      if (osc1Type < 0) osc1Type = numOscTypes - 1;
      if (osc1Type >= numOscTypes) osc1Type = 0;
    } else if (editMode && currentMenu == 5 && currentSelection == 0) {
      osc2Type += val;
      if (osc2Type < 0) osc2Type = numOscTypes - 1;
      if (osc2Type >= numOscTypes) osc2Type = 0;
    } else if (editMode && currentMenu == 6 && currentSelection == 0) {
      osc3Type += val;
      if (osc3Type < 0) osc3Type = numOscTypes - 1;
      if (osc3Type >= numOscTypes) osc3Type = 0;
    } else if (editMode && currentMenu == 7 && currentSelection == 0) {
      osc4Type += val;
      if (osc4Type < 0) osc4Type = numOscTypes - 1;
      if (osc4Type >= numOscTypes) osc4Type = 0;
    } else if (editMode && currentMenu == 8 && currentSelection == 0) {
      osc5Type += val;
      if (osc5Type < 0) osc5Type = numOscTypes - 1;
      if (osc5Type >= numOscTypes) osc5Type = 0;
    } else if (editMode && currentMenu == 9 && currentSelection == 0) {
      osc6Type += val;
      if (osc6Type < 0) osc6Type = numOscTypes - 1;
      if (osc6Type >= numOscTypes) osc6Type = 0;
    } else if (editMode && currentMenu == 5 && currentSelection == 3) {
      if (val != 0) osc0Chains[1] ^= 1;
    } else if (editMode && currentMenu == 6 && currentSelection == 3) {
      if (val != 0) osc0Chains[2] ^= 1;
    } else if (editMode && currentMenu == 7 && currentSelection == 3) {
      if (val != 0) osc0Chains[3] ^= 1;
    } else if (editMode && currentMenu == 8 && currentSelection == 3) {
      if (val != 0) osc0Chains[4] ^= 1;
    } else if (editMode && currentMenu == 9 && currentSelection == 3) {
      if (val != 0) osc0Chains[5] ^= 1;
    } else {
      // Navigation mode: move cursor up/down
      currentSelection += (val > 0) ? 1 : -1;

      // Clamp selection
      if (currentSelection < 0) currentSelection = 0;
      if (currentSelection >= menus[currentMenu].numItems)
        currentSelection = menus[currentMenu].numItems - 1;

      // Scroll logic with clamping
      if (currentSelection < scrollOffset) {
        scrollOffset = currentSelection;
      } else if (currentSelection >= scrollOffset + visibleItems) {
        scrollOffset = currentSelection - visibleItems + 1;
      }

      // Clamp scroll offset
      if (scrollOffset < 0) scrollOffset = 0;
      int maxScroll = menus[currentMenu].numItems - visibleItems;
      if (maxScroll < 0) maxScroll = 0;
      if (scrollOffset > maxScroll) scrollOffset = maxScroll;
    }

  menuNeedsRedraw = true;
  }

  // --- Handle button press (encoder switch) ---
  if (digitalRead(pinSW) == LOW && (millis() - lastPress > debounce)) {
    lastPress = millis();
    // Toggle edit mode if on editable item
    if (currentMenu == 2 && currentSelection == 0) {
      editMode = !editMode;
      menuNeedsRedraw = true;
      return;
    }
    else if (currentMenu == 3 && currentSelection == 0) {
      editMode = !editMode;
      menuNeedsRedraw = true;
      return;
    }
    else if (currentMenu == 4 && currentSelection == 0) {
      editMode = !editMode;
      menuNeedsRedraw = true;
      return;
    }
    else if (currentMenu == 5 && (currentSelection == 0 || currentSelection == 3 || currentSelection == 4)) {
      editMode = !editMode;
      menuNeedsRedraw = true;
      return;
    }
    else if (currentMenu == 6 && (currentSelection == 0 || currentSelection == 3 || currentSelection == 4)) {
      editMode = !editMode;
      menuNeedsRedraw = true;
      return;
    }
    else if (currentMenu == 7 && (currentSelection == 0 || currentSelection == 3 || currentSelection == 4)) {
      editMode = !editMode;
      menuNeedsRedraw = true;
      return;
    }
    else if (currentMenu == 8 && (currentSelection == 0 || currentSelection == 3 || currentSelection == 4)) {
      editMode = !editMode;
      menuNeedsRedraw = true;
      return;
    }
    else if (currentMenu == 9 && (currentSelection == 0 || currentSelection == 3 || currentSelection == 4)) {
      editMode = !editMode;
      menuNeedsRedraw = true;
      return;
    }
    else if (currentMenu == 10 && (currentSelection == 0 || currentSelection == 2 || currentSelection == 3)) {
      editMode = !editMode;
      menuNeedsRedraw = true;
      return;
    }
    const char* choice = menus[currentMenu].items[currentSelection];
    if (strcmp(choice, "Settings") == 0) {
      currentMenu = 1;
      currentSelection = 0;
      scrollOffset = 0;
    } else if (strcmp(choice, "Patch") == 0) {
      currentMenu = 2;
      currentSelection = 0;
      scrollOffset = 0;
    } else if (strcmp(choice, "User Patch") == 0) {
      currentMenu = 3;
      currentSelection = 0;
      scrollOffset = 0;
    } else if (strcmp(choice, "OSC 1") == 0) {
      currentMenu = 4;
      currentSelection = 0;
      scrollOffset = 0;
    } else if (strcmp(choice, "OSC 2") == 0) {
      currentMenu = 5;
      currentSelection = 0;
      scrollOffset = 0;
    } else if (strcmp(choice, "OSC 3") == 0) {
      currentMenu = 6;
      currentSelection = 0;
      scrollOffset = 0;
    } else if (strcmp(choice, "OSC 4") == 0) {
      currentMenu = 7;
      currentSelection = 0;
      scrollOffset = 0;
    } else if (strcmp(choice, "OSC 5") == 0) {
      currentMenu = 8;
      currentSelection = 0;
      scrollOffset = 0;
    } else if (strcmp(choice, "OSC 6") == 0) {
      currentMenu = 9;
      currentSelection = 0;
      scrollOffset = 0;
    } else if (strcmp(choice, "ENV 1") == 0 && currentMenu == 4) {
      currentMenu = 10;
      selectOscillator(0);
      currentSelection = 0;
      scrollOffset = 0;
    } else if (strcmp(choice, "ENV 2") == 0 && currentMenu == 4) {
      currentMenu = 11;
      currentSelection = 0;
      scrollOffset = 0;
    } else if (strcmp(choice, "ENV 1") == 0 && currentMenu == 5) {
      currentMenu = 12;
      currentSelection = 0;
      scrollOffset = 0;
    } else if (strcmp(choice, "ENV 2") == 0 && currentMenu == 5) {
      currentMenu = 13;
      currentSelection = 0;
      scrollOffset = 0;
    } else if (strcmp(choice, "ENV 1") == 0 && currentMenu == 6) {
      currentMenu = 14;
      currentSelection = 0;
      scrollOffset = 0;
    } else if (strcmp(choice, "ENV 2") == 0 && currentMenu == 6) {
      currentMenu = 15;
      currentSelection = 0;
      scrollOffset = 0;
    } else if (strcmp(choice, "ENV 1") == 0 && currentMenu == 7) {
      currentMenu = 16;
      currentSelection = 0;
      scrollOffset = 0;
    } else if (strcmp(choice, "ENV 2") == 0 && currentMenu == 7) {
      currentMenu = 17;
      currentSelection = 0;
      scrollOffset = 0;
    } else if (strcmp(choice, "ENV 1") == 0 && currentMenu == 8) {
      currentMenu = 18;
      currentSelection = 0;
      scrollOffset = 0;
    } else if (strcmp(choice, "ENV 2") == 0 && currentMenu == 8) {
      currentMenu = 19;
      currentSelection = 0;
      scrollOffset = 0;
    } else if (strcmp(choice, "ENV 1") == 0 && currentMenu == 9) {
      currentMenu = 20;
      currentSelection = 0;
      scrollOffset = 0;
    } else if (strcmp(choice, "ENV 2") == 0 && currentMenu == 9) {
      currentMenu = 21;
      currentSelection = 0;
      scrollOffset = 0;
    } else if (strcmp(choice, "LFO") == 0) {
      currentMenu = 22;
      currentSelection = 0;
      scrollOffset = 0;
    } else if (strcmp(choice, "Filter Cutoff") == 0) {
      currentMenu = 23;
      currentSelection = 0;
      scrollOffset = 0;
    } else if (strcmp(choice, "Resonance") == 0) {
      currentMenu = 24;
      currentSelection = 0;
      scrollOffset = 0;
    } else if (strcmp(choice, "Save") == 0) {
      // Save user patch
    } else if (strcmp(choice, "Back") == 0) {
      currentMenu = menus[currentMenu].parent;
      currentSelection = 0;
      scrollOffset = 0;
    } else {
      showMessage(choice);
    }
    editMode = false; // always exit edit mode after navigating
    menuNeedsRedraw = true;
    updateUserPatch();
  }
}

void drawEnvelope(const Envelope& e) {

    float totalTime = 0.0f;
    for (int i = 0; i < 4; i++) {
        totalTime += max(e.time[i], 1.0f);
    }

    int x = 0;
    int y = levelToY(0.0f);  // start at zero level

    for (int i = 0; i < 4; i++) {

        float segTime = max(e.time[i], 1.0f);
        int nextX = x + (segTime / totalTime) * (SCREEN_WIDTH - 1);
        int nextY = levelToY(e.level[i]);

        display.drawLine(x, y, nextX, nextY, SSD1306_WHITE);
        display.fillCircle(nextX, nextY, 2, SSD1306_WHITE);

        x = nextX;
        y = nextY;
    }

    // Final release to zero
    display.drawLine(
        x,
        y,
        SCREEN_WIDTH - 1,
        levelToY(0.0f),
        SSD1306_WHITE
    );
}

void printEnvelope(uint8_t osc) {
  if (osc >= NUM_OSC) return;

  Envelope& e = env[osc];

  Serial.print("OSC ");
  Serial.print(osc);
  Serial.println(" envelope:");

  for (int i = 0; i < NUM_BREAKPOINTS; i++) {
    Serial.print("  BP ");
    Serial.print(i);
    Serial.print("  time=");
    Serial.print(e.time[i], 3);
    Serial.print("  level=");
    Serial.println(e.level[i], 3);
  }
}

void updateEnvelopeFromPots(bool shiftHeld) {
    Envelope& e = env[activeOsc];

    for (int i = 0; i < NUM_POTS; i++) {
        int raw = analogRead(potPin[i]);
        float norm = raw / 1023.0f; // normalize to 0–1

        if (shiftHeld) {
            if (!levelPickup[i].update(raw)) continue;
            e.level[i] = norm;
        }
        else {
            if (!timePickup[i].update(raw)) continue;
            e.time[i] = norm * 1000.0f; // or whatever scaling
        }
    }
    printEnvelope(activeOsc);
}

void drawMenu() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println(menus[currentMenu].title);
  display.drawLine(0, 10, SCREEN_WIDTH, 10, SSD1306_WHITE);

  for (int i = scrollOffset; i < menus[currentMenu].numItems && i < scrollOffset + visibleItems; i++) {
    bool selected = (i == currentSelection);
    int y = 12 + (i - scrollOffset) * 10;
    if (selected) {
      display.fillRect(0, y, SCREEN_WIDTH, 10, SSD1306_WHITE);
      display.setTextColor(SSD1306_BLACK);
    } else {
      display.setTextColor(SSD1306_WHITE);
    }
    display.setCursor(2, y);
    // --- Patch submenu dynamic patch number ---
    if (currentMenu == 2 && i == 0) {
      if (editMode && currentSelection==0) {
        char buf[20];
        snprintf(buf, sizeof(buf), "Patch: [%02d]", patchNumber);
        display.println(buf);
      } else {
        display.print("Patch: ");
        display.println(patchNumber);
      }
    // --- User Patch: Patch Number ---
    } else if (currentMenu == 3 && i == 0) {
      if (editMode && currentSelection==0) {
        char buf[20];
        snprintf(buf, sizeof(buf), "Patch: [%02d]", patchNumber);
        display.println(buf);
      } else {
        display.print("Patch: ");
        display.println(patchNumber);
      }
    // --- User Patch: OSC 1 type ---
    } else if (currentMenu == 4 && i == 0) {
      if (editMode && currentSelection==0) {
        display.print("Wave: [");
        display.print(oscTypes[osc1Type]);
        display.println("]");
      } else {
        display.print("Wave: ");
        display.println(oscTypes[osc1Type]);
      }
    // --- User Patch: OSC 2 type ---
    } else if (currentMenu == 5 && i == 0) {
      if (editMode && currentSelection==0) {
        display.print("Wave: [");
        display.print(oscTypes[osc2Type]);
        display.println("]");
      } else {
        display.print("Wave: ");
        display.println(oscTypes[osc2Type]);
      }
    } else if (currentMenu == 5 && i == 3) {
      if (editMode && currentSelection==3) {
        display.print("OSC 1 Chain: [");
        display.print(osc0Chains[1]);
        display.println("]");
      } else {
        display.print("OSC 1 Chain: ");
        display.println(osc0Chains[1]);
      }
    // --- User Patch: OSC 3 type ---  
    } else if (currentMenu == 6 && i == 0) {
      if (editMode && currentSelection==0) {
        display.print("Wave: [");
        display.print(oscTypes[osc3Type]);
        display.println("]");
      } else {
        display.print("Wave: ");
        display.println(oscTypes[osc3Type]);
      }
    } else if (currentMenu == 6 && i == 3) {
      if (editMode && currentSelection==3) {
        display.print("OSC 1 Chain: [");
        display.print(osc0Chains[2]);
        display.println("]");
      } else {
        display.print("OSC 1 Chain: ");
        display.println(osc0Chains[2]);
      }
    // --- User Patch: OSC 4 type ---
    } else if (currentMenu == 7 && i == 0) {
      if (editMode && currentSelection==0) {
        display.print("Wave: [");
        display.print(oscTypes[osc4Type]);
        display.println("]");
      } else {
        display.print("Wave: ");
        display.println(oscTypes[osc4Type]);
      }
    } else if (currentMenu == 7 && i == 3) {
      if (editMode && currentSelection==3) {
        display.print("OSC 1 Chain: [");
        display.print(osc0Chains[3]);
        display.println("]");
      } else {
        display.print("OSC 1 Chain: ");
        display.println(osc0Chains[3]);
      }
    // --- User Patch: OSC 5 type ---
    } else if (currentMenu == 8 && i == 0) {
      if (editMode && currentSelection==0) {
        display.print("Wave: [");
        display.print(oscTypes[osc5Type]);
        display.println("]");
      } else {
        display.print("Wave: ");
        display.println(oscTypes[osc5Type]);
      }
    // --- User Patch: OSC 6 type ---
    } else if (currentMenu == 8 && i == 3) {
      if (editMode && currentSelection==3) {
        display.print("OSC 1 Chain: [");
        display.print(osc0Chains[4]);
        display.println("]");
      } else {
        display.print("OSC 1 Chain: ");
        display.println(osc0Chains[4]);
      }
    } else if (currentMenu == 9 && i == 0) {
      if (editMode && currentSelection==0) {
        display.print("Wave: [");
        display.print(oscTypes[osc6Type]);
        display.println("]");
      } else {
        display.print("Wave: ");
        display.println(oscTypes[osc6Type]);
      }
    } else if (currentMenu == 9 && i == 3) {
      if (editMode && currentSelection==3) {
        display.print("OSC 1 Chain: [");
        display.print(osc0Chains[5]);
        display.println("]");
      } else {
        display.print("OSC 1 Chain: ");
        display.println(osc0Chains[5]);
      }
    } else if (currentMenu == 10) {
        drawEnvelope(env[activeOsc]);
        if (shiftHeld)
          display.drawRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, SSD1306_WHITE);
    // --- Default item rendering ---
    } else {
      display.println(menus[currentMenu].items[i]);
    }
  }
  display.display();
}

void showMessage(const char* msg) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 20);
  display.print("You selected:");
  display.setCursor(0, 35);
  display.println(msg);
  display.display();
  delay(1000);
}

void setup() {
  pinMode(octUp, INPUT_PULLUP);
  pinMode(shiftKey, INPUT_PULLUP);
  pinMode(octDown, INPUT_PULLUP);
  Wire.begin(40, 39);
  // Wire.setClock(100000);
  pinMode(pinA, INPUT_PULLUP);
  pinMode(pinB, INPUT_PULLUP);
  pinMode(pinSW, INPUT_PULLUP);
  selectOscillator(0);
  Serial.begin(115200);
  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;); // Loop forever
  }
  display.clearDisplay();
  display.display();
  delay(2000);
  Serial.println("AMY_Synth");
  if (!mcp.begin_I2C(0x20)) {
    Serial.println("Error.");
    while (1);
  }
  for (uint8_t pin : { LPLED, BPLED, HPLED }) {
    mcp.pinMode(pin, OUTPUT);
  }
  mcp.digitalWrite(LPLED, HIGH);
  mcp.digitalWrite(BPLED, LOW);
  mcp.digitalWrite(HPLED, LOW);
  mcp.pinMode(FILTERTYPE, INPUT_PULLUP);
  for (int i = 0; i < numKeys; i++) {
    mcp.pinMode(keyPins[i], INPUT_PULLUP);
    keyState[i] = mcp.digitalRead(keyPins[i]);
    lastKeyState[i] = keyState[i];
  }
  
  amy_config_t amy_config = amy_default_config();
  amy_config.features.startup_bleep = 0;
  // Install the default_synths on synths (MIDI chans) 1, 2, and 10 (this is the default).
  amy_config.features.default_synths = 1;
  // If you want MIDI over UART (5-pin or 3-pin serial MIDI)
  amy_config.midi = AMY_MIDI_IS_UART;
  // amy_config.midi_out = 17;
  amy_config.midi_in = 15;
  amy_config.i2s_bclk = 35;
  amy_config.i2s_lrc = 36;
  amy_config.i2s_dout = 21;
  amy_config.audio = AMY_AUDIO_IS_I2S;
  amy_start(amy_config);
  // amy_live_start();
  amy_event e = amy_default_event();
  e.synth = 1;
  e.filter_type = 1;
  e.num_voices = 6;
  e.patch_number = 0;
  amy_add_event(&e);
}

float fmap(float x, float in_min, float in_max, float out_min, float out_max) {
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

static long last_millis = 0;
static const long millis_interval = 250;
static bool led_state = 0;

unsigned long lastEnvUpdate = 0;
const unsigned long envInterval = 250;  // 250 ms

void loop() {
  bool octDownState = digitalRead(octDown);
  bool octUpState   = digitalRead(octUp);
  bool shiftHeld = digitalRead(shiftKey) == LOW;

  // Octave down (falling edge)
  if (lastOctDownState == HIGH && octDownState == LOW) {
    baseNote -= 12;
    if (baseNote < 12) baseNote = 12;
  }

  // Octave up (falling edge)
  if (lastOctUpState == HIGH && octUpState == LOW) {
    baseNote += 12;
    if (baseNote > 120) baseNote = 120;
  }

  lastOctDownState = octDownState;
  lastOctUpState   = octUpState;
  handleEncoderMenu();
  // ~ cpu usage
  // loopCounter++;
  // if (millis() - lastPrint >= 1000) {
  //   Serial.print("Loops per second: ");
  //   Serial.println(loopCounter);

  //   loopCounter = 0;
  //   lastPrint = millis();
  // }
  //
  if (currentMenu >= 10 && currentMenu <= 21) {
    updateEnvelopeFromPots(shiftHeld);
    menuNeedsRedraw = true;
  }
  if (menuNeedsRedraw) {
    drawMenu();
    menuNeedsRedraw = false;
  }
  filterState = mcp.digitalRead(FILTERTYPE);
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
    keyState[i] = mcp.digitalRead(keyPins[i]);
    if (keyState[i] != lastKeyState[i]) {
      delay(10);
      keyState[i] = mcp.digitalRead(keyPins[i]);
      if (keyState[i] != lastKeyState[i]) {
        lastKeyState[i] = keyState[i];
        if (keyState[i] == LOW) {
          playNote(i);
          // Serial.print("Key ");
          // Serial.print(i);
          // Serial.print(" pressed");
          // Serial.println("keystate");
          // Serial.print(keyState[i]);
        }
        else {
          noteOff(i);
          // Serial.print("Key ");
          // Serial.print(i);
          // Serial.print(" released");
          // Serial.println("keystate");
          // Serial.print(keyState[i]);
        }
      }
    }
  }
  amy_update();
  resonancePotValue = analogRead(resonancePot);
  cutoffPotValue = analogRead(cutoffPot);

  // --- Read and average joystick X ---
  long totalX = 0;
  for (int i = 0; i < samplesX; i++) {
    totalX += analogRead(stickX);
  }
  float newStickXValue = totalX / (float)samplesX;

  // --- Read and average joystick Y ---
  long totalY = 0;
  for (int i = 0; i < samplesY; i++) {
    totalY += analogRead(stickY);
  }
  float newStickYValue = totalY / (float)samplesY;

  // // --- Read and average attack pot ---
  // long totalAttack = 0;
  // for (int i = 0; i < samplesAttack; i++) {
  //   totalAttack += analogRead(attackPot);
  // }
  // float newAttackValue = totalAttack / (float)samplesAttack;
  // // --- Read and average decay pot ---
  // long totalDecay = 0;
  // for (int i = 0; i < samplesDecay; i++) {
  //   totalDecay += analogRead(decayPot);
  // }
  // float newDecayValue = totalDecay / (float)samplesDecay;
  // // --- Read and average sustain pot ---
  // long totalSustain = 0;
  // for (int i = 0; i < samplesSustain; i++) {
  //   totalSustain += analogRead(sustainPot);
  // }
  // float newSustainValue = totalSustain / (float)samplesSustain;
  // // --- Read and average release pot ---
  // long totalRelease = 0;
  // for (int i = 0; i < samplesRelease; i++) {
  //   totalRelease += analogRead(releasePot);
  // }
  // float newReleaseValue = totalRelease / (float)samplesRelease;
  // --- Read and average volume pot ---
  long totalVolume = 0;
  for (int i = 0; i < samplesVolume; i++) {
    totalVolume += analogRead(volumePot);
  }
  float newVolumeValue = totalVolume / (float)samplesVolume;
  
  // --- Exponential smoothing (low-pass filter) ---
  stickXValue = (stickXValue * 0.85f) + (newStickXValue * 0.15f);
  stickYValue = (stickYValue * 0.85f) + (newStickYValue * 0.15f);
  //attackPotValue = (attackPotValue * 0.85f) + (newAttackValue * 0.15f);
  //decayPotValue = (decayPotValue * 0.85f) + (newDecayValue * 0.15f);
  //sustainPotValue = (sustainPotValue * 0.85f) + (newSustainValue * 0.15f);
  //releasePotValue = (releasePotValue * 0.85f) + (newReleaseValue * 0.15f);
  volumePotValue = (volumePotValue * 0.85f) + (newVolumeValue * 0.15f);

  // --- Map and process controls ---
  float baseCutoff = 60.0f + (cutoffPotValue / 4095.0f) * (16000.0f - 60.0f);
  float modCutoff  = fmap(stickXValue, 0.0f, 4095.0f, -8000.0f, 8000.0f);
  float targetCutoff = baseCutoff + modCutoff;

  if (targetCutoff < 200.0f) targetCutoff = 200.0f;
  if (targetCutoff > 16000.0f) targetCutoff = 16000.0f;

  cutoff += (targetCutoff - cutoff) * 0.1f;

  float resonance = 0.7f + (resonancePotValue / 4095.0f) * (16.0f - 0.7f);

  if (abs(volumePotValue - lastVolumePot) > 25) {
    lastVolumePot = volumePotValue;
    volume = (volumePotValue / 4095.0f) * 10.0f;
  }

  a = (attackPotValue * 7000) / 4095 - 50;
  if (a < 0) a = 0;
  b = (decayPotValue * 7000) / 4095 - 30;
  if (b < 0) b =0;
  c = (sustainPotValue * 7000) / 4095 - 30;
  if (c < 0) c = 0;
  d = (releasePotValue * 7000) / 4095 - 30;
  if (d < 0) d = 0;

  snprintf(
    envelope,
    sizeof(envelope),
    "%d,1,%d,0.5,%d,0.4,%d,0",
    a, b, c, d
  );
  // Serial.println(envelope);
  Serial.println(cutoff);
  float targetPitchBend = fmap(stickYValue, 0.0f, 4095.0f, 1.5f, -1.5f);
  pitchBend += (targetPitchBend - pitchBend) * smoothing;

  // if (patchNumber > 300) {
  //   updateEnvelope();
  // }
  updateEnvelope();
  // amy_event e = amy_default_event();
  amy_event e = amy_default_event();
  e.synth = 1;             // target same osc
  e.filter_type = filterType; 
  e.filter_freq_coefs[0] = cutoff;
  e.volume = volume;
  e.resonance = resonance; 
  e.pitch_bend = pitchBend;
  amy_add_event(&e);

}

