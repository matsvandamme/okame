#pragma once
#include <stdint.h>

// Okame's care rules. Pure logic: time comes in as epoch seconds, the local hour via a callback.

enum class Stage : uint8_t { Egg, Chick, Juvenile, Adult, Gone };
enum class Variant : uint8_t { Grey, Pearl, Pied, Scruffy };
enum class Action : uint8_t { Seed, Millet, Bath, Vet, Light, Scritch, GameWin, GameLose };
enum class Result : uint8_t { Ok, Refused, NotNeeded, Sleeping };
enum class Mood : uint8_t { Content, Meh, Sad, Sick, Asleep };

namespace rules {
constexpr uint32_t MIN = 60, HOUR = 3600, DAY = 86400;
constexpr uint32_t HATCH_AFTER = 2 * MIN;
constexpr uint32_t JUVENILE_AT = 1 * DAY;
constexpr uint32_t ADULT_AT = 3 * DAY;
constexpr uint32_t HUNGER_EVERY = 100 * MIN;   // awake; doubled while asleep
constexpr uint32_t HAPPY_EVERY = 120 * MIN;    // awake; doubled asleep, halved sick
constexpr uint32_t POOP_EVERY = 180 * MIN;     // awake only
constexpr uint32_t DIRTY_SICK_AFTER = 2 * HOUR;
constexpr uint32_t STARVE_SICK_AFTER = 6 * HOUR;
constexpr uint32_t NEGLECT_MISTAKE_EVERY = 6 * HOUR;
constexpr uint32_t FLY_AWAY_AFTER = 72 * HOUR;
constexpr uint32_t LIGHTS_MISTAKE_AFTER = 1 * HOUR;
constexpr uint32_t CATCH_UP_CAP = 12 * HOUR;
constexpr uint32_t SCRITCH_COOLDOWN = 10 * MIN;
constexpr uint32_t PRACTICE_GAP = 20 * HOUR;
constexpr uint8_t NIGHT_FROM = 22, NIGHT_TO = 8;
constexpr uint8_t MAX_STAT = 4, MAX_POOP = 3, TUNE_MAX = 8, TUNE_LEARNED = 3;
}  // namespace rules

struct PetState {
  uint16_t version;
  Stage stage;
  Variant variant;
  uint8_t hunger, happy, poop;
  bool sick, lightsOff;
  uint16_t weight, affection, mistakes;
  uint32_t eggStart, born, lastTick;
  uint32_t hungerT, happyT, poopT, dirtyT, starveT, neglectT, neglectMistakeT, lightsT;
  bool lightsMistakeTonight;
  uint32_t lastScritch;
  char name[17];
  uint8_t tune[rules::TUNE_MAX], tuneLen, tuneLevel;
  uint32_t tuneLastPractice;
};

class Pet {
 public:
  static constexpr uint16_t VERSION = 1;
  using HourFn = int (*)(uint32_t epoch);

  explicit Pet(HourFn hour) : hour_(hour) { reset(0); }

  void reset(uint32_t now);
  // Advance to `now`, catching up at most rules::CATCH_UP_CAP of missed time.
  void tick(uint32_t now);
  Result act(Action a, uint32_t now);

  void setName(const char* name);
  void setTune(const uint8_t* notes, uint8_t len);
  bool practiceTune(uint32_t now);  // true when the bird learned a little more

  bool isNight(uint32_t now) const;
  bool asleep(uint32_t now) const { return isNight(now) && s_.stage != Stage::Egg; }
  bool needsAttention(uint32_t now) const;
  Mood mood(uint32_t now) const;
  uint32_t ageSeconds(uint32_t now) const { return s_.born && now > s_.born ? now - s_.born : 0; }

  const PetState& state() const { return s_; }
  bool load(const PetState& st);  // false if the version doesn't match

 private:
  void step(uint32_t now, uint32_t dt);
  void grow(uint32_t now);
  uint16_t baseWeight() const;

  PetState s_;
  HourFn hour_;
};
