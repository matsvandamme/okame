#pragma once
#include <Arduino.h>
#include "config.h"
#include "pet.h"

struct SavedNetwork {
  char ssid[33];
  char pass[65];
};

// Everything that survives a reboot, in NVS ("okame" namespace).
namespace storage {
void begin();
bool loadPet(PetState& out);
void savePet(const PetState& st);
void loadExtra(Pet& pet);   // 0.4 additions, their own key so a rollback leaves "pet" untouched
void saveExtra(const PetExtra& x);
int loadNetworks(SavedNetwork* out, int max);
void saveNetworks(const SavedNetwork* nets, int n);
String apPassword();   // generated once, shown on the LCD
uint32_t lastEpoch();  // best known wall-clock time, for boots without internet
void setLastEpoch(uint32_t e);
void factoryReset();
}  // namespace storage
