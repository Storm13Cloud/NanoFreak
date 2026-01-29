#ifndef POTS_H
#define POTS_H

#include "SoftPickup.h"
#include <Arduino.h>

const uint8_t NUM_POTS = 4;

//input pins
extern const uint8_t potPin[NUM_POTS];

//object for each pot
extern SoftPickup pots[NUM_POTS];

#endif