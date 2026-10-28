#pragma once
#include <Arduino.h>

// Signed over-the-air updates from GitHub releases.
// manifest.json: {"version":N,"name":"x.y.z","size":S,"sha256":"hex","sig":"base64 DER"}
// sig = ECDSA P-256 over SHA-256("okame:<version>:<size>:<sha256>"), checked with ota_pubkey.h.
namespace ota {
void begin();
// Call often. Starts a background check when due and the bird is idle.
void loop(bool online, bool idle, uint32_t epoch);
void checkNow();
int progress();          // -1 when not installing, else 0..100
const char* status();    // human-readable last result
uint32_t lastCheck();
// After a new image boots: mark it good once it has run for a while.
void healthy();
}  // namespace ota
