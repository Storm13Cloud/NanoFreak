#include "Envelopes.h"

//init envelopes
Envelope env[NUM_OSC]= {0};
uint8_t activeOsc = 0;

//switch osc and reset pickups
void selectOscillator(uint8_t newOsc) {
  if (newOsc >= NUM_OSC) return;
  activeOsc = newOsc;

  //reset pickups for the new osc
  for (int i = 0; i < NUM_POTS; i++) {
    pots[i].reset(analogRead(potPin[i]));
  }
}