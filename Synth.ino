#include <AMY-Arduino.h>
#include <Arduino.h>

int potPin = 1;   // GPIO 39
int potValue = 0;  // Raw 0–4095
float cutoff = 0.0f;
float lastCutoff = -1.0f;
float resonance = 5.0f;  // starting resonance
float targetResonance = 5.0f;


void test() {
  amy_event e = amy_default_event();
  e.osc = 0;
  e.wave = SAW_UP;
  //e.resonance = 5;
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
  e.midi_note = 50;
  amy_add_event(&e);
}

void setup() {
  Serial.begin(115200);
  Serial.println("AMY_Synth");

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
  test();
}
static long last_millis = 0;
static const long millis_interval = 250;
static bool led_state = 0;

void loop() {
  // Your loop() must contain this call to amy:
  amy_update();
  potValue = analogRead(potPin);
  cutoff = 100.0f + (potValue / 4095.0f) * (5000.0f - 100.0f);

  // Only update if significant change
  if (fabs(cutoff - lastCutoff) > 5.0f) {
    lastCutoff = cutoff;

    // Send update event to existing osc
    amy_event e = amy_default_event();
    e.osc = 0;             // target same osc
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
