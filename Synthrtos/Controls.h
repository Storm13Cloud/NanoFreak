#ifndef CONTROLS_H
#define CONTROLS_H

bool checkADSRChanged();
void updateKnobs();
void updatePreset();
void updateEnvelope();
void printMemoryDiagnostics();
void changePresetAndKeepEnvelope(int direction);

#endif