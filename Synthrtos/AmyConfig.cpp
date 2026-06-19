#include "Config.h"
#include <Arduino.h>
#include <AMY-Arduino.h> 

int baseNote = 60; 

// Track the active MIDI note playing on each of the 12 keys
int activeNotes[12] = {0}; 

void setupAmy() {
  amy_config_t amy_config = amy_default_config();
  amy_config.features.startup_bleep = 0;
  amy_config.features.default_synths = 1;
  amy_config.midi = AMY_MIDI_IS_UART;
  amy_config.midi_in = 15;
  
  #ifndef AMYBOARD_ARDUINO
  amy_config.audio = AMY_AUDIO_IS_I2S;
  amy_config.i2s_bclk = 35;
  amy_config.i2s_lrc = 36;
  amy_config.i2s_dout = 21;
  #endif
  
  amy_config.platform.multicore = 1;
  amy_config.platform.multithread = 1;
  
  amy_start(amy_config);
  
  amy_event e = amy_default_event();
  e.synth = 1;
  e.num_voices = 6;
  e.volume[0] = 0.6f;
  e.patch_number = 0;
  e.filter_type = FILTER_LPF;
  amy_add_event(&e);
}

void playNote(int i) {
  if (i < 0 || i >= 12) return;

  int targetNote = baseNote + i;
  activeNotes[i] = targetNote; // Save the playing note

  amy_event e = amy_default_event();
  e.synth = 1;
  e.midi_note = targetNote;
  e.velocity = 1;
  amy_add_event(&e);
}

void noteOff(int i) {
  if (i < 0 || i >= 12) return;

  // Use the exact note that was stored when the key was pressed
  int targetNote = activeNotes[i]; 
  
  if (targetNote > 0) {
    amy_event e = amy_default_event();
    e.synth = 1;
    e.midi_note = targetNote;
    e.velocity = 0;
    amy_add_event(&e);
    
    activeNotes[i] = 0; // Clear slot
  }
}

void updateSynthesizerControls() {
  // 1. Process Filter Cutoff & Resonance
  float cutoffHz = map(finalValues[IDX_POT_CUTOFF], 0, 4095, 70, 8000);
  float resonance = map(finalValues[IDX_POT_RES], 0, 4095, 5, 160) / 10.0;
  Serial.println(cutoffHz);
  
  amy_event e = amy_default_event();
  e.synth = 1;
  e.filter_freq_coefs[0] = cutoffHz;
  e.resonance = resonance;
  amy_add_event(&e);

}