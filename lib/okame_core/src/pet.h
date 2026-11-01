#pragma once
#include <stdint.h>

// Okame's care rules. Pure logic: time comes in as epoch seconds, the local hour via a callback.

enum class Stage : uint8_t { Egg, Chick, Juvenile, Adult, Gone };
enum class Variant : uint8_t { Grey, Pearl, Pied, Scruffy };
// Which bird hatches from the egg. Chosen only when a new egg starts, so a bird never changes species.
enum class Species : uint8_t { Cockatiel, Budgie, Owl, Count };
// Friends who drop in on Okame now and then. The pigeon brings Matthieu's notes and hugs.
enum Friend : uint8_t { FRIEND_BUDGIE, FRIEND_PIGEON, FRIEND_ENAGA, FRIEND_MEJIRO, FRIEND_OWL, FRIEND_COUNT };
enum class Action : uint8_t { Seed, Millet, Bath, Vet, Light, Scritch, GameWin, GameLose };
enum class Result : uint8_t { Ok, Refused, NotNeeded, Sleeping, Away };
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

// Everything added in 0.5: the species and the friends who visited. Its own key again, so 0.4 still finds
// "pet" and "pet2" exactly as it left them after a rollback. Zeroed when missing (a cockatiel with no friends yet).
struct PetBirds {
  uint16_t version;
  uint8_t species;                // Species
  uint8_t met;                    // bit per Friend
  uint16_t visits[FRIEND_COUNT];
  int32_t visitDay;               // civil day that visitsToday counts
  uint8_t visitsToday;
  uint32_t eggStart;              // the egg this species belongs to; another egg (say, after a rollback) is a cockatiel
};

// Tricks a trained bird can do, one per training level.
enum Trick : uint8_t { TRICK_WAVE, TRICK_SPIN, TRICK_BOW, TRICK_HANG, TRICK_COUNT };
// Age milestones (days) that get a party.
constexpr uint16_t PARTY_DAYS[] = {7, 10, 30, 50, 100, 200, 365};

// Everything added in 0.6: training, old age, the golden form, the clock and the alarm. Its own key ("pet4"), so
// 0.5 still finds "pet", "pet2" and "pet3" exactly as it left them after a rollback. Later releases append fields
// and keep LIFE_VERSION: a shorter save loads with the new fields zeroed.
struct PetLife {
  uint16_t version;
  uint8_t training;               // 0..4, one trick per level
  uint8_t tricks;                 // bit per Trick
  uint32_t fussAt;                // epoch a fuss started (0 = none)
  uint32_t nextFuss;              // the earliest the next fuss may come
  int32_t fussDay;                // civil day that fussToday counts
  uint8_t fussToday;
  uint8_t senior, golden;
  uint8_t baselined;              // adultMistakes holds the mistakes count at adulthood
  uint16_t adultMistakes;
  uint16_t partyDays;             // the last milestone celebrated
  uint8_t lrBest;
  uint8_t clockOnCover;           // show the time on the covered cage
  struct Alarm { uint8_t h, m, days, on; } alarm;  // days: bit 0 = Monday
  uint8_t seniorShown, goldenShown;  // the reveal played (it waits for an awake bird on its main screen)
  int32_t alarmDay;               // civil day the alarm last greeted, so a reboot in that minute doesn't repeat it
  uint32_t eggStart;              // the egg this belongs to; another egg (say, after a rollback) starts fresh
  // 0.7
  struct Outing { uint8_t place, done; uint32_t start, back; } outing;  // place: Place + 1 (0 = home)
  uint32_t toyAt;                 // last time it played with its toy (a little joy, once an hour)
  uint32_t familyAt;              // last visit from the family
};

// 0.7: Okame's world. Things that belong to the home rather than to one bird, so they stay for the next egg:
// seeds (points) and what they bought, souvenirs, and the family album. Own key ("pet5").
enum Toy : uint8_t { TOY_SWING, TOY_BELL, TOY_MIRROR, TOY_LADDER, TOY_PERCH, TOY_COUNT };
enum Place : uint8_t { PLACE_PARK, PLACE_CAFE, PLACE_BEACH, PLACE_SHRINE, PLACE_WINDMILL, PLACE_FRITES, PLACE_COUNT };
constexpr uint16_t TOY_PRICE[TOY_COUNT] = {30, 20, 25, 30, 20};
constexpr uint16_t HAT_PRICE = 40, TREAT_PRICE = 5;
constexpr uint8_t PLACE_MIN_DAYS[PLACE_COUNT] = {0, 0, 7, 7, 14, 14};
constexpr int FAMILY_MAX = 6;
struct PetHome {
  uint16_t version;
  uint32_t points;                // seeds 🌻
  int32_t careDay;                // day that carePoints counts (days since 1970, UTC)
  uint8_t carePoints;             // seeds earned by care today (capped)
  uint8_t toys;                   // bit per Toy owned
  uint8_t toy;                    // the toy in the cage: Toy + 1, 0 = none
  uint8_t souvenirs;              // bit per Place visited
  uint8_t generation;             // 0 for the first bird
  uint32_t partnerAt;             // a partner came (the egg comes the next morning); 0 = none
  int32_t partnerDay;             // the local day it came
  uint8_t inherit;                // the parent's look for the next adult: Variant + 1, +8 if it was golden
  uint8_t familyCount;
  struct Member { char name[17]; uint8_t species, variant, golden, generation; uint16_t days; } family[FAMILY_MAX];
  uint16_t hats;                  // accessories bought in the shop: every bird of the family keeps them
  int32_t playDay;                // day that playPoints counts
  uint8_t playPoints;             // seeds won at games today (capped)
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

  // 0.5 additions (see PetBirds). A new egg keeps the friends; the species is set by whoever starts the egg.
  static constexpr uint16_t BIRDS_VERSION = 2;
  const PetBirds& birds() const { return b_; }
  PetBirds& birdsMut() { return b_; }
  void loadBirds(const PetBirds& b);
  Species species() const { return b_.species < uint8_t(Species::Count) ? Species(b_.species) : Species::Cockatiel; }
  void setSpecies(Species sp) { b_.species = uint8_t(sp) < uint8_t(Species::Count) ? uint8_t(sp) : 0; }
  bool recordVisit(Friend f);  // true the first time this friend comes

  // 0.6 additions (see PetLife).
  static constexpr uint16_t LIFE_VERSION = 1;
  static constexpr uint32_t FUSS_LASTS = 20 * rules::MIN, SENIOR_AT = 30 * rules::DAY;
  const PetLife& life() const { return l_; }
  PetLife& lifeMut() { return l_; }
  void loadLife(const PetLife& l);
  // Now and then a well-cared-for bird fusses for attention (わがまま). Starts one if it's time; true when it did.
  bool maybeFuss(uint32_t now, int32_t today, uint32_t rnd);
  // A fuss shows only while every need is still met; a hungry or sad bird has a real reason.
  bool fussing(uint32_t now) const {
    return l_.fussAt && now - l_.fussAt < FUSS_LASTS && !asleep(now) && !s_.lightsOff && !s_.sick && s_.hunger >= 2 &&
           s_.happy >= 2 && s_.poop < 2;
  }
  // A calm "no": training goes up. Returns the trick just learned, or -1 (no fuss, or nothing new).
  int calmFuss(uint32_t now);
  // Giving in: millet makes it happy, but it learns nothing.
  Result spoilFuss(uint32_t now);
  int trickCount() const;
  int pickTrick(uint32_t rnd) const;  // a known trick, or -1
  // The highest age milestone reached but not celebrated yet (days), or 0. markParty() once it has played.
  uint16_t partyDue(uint32_t now) const;
  void markParty(uint16_t days) {
    if (days <= l_.partyDays) return;
    l_.partyDays = days;
    earn(20);
  }
  bool senior() const { return l_.senior; }
  bool golden() const { return l_.golden; }
  void recordLeftRight(int hits);

  // 0.7 additions (see PetHome).
  static constexpr uint16_t HOME_VERSION = 1;
  static constexpr uint8_t CARE_POINTS_PER_DAY = 10, PLAY_POINTS_PER_DAY = 30;
  void setToday(int32_t day) { today_ = day; }  // the local calendar day, from the platform (0 = unknown)
  static constexpr uint32_t TOY_COOLDOWN = rules::HOUR;
  const PetHome& home() const { return h_; }
  PetHome& homeMut() { return h_; }
  void loadHome(const PetHome& h);
  void earn(uint32_t n) { h_.points = h_.points + n > 99999 ? 99999 : h_.points + n; }
  // The bird shop. 0 bought, -1 not enough seeds, -2 already have it, -3 not for sale (or not now).
  int buyTreat(uint32_t now);
  int buyToy(uint8_t toy);
  int buyHat(uint8_t hat);
  bool placeToy(uint8_t toy);  // Toy + 1, 0 takes it out
  bool playToy(uint32_t now);  // true when it cheered the bird up (once an hour)
  // Outings (おでかけ). Only a content, awake grown-up goes out, in the daytime; it's back in 60-90 minutes with a
  // souvenir. While it's away nothing needs doing, and time stands still for its hearts.
  bool canGoOut(uint8_t place, uint32_t now) const;
  bool goOut(uint8_t place, uint32_t now, uint32_t rnd);
  bool away(uint32_t now) const { return l_.outing.place && !l_.outing.done && now < l_.outing.back; }
  int homeFrom() const { return l_.outing.place && l_.outing.done ? l_.outing.place - 1 : -1; }  // back, not greeted
  void greetedHome() { l_.outing = {}; }
  // Generations. Opt-in only: a grown bird meets a partner; the next morning it leaves an egg and moves into the
  // family album. The new egg is the same kind, and may take after its parent.
  bool canFindPartner(uint32_t now) const;
  bool findPartner(uint32_t now, int32_t today);
  bool eggDue(int32_t today) const { return h_.partnerAt && s_.stage == Stage::Adult && today > h_.partnerDay; }
  void nextGeneration(uint32_t now);

 private:
  void step(uint32_t now, uint32_t dt);
  Result doAct(Action a, uint32_t now);
  void grow(uint32_t now);
  uint16_t baseWeight() const;
  void unlock(uint8_t a);
  int pendingUnlock_ = -1;

  PetState s_;
  PetExtra x_{};
  PetBirds b_{BIRDS_VERSION, 0, 0, {}, 0, 0, 0};
  PetLife l_{LIFE_VERSION};
  PetHome h_{HOME_VERSION};
  int32_t today_ = 0;
  int32_t dayOf(uint32_t now) const { return today_ ? today_ : (int32_t)(now / rules::DAY); }
  HourFn hour_;
};
