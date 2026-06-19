#include "Config.h"


// State arrays for 4 ADSR parameters
int timeVals[4]   = {50, 50, 50, 100};   // AttackTime, DecayTime, SustainTime, ReleaseTime+
uint16_t levelVals[4]  = {50, 45, 45, 0};   // AttackLevel, DecayLevel, SustainLevel, ReleaseLevel
// int levelMap[3] = {0, 2, 2};
bool controlling[4] = {false, false, false, false}; // is knob controlling
int oldVal[4]     = {0, 0, 0, 0};   // last value when button pressed/released
int side[4]       = {0, 0, 0, 0};   // direction tracking

bool prevButton = false;


int lastTimeVals[4] = {50, 50, 50, 100};
uint16_t lastLevelVals[4] = {50, 45, 45, 0};
bool adsrChanged = false;

bool checkADSRChanged() {
  bool changed = false;
  
  for (int i = 0; i < 4; i++) {
    if (timeVals[i] != lastTimeVals[i] || levelVals[i] != lastLevelVals[i]) {
      changed = true;
      lastTimeVals[i] = timeVals[i];
      lastLevelVals[i] = levelVals[i];
    }
  }
  
  return changed;
}

void updateKnobs() {
  bool button = !digitalRead(PIN_SHIFT); // active-low
  bool buttonDelta = (button != prevButton);

  for (int i = 3; i < 7; i++) {
    int idx = i - 3; // Map pot index (3 to 6) to array index (0 to 3)
    int potRaw = finalValues[i]; 
    int levelPot = (potRaw * 100 + 2047) / 4095;  // 0–100

    if (buttonDelta) {
      controlling[idx] = false;
      oldVal[idx] = button ? levelVals[idx] : timeVals[idx];
      side[idx] = button ? (levelPot - levelVals[idx]) : (potRaw - (timeVals[idx] * 4095) / 7000);
    }

    if (controlling[idx]) {
      if (button) {
        levelVals[idx] = levelPot;           // 0–100
      } else {
        timeVals[idx] = (potRaw * 7000 + 2047) / 4095;
      }
    } else {
      if (button) {
        if (side[idx] * (levelPot - levelVals[idx]) <= 0)
          controlling[idx] = true;
      } else {
        if (side[idx] * (potRaw - (timeVals[idx] * 4095) / 7000) <= 0)
          controlling[idx] = true;
      }
    }
  }

  prevButton = button;
}

void updatePreset() {
  amy_event e = amy_default_event();
  e.patch_number = presetNumber;
  Serial.println("Patch reset");
  e.reset_osc = RESET_ALL_OSCS;
  amy_add_event(&e);
  e = amy_default_event();
  e.synth = 1;
  e.filter_type = FILTER_LPF;
  e.num_voices = 6;
  e.patch_number = presetNumber;
  amy_add_event(&e);
}

void updateEnvelope() {
  amy_event e = amy_default_event();
  e.synth = 1;
  e.osc = 0;
  for (int i = 0; i < 4; i++) {
    Serial.println(timeVals[i]);
    e.eg0_times[i] = timeVals[i];
    e.eg0_values[i] = levelVals[i] / 100.0f;
  }
  amy_add_event(&e);
}

void changePresetAndKeepEnvelope(int direction) {
  // 1. Copy the current envelope values of the active preset before it changes
  int savedTimeVals[4];
  uint16_t savedLevelVals[4];
  for (int i = 0; i < 4; i++) {
    savedTimeVals[i] = timeVals[i];
    savedLevelVals[i] = levelVals[i];
  }

  // 2. Change the preset number
  presetNumber += direction;
  presetNumber = constrain(presetNumber, 0, 256); // Standard MIDI program range
  Serial.print("New Preset: ");
  Serial.println(presetNumber);

  // 3. Reset oscillators and load the new preset in AMY
  updatePreset();

  // 4. Restore the copied values back to the active variables
  for (int i = 0; i < 4; i++) {
    timeVals[i] = savedTimeVals[i];
    levelVals[i] = savedLevelVals[i];
    
    // Sync tracking variables so checkADSRChanged() recognizes them as current
    lastTimeVals[i] = savedTimeVals[i];
    lastLevelVals[i] = savedLevelVals[i];
  }

  // 5. Re-apply the copied envelope values to the new preset template in AMY
  updateEnvelope();

  // 6. Re-apply the physical filter settings
  float cutoffHz = map(finalValues[IDX_POT_CUTOFF], 0, 4095, 50, 8000);
  float resonance = map(finalValues[IDX_POT_RES], 0, 4095, 5, 160) / 10.0;

  amy_event e = amy_default_event();
  e.synth = 1;
  e.filter_freq_coefs[0] = cutoffHz;
  e.resonance = resonance;
  amy_add_event(&e);
}

void printMemoryDiagnostics() {
  Serial.println("\n=== MEMORY DIAGNOSTICS ===");
  
  // 1. Internal SRAM (Fast, internal memory)
  uint32_t totalSram = ESP.getHeapSize();
  uint32_t freeSram  = ESP.getFreeHeap();
  uint32_t usedSram  = totalSram - freeSram;
  uint32_t minSram   = ESP.getMinFreeHeap(); // Lowest free SRAM recorded since boot
  
  Serial.println("SRAM (Internal Fast RAM):");
  Serial.printf("  Total Heap: %u bytes (%.1f KB)\n", totalSram, totalSram / 1024.0);
  Serial.printf("  Free Heap:  %u bytes (%.1f KB)\n", freeSram, freeSram / 1024.0);
  Serial.printf("  Used Heap:  %u bytes (%.1f KB)\n", usedSram, usedSram / 1024.0);
  Serial.printf("  Min Free Ever: %u bytes (%.1f KB)\n\n", minSram, minSram / 1024.0);

  // 2. External PSRAM (Slower, massive external memory)
  if (psramFound()) { // Check if the IDE has successfully mounted the PSRAM
    uint32_t totalPsram = ESP.getPsramSize();
    uint32_t freePsram  = ESP.getFreePsram();
    uint32_t usedPsram  = totalPsram - freePsram;
    uint32_t minPsram   = ESP.getMinFreePsram(); // Lowest free PSRAM recorded since boot

    Serial.println("PSRAM (External Octal RAM):");
    Serial.printf("  Total Size: %u bytes (%.2f MB)\n", totalPsram, totalPsram / 1024.0 / 1024.0);
    Serial.printf("  Free Size:  %u bytes (%.2f MB)\n", freePsram, freePsram / 1024.0 / 1024.0);
    Serial.printf("  Used Size:  %u bytes (%.2f MB)\n", usedPsram, usedPsram / 1024.0 / 1024.0);
    Serial.printf("  Min Free Ever: %u bytes (%.2f MB)\n", minPsram, minPsram / 1024.0 / 1024.0);
  } else {
    Serial.println("PSRAM: Not detected or not enabled in Tools settings!");
  }
  Serial.println("==========================\n");
}
