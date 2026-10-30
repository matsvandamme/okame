#include "storage.h"
#include <Preferences.h>
#include <esp_random.h>

static Preferences prefs;

namespace storage {

void begin() { prefs.begin("okame", false); }

bool loadPet(PetState& out) {
  // Accept shorter blobs so an update that appends fields keeps the bird.
  size_t len = prefs.getBytesLength("pet");
  if (len < 2 || len > sizeof out) return false;
  memset(&out, 0, sizeof out);
  prefs.getBytes("pet", &out, len);
  return out.version == Pet::VERSION;
}

void savePet(const PetState& st) { prefs.putBytes("pet", &st, sizeof st); }

void loadExtra(Pet& pet) {
  size_t len = prefs.getBytesLength("pet2");
  if (len < 2 || len > sizeof(PetExtra)) return;
  static PetExtra x;  // ~300 bytes: keep it off the loop task's stack
  memset(&x, 0, sizeof x);
  prefs.getBytes("pet2", &x, len);
  pet.loadExtra(x);
}

void saveExtra(const PetExtra& x) { prefs.putBytes("pet2", &x, sizeof x); }

void loadBirds(Pet& pet) {
  size_t len = prefs.getBytesLength("pet3");
  if (len < 2 || len > sizeof(PetBirds)) return;  // a shorter save from an older layout loads with the rest zeroed
  PetBirds b;
  memset(&b, 0, sizeof b);
  prefs.getBytes("pet3", &b, len);
  pet.loadBirds(b);
}

void saveBirds(const PetBirds& b) { prefs.putBytes("pet3", &b, sizeof b); }

int loadNetworks(SavedNetwork* out, int max) {
  size_t len = prefs.getBytesLength("nets");
  int n = len / sizeof(SavedNetwork);
  if (n > max) n = max;
  if (n) prefs.getBytes("nets", out, n * sizeof(SavedNetwork));
  return n;
}

void saveNetworks(const SavedNetwork* nets, int n) {
  if (n == 0) prefs.remove("nets");
  else prefs.putBytes("nets", nets, n * sizeof(SavedNetwork));
}

String apPassword() {
  String p = prefs.getString("appw", "");
  if (p.length() != 8) {
    char buf[9];
    for (int i = 0; i < 8; i++) buf[i] = '0' + esp_random() % 10;
    buf[8] = 0;
    p = buf;
    prefs.putString("appw", p);
  }
  return p;
}

uint32_t lastEpoch() { return prefs.getULong("epoch", 0); }
void setLastEpoch(uint32_t e) { prefs.putULong("epoch", e); }

void factoryReset() {
  String appw = apPassword();
  prefs.clear();
  prefs.putString("appw", appw);
}

}  // namespace storage
