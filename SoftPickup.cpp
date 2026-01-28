#include "SoftPickup.h"

SoftPickup::SoftPickup(int dz) {
  deadzone = dz;
  lastRaw = 0;
  active = true; // start active on boot
}

bool SoftPickup::update(int raw) {
  if (!active) {
    if (abs(raw - lastRaw) <= deadzone) {
      active = true; //pot caught up
    }
    else {
      return false; // ignore until pickup
    }
  }
  lastRaw = raw;
  return true;
}

void SoftPickup::reset(int raw) {
  active = false;
  lastRaw = raw;
}