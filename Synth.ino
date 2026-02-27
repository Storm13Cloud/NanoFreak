// Global variables
const unsigned long envelopeInterval = 200;
const int DEAD_ZONE = 15;

int lastProcessedTimeVals[4] = {50, 50, 50, 100};
int lastProcessedLevelVals[4] = {50, 45, 25, 0};
int side[4] = {0, 0, 0, 0};

void updateKnobs() {
    // New implementation using DEAD_ZONE constant
    // Filtering jitter logic here
}

