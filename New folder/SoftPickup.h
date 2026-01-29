#ifndef SOFTPICKUP_H
#define SOFTPICKUP_H

#include <Arduino.h>

class SoftPickup {
public:
    SoftPickup(int dz = 8); //deadzone
    //call each loop with raw ADC reading
    //returns true if param can be updated
    bool update(int raw);
    //reset pickup state
    void reset(int raw);

private:
    int lastRaw;
    bool active;
    int deadzone;
};

#endif