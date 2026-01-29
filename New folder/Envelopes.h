#ifndef ENVELOPES_H
#define ENVELOPES_H

#include <Arduino.h>
#include "Pots.h"

const uint8_t NUM_OSC = 6;
const uint8_t NUM_BREAKPOINTS = 4;

struct Envelope {
  float time[NUM_BREAKPOINTS];
  float level [NUM_BREAKPOINTS];
};

//one env per osc
extern Envelope env[NUM_OSC];

//current osc
extern uint8_t activeOsc;

//call when switching osc
void selectOscillator(uint8_t newOsc);

#endif