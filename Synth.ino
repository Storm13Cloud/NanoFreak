#include <AMY-Arduino.h>
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_MCP23X17.h>
#include <KeyPins.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
Adafruit_MCP23X17 mcp;

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);


int potPin = 1;   // GPIO 39
int potValue = 0;  // Raw 0–4095
float cutoff = 0.0f;
float lastCutoff = -1.0f;
float resonance = 5.0f;  // starting resonance
float targetResonance = 5.0f;

const int upButton = 2;
const int selectButton = 38;
const int downButton = 37;

const int keyPins[] = {KEY1, KEY2, KEY3, KEY4, KEY5, KEY6, KEY7, KEY8, KEY9, KEY10, KEY11, KEY12};
const int numKeys = 12;
bool keyState[numKeys];
bool lastKeyState[numKeys];

int midiNote = 50;
int currentNote = 0;

// Debounce timing
unsigned long lastButtonPress = 0;
const int debounceDelay = 200;

// Menu state
int currentMenu = 0;
int currentSelection = 0;
int patchNumber = 1;

struct Menu {
  const char* title;
  const char* items[5];
  int numItems;
  int parent; // index of parent menu (-1 if root)
};

Menu menus[] = {
  {"Main Menu", {"Patch", "Option 2", "Settings"}, 3, -1},         // 0
  {"Settings", {"Brightness", "Volume", "Back"}, 3, 0},               // 1
  {"Patch", {"Patch Number", "Back"}, 2, 0},                                   // 2
};

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


void playNote(int i) {
  int baseNote = 50;
  amy_event e = amy_default_event();
  e.synth = 1;
  e.midi_note = baseNote + i;
  e.velocity = 1;
  amy_add_event(&e);
}

void noteOff(int i) {
  int baseNote = 50;
  amy_event e = amy_default_event();
  e.synth = 1;
  e.midi_note = baseNote + i;
  e.velocity = 0;
  amy_add_event(&e);
}

void handleButtons() {
  if (millis() - lastButtonPress < debounceDelay) return;

  // ----- UP BUTTON -----
  if (digitalRead(upButton) == LOW) {
    lastButtonPress = millis();

    if (editMode && currentMenu == 2 && currentSelection == 0) {
      // In edit mode: change patch number
      patchNumber++;
      if (patchNumber > 99) patchNumber = 99;
    } else {
      // Navigation mode: move cursor up
      currentSelection--;
      if (currentSelection < 0) currentSelection = menus[currentMenu].numItems - 1;
    }

    menuNeedsRedraw = true;
  }

  // ----- DOWN BUTTON -----
  if (digitalRead(downButton) == LOW) {
    lastButtonPress = millis();

    if (editMode && currentMenu == 2 && currentSelection == 0) {
      // In edit mode: change patch number
      patchNumber--;
      if (patchNumber < 1) patchNumber = 1;
    } else {
      // Navigation mode: move cursor down
      currentSelection++;
      if (currentSelection >= menus[currentMenu].numItems) currentSelection = 0;
    }

    menuNeedsRedraw = true;
  }

  // ----- SELECT BUTTON -----
  if (digitalRead(selectButton) == LOW) {
    lastButtonPress = millis();

    // If we're on the Patch menu's "Patch Number" entry, toggle edit mode
    if (currentMenu == 2 && currentSelection == 0) {
      editMode = !editMode;           // enter/exit editing
      menuNeedsRedraw = true;
      return;
    }

    const char* choice = menus[currentMenu].items[currentSelection];

    if (strcmp(choice, "Settings") == 0) {
      currentMenu = 1;
      currentSelection = 0;
    } else if (strcmp(choice, "Patch") == 0) {
      currentMenu = 2;
      currentSelection = 0;
    } else if (strcmp(choice, "Back") == 0) {
      currentMenu = menus[currentMenu].parent;
      currentSelection = 0;
    } else {
      showMessage(choice);
    }

    // exiting edit mode if we changed menu or pressed back
    editMode = false;
    menuNeedsRedraw = true;
  }
}

void drawMenu() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println(menus[currentMenu].title);
  display.drawLine(0, 10, SCREEN_WIDTH, 10, SSD1306_WHITE);

  for (int i = 0; i < menus[currentMenu].numItems; i++) {
    bool selected = (i == currentSelection);

    if (selected) {
      display.fillRect(0, 12 + i * 10, SCREEN_WIDTH, 10, SSD1306_WHITE);
      display.setTextColor(SSD1306_BLACK);
    } else {
      display.setTextColor(SSD1306_WHITE);
    }
    display.setCursor(2, 12 + i * 10);

    // Special case: Patch submenu shows dynamic patch number
    if (currentMenu == 2 && i == 0) {
      if (editMode) {
        // show editing indicator (brackets)
        char buf[20];
        snprintf(buf, sizeof(buf), "Patch: [%02d]", patchNumber);
        display.println(buf);
      } else {
        // normal display
        display.print("Patch: ");
        display.println(patchNumber);
      }
      amy_event e = amy_default_event();
      e.synth = 1;
      e.num_voices = 6;
      e.patch_number = patchNumber;
      amy_add_event(&e);
      Serial.println("patch is");
      Serial.println(e.patch_number);
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
  pinMode(upButton, INPUT_PULLUP);
  pinMode(selectButton, INPUT_PULLUP);
  pinMode(downButton, INPUT_PULLUP);
  Wire.begin(8, 9);
  Wire.setClock(100000);
  Serial.begin(115200);
  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;); // Don't proceed, loop forever
  }

  // Show initial display buffer contents on the screen --
  // the library initializes this with an Adafruit splash screen.
  display.clearDisplay();
  display.display();
  delay(2000); // Pause for 2 seconds

  // Clear the buffer
  

  Serial.println("AMY_Synth");
  if (!mcp.begin_I2C(0x27)) {
    Serial.println("Error.");
    while (1);
  }
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
  amy_config.i2s_bclk = 15;
  amy_config.i2s_lrc = 17;
  amy_config.i2s_dout = 10;
  amy_start(amy_config);
  amy_live_start();
  amy_event e = amy_default_event();
  e.synth = 1;
  e.num_voices = 6;
  e.patch_number = 1;
  amy_add_event(&e);
  //test();
}
static long last_millis = 0;
static const long millis_interval = 250;
static bool led_state = 0;

void loop() {
  // Your loop() must contain this call to amy:
  handleButtons();
  if (menuNeedsRedraw) {
    drawMenu();
    menuNeedsRedraw = false;
  }
  //display.display();
  for (int i = 0; i < numKeys; i++) {
    keyState[i] = mcp.digitalRead(keyPins[i]);
    if (keyState[i] != lastKeyState[i]) {
      delay(10);
      keyState[i] = mcp.digitalRead(keyPins[i]);
      if (keyState[i] != lastKeyState[i]) {
        lastKeyState[i] = keyState[i];
        if (keyState[i] == LOW) {
          playNote(i);
          Serial.print("Key ");
          Serial.print(i);
          Serial.print(" pressed");
        }
        else {
          noteOff(i);
          Serial.print("Key ");
          Serial.print(i);
          Serial.print(" released");
        }
      }
    }
  }
  //digitalRead()
  amy_update();
  potValue = analogRead(potPin);
  cutoff = 100.0f + (potValue / 4095.0f) * (5000.0f - 100.0f);

  // Only update if significant change
  if (fabs(cutoff - lastCutoff) > 5.0f) {
    lastCutoff = cutoff;

    amy_event e = amy_default_event();
    e.synth = 1;             // target same osc
    e.filter_freq_coefs[0] = cutoff;
    e.filter_type = 1;     // maintain filter type
    if (cutoff < 150.0f) {
     targetResonance = 0.5f;
  } else {
      targetResonance = 5.0f; // default resonance
  }
    resonance = lerp(resonance, targetResonance, 0.05f);
    e.resonance = resonance;       // maintain resonance
    amy_add_event(&e);

    //Serial.print("Filter cutoff: ");
    //Serial.println(cutoff);
  }

 // small update interval
}
