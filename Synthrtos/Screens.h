#ifndef SCREENS_H
#define SCREENS_H

#include <Arduino.h>

enum ScreenType {
  SCREEN_PRESET = 0,
  SCREEN_USER,
  SCREEN_SEQ,
  SCREEN_SETTINGS,
  
  SCREEN_PRESET_SUB_EDIT,
  SCREEN_PRESET_SUB_SAVE,
  SCREEN_PRESET_SUB_DELETE,
  
  SCREEN_PRESET_VOLUME_CONTROL,
  
  SCREEN_SEQ_TEMPO,
  SCREEN_SEQ_LENGTH,
  
  NUM_SCREENS
};

extern int currentScreen;
extern bool isEditingValue; // <-- Added: Tracks if we are actively editing a parameter
extern int presetNumber;    // <-- Added: Tracks the active synth patch number
extern int seqTempo;        // <-- Added: Tracks sequencer BPM
extern int seqLength;       // <-- Added: Tracks sequencer step count

// Core navigation API
void handleScreenNavigation(int8_t direction);
void handleScreenClick();
void handleScreenHold();
const unsigned char* getCurrentScreenBitmap();
const char* getScreenName(int screenIndex);

#endif