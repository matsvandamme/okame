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
bool hasPet();              // a bird was ever saved (so a load that fails is never a reason to start over)
bool loadPet(PetState& out);
void savePet(const PetState& st);
void loadExtra(Pet& pet);   // 0.4 additions, their own key so a rollback leaves "pet" untouched
void saveExtra(const PetExtra& x);
void loadBirds(Pet& pet);   // 0.5: species and friends, under "pet3"
void saveBirds(const PetBirds& b);
void loadLife(Pet& pet);    // 0.6: training, old age, clock and alarm, under "pet4"
void saveLife(const PetLife& l);
void loadHome(Pet& pet);    // 0.7: seeds, toys, souvenirs and the family album, under "pet5"
void saveHome(const PetHome& h);
int loadNetworks(SavedNetwork* out, int max);
void saveNetworks(const SavedNetwork* nets, int n);
String apPassword();   // generated once, shown on the LCD
uint32_t lastEpoch();  // best known wall-clock time, for boots without internet
void setLastEpoch(uint32_t e);
void factoryReset();
}  // namespace storage
