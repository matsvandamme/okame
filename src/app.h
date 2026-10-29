#pragma once
#include <Arduino.h>
#include <freertos/semphr.h>
#include "pet.h"
#include "ui.h"

// Shared state between the main loop and the web server task. Hold app::Lock while touching it.
namespace app {
extern Pet pet;
extern Ui ui;
extern SemaphoreHandle_t mutex;
struct Lock {
  Lock() { xSemaphoreTakeRecursive(mutex, portMAX_DELAY); }
  ~Lock() { xSemaphoreGiveRecursive(mutex); }
};
uint32_t epoch();
Date today();  // local date, y == 0 until the clock is known
void requestSave();
void frameHex(char out[201]);
bool pairActive();
bool requestPair();  // show a fresh code on the LCD; false if asked too recently
// Returns a session token on success, "" on failure; triesLeft reports what remains.
String tryPair(const String& code, int& triesLeft);
bool sessionValid(const String& token);
void playTune();        // the bird whistles its learned tune
bool takeTuneEvent();   // true once after playTune(), for the phone page
}  // namespace app
