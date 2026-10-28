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
