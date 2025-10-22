#include <AMY-Arduino.h>
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_MCP23X17.h>
#include <KeyPins.h>
Adafruit_MCP23X17 mcp;

int potPin = 1;   // GPIO 39
int potValue = 0;  // Raw 0–4095
float cutoff = 0.0f;
float lastCutoff = -1.0f;
float resonance = 5.0f;  // starting resonance
float targetResonance = 5.0f;

const int keyPins[] = {KEY1, KEY2, KEY3, KEY4, KEY5, KEY6, KEY7, KEY8, KEY9, KEY10, KEY11, KEY12};
const int numKeys = 12;
bool keyState[numKeys];
bool lastKeyState[numKeys];

int midiNote = 50;
int currentNote = 0;


void test() {
  amy_event e = amy_default_event();
  e.osc = 0;
  e.wave = SAW_UP;
  e.filter_type = 1;
  amy_add_event(&e);
  e.osc = 0;
  e.filter_freq_coefs[0] = 50.0f;  // Constant term
  e.filter_freq_coefs[1] = 0.0f;   // Note
  e.filter_freq_coefs[2] = 0.0f;   // Velocity
  e.filter_freq_coefs[3] = 0.0f;   // Envelope Gen 0 (bp0)
  e.filter_freq_coefs[4] = 1.0f;
  strncpy(e.bp1, "0,6.0,1000,3.0,200,0", sizeof(e.bp1));
  // https://github.com/shorepine/amy/blob/main/docs/synth.md#ctrlcoefficients
  e.bp1[sizeof(e.bp1) - 1] = '\0';
  amy_add_event(&e);
  e.osc =0;
  e.velocity = 1;
  e.midi_note = currentNote;
  amy_add_event(&e);
}

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

void testOff() {
  amy_event e = amy_default_event();
  e.osc =0;
  e.velocity = 0;
  amy_add_event(&e);
}

void setup() {
  Wire.begin(8, 9);
  Wire.setClock(100000);
  Serial.begin(115200);
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
     targetResonance = 0.0f;
  } else {
      targetResonance = 5.0f; // default resonance
  }
    resonance = lerp(resonance, targetResonance, 0.05f);
    e.resonance = resonance;       // maintain resonance
    amy_add_event(&e);

    Serial.print("Filter cutoff: ");
    Serial.println(cutoff);
  }

 // small update interval
}
