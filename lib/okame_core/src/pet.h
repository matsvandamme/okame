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
  // Added later (older saves load with these zeroed).
  uint8_t hat;            // 0 = none, else an Accessory
  uint16_t unlocked;      // bit per Accessory
  uint32_t lastGift;      // epoch of the last daily gift
  uint16_t gamesWon, careCount, bestRain;
};

// Index = Accessory. Omamori and hachimaki are earned only (omikuji collection, focus sessions), never in a gift.
enum Accessory : uint8_t {
  HAT_NONE, HAT_BOW, HAT_CROWN, HAT_BERET, HAT_PARTY, HAT_SAKURA, HAT_OMAMORI, HAT_HACHIMAKI, HAT_COUNT
};
constexpr uint8_t HAT_GIFTABLE = HAT_SAKURA + 1;

// A calendar date. Days are counted with civil(), so countdowns and "once a day" follow the local calendar.
struct Date {
  uint16_t y;
  uint8_t m, d;
};
int32_t civil(Date d);  // days since 1970-01-01

enum class Occasion : uint8_t { None, Hina, Sakura, Kodomo, Tanabata, Halloween, Sinterklaas, Christmas, NewYear,
                                Birthday, Anniversary, HomeDay, YearEnd };

struct PetExtra;
// What today is: a public holiday or season, or one of the private dates the relay sent (birthday first).
Occasion occasion(const Date& today, const PetExtra& x);

// Everything added in 0.4. Saved under its own key so a rollback to an older firmware still finds PetState
// exactly as it left it. Zeroed when missing.
struct PetExtra {
  static constexpr int WORDS = 8, WORD_LEN = 24, DATES = 4;
  uint16_t version;
  Date home;                       // Matthieu's return day; y == 0 when unset
  int32_t omikujiDay;              // civil day of the last draw
  uint8_t omikujiGrade, omikujiSeen;
  uint8_t bestDance;
  uint16_t jankenWins, focusCount, rivalRain;
  struct Special { uint8_t m, d, kind; } dates[DATES];  // kind: 1 birthday, 2 anniversary
  int32_t greetedDay;              // civil day the occasion/countdown greeting last played
  struct Word { char text[WORD_LEN + 1]; uint32_t at; } words[WORDS];
  char who[13];                    // whose birthday it is, for the banner
  int16_t lastCardDays;            // days shown on the last countdown card, so only a number she saw gets crossed off
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

  // Dress-up. Accessories unlock by caring and playing; a daily gift can unlock one too.
  bool wear(uint8_t hat);
  bool giftReady(uint32_t now) const;
  int openGift(uint32_t now, uint32_t rnd);  // returns the unlocked Accessory, or -1 for a treat
  int newUnlock();                            // an accessory unlocked since the last call, or -1
  void recordGame(bool won, int rainScore);

  bool isNight(uint32_t now) const;
  bool asleep(uint32_t now) const { return isNight(now) && s_.stage != Stage::Egg; }
  bool needsAttention(uint32_t now) const;
  Mood mood(uint32_t now) const;
  uint32_t ageSeconds(uint32_t now) const { return s_.born && now > s_.born ? now - s_.born : 0; }

  const PetState& state() const { return s_; }
  bool load(const PetState& st);  // false if the version doesn't match

  // 0.4 additions (see PetExtra).
  static constexpr uint16_t EXTRA_VERSION = 1;
  const PetExtra& extra() const { return x_; }
  PetExtra& extraMut() { return x_; }
  void loadExtra(const PetExtra& x);
  bool learnWord(const char* text, uint32_t now);  // false when it already knows it
  bool forgetWord(const char* text);
  int wordCount() const;
  bool canDrawOmikuji(int32_t today) const { return x_.omikujiDay != today && s_.stage != Stage::Egg && s_.stage != Stage::Gone; }
  int drawOmikuji(int32_t today, uint32_t rnd);  // grade 0..5 (大吉..凶), or -1 if already drawn today
  void recordDance(int score);
  void recordJanken(bool won);
  void recordFocus();

 private:
  void step(uint32_t now, uint32_t dt);
  void grow(uint32_t now);
  uint16_t baseWeight() const;
  void unlock(uint8_t a);
  int pendingUnlock_ = -1;

  PetState s_;
  PetExtra x_{};
  HourFn hour_;
};
