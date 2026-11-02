#include "storage.h"
#include <Preferences.h>
#include <esp_random.h>

static Preferences prefs;

namespace storage {

void begin() { prefs.begin("okame", false); }

// Saved records only ever grow at the end. A shorter one (from an older firmware) loads with the new fields zeroed;
// a longer one (after a rollback from a newer firmware) loads what this firmware knows. Nothing is ever refused.
static uint8_t blob[512];
static size_t readBlob(const char* key, void* out, size_t size) {
  size_t len = prefs.getBytesLength(key);
  if (len < 2 || len > sizeof blob) return 0;
  memset(blob, 0, sizeof blob);
  prefs.getBytes(key, blob, len);
  memcpy(out, blob, size);
  return len;
}

bool hasPet() { return prefs.getBytesLength("pet") >= 2; }

bool loadPet(PetState& out) {
  memset(&out, 0, sizeof out);
  return readBlob("pet", &out, sizeof out) > 0;
}

void savePet(const PetState& st) { prefs.putBytes("pet", &st, sizeof st); }

void loadExtra(Pet& pet) {
  static PetExtra x;  // ~300 bytes: keep it off the loop task's stack
  if (readBlob("pet2", &x, sizeof x)) pet.loadExtra(x);
}

void saveExtra(const PetExtra& x) { prefs.putBytes("pet2", &x, sizeof x); }

void loadBirds(Pet& pet) {
  PetBirds b;
  if (readBlob("pet3", &b, sizeof b)) pet.loadBirds(b);
}

void saveBirds(const PetBirds& b) { prefs.putBytes("pet3", &b, sizeof b); }

void loadLife(Pet& pet) {
  PetLife l;
  if (readBlob("pet4", &l, sizeof l)) pet.loadLife(l);
}

void saveLife(const PetLife& l) { prefs.putBytes("pet4", &l, sizeof l); }

void loadHome(Pet& pet) {
  static PetHome h;
  if (readBlob("pet5", &h, sizeof h)) pet.loadHome(h);
}

void saveHome(const PetHome& h) { prefs.putBytes("pet5", &h, sizeof h); }

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
  if (p.length() != 10) {  // ten digits: easy on a phone keypad, far harder to guess than eight
    char buf[11];
    for (int i = 0; i < 10; i++) buf[i] = '0' + esp_random() % 10;
    buf[10] = 0;
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
