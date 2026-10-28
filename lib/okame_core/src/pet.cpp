#include "pet.h"
#include <string.h>

using namespace rules;

static uint8_t dec(uint8_t v) { return v ? v - 1 : 0; }
static uint8_t inc(uint8_t v, uint8_t max) { return v < max ? v + 1 : max; }

void Pet::reset(uint32_t now) {
  memset(&s_, 0, sizeof s_);
  s_.version = VERSION;
  s_.stage = Stage::Egg;
  s_.hunger = s_.happy = 3;
  s_.eggStart = s_.lastTick = now;
}

bool Pet::load(const PetState& st) {
  if (st.version != VERSION) return false;
  s_ = st;
  s_.name[sizeof s_.name - 1] = 0;
  return true;
}

uint16_t Pet::baseWeight() const {
  switch (s_.stage) {
    case Stage::Chick: return 30;
    case Stage::Juvenile: return 60;
    case Stage::Adult: return 90;
    default: return 0;
  }
}

bool Pet::isNight(uint32_t now) const {
  int h = hour_(now);
  return h >= NIGHT_FROM || h < NIGHT_TO;
}

void Pet::tick(uint32_t now) {
  if (s_.stage == Stage::Gone) {
    s_.lastTick = now;
    return;
  }
  if (s_.eggStart == 0) s_.eggStart = now;
  if (s_.lastTick == 0 || now < s_.lastTick) s_.lastTick = now;
  if (now - s_.lastTick > CATCH_UP_CAP) s_.lastTick = now - CATCH_UP_CAP;
  while (now - s_.lastTick >= MIN) {
    s_.lastTick += MIN;
    step(s_.lastTick, MIN);
    if (s_.stage == Stage::Gone) break;
  }
}

void Pet::grow(uint32_t now) {
  if (s_.stage == Stage::Egg && now - s_.eggStart >= HATCH_AFTER) {
    s_.stage = Stage::Chick;
    s_.born = now;
    s_.weight = baseWeight();
    s_.hunger = 2;
    s_.happy = 3;
  } else if (s_.stage == Stage::Chick && ageSeconds(now) >= JUVENILE_AT) {
    s_.stage = Stage::Juvenile;
    s_.weight += 30;
  } else if (s_.stage == Stage::Juvenile && ageSeconds(now) >= ADULT_AT) {
    s_.stage = Stage::Adult;
    s_.weight += 30;
    if (s_.mistakes <= 2)
      s_.variant = s_.affection >= 300 ? Variant::Pied : Variant::Pearl;
    else if (s_.mistakes <= 6)
      s_.variant = Variant::Grey;
    else
      s_.variant = Variant::Scruffy;
  }
}

void Pet::step(uint32_t now, uint32_t dt) {
  grow(now);
  if (s_.stage == Stage::Egg || s_.stage == Stage::Gone) return;

  bool night = isNight(now);
  uint32_t hungerEvery = night ? HUNGER_EVERY * 2 : HUNGER_EVERY;
  uint32_t happyEvery = night ? HAPPY_EVERY * 2 : HAPPY_EVERY;
  if (s_.sick) happyEvery /= 2;

  if ((s_.hungerT += dt) >= hungerEvery) {
    s_.hungerT = 0;
    s_.hunger = dec(s_.hunger);
  }
  if ((s_.happyT += dt) >= happyEvery) {
    s_.happyT = 0;
    s_.happy = dec(s_.happy);
  }
  if (!night && (s_.poopT += dt) >= POOP_EVERY) {
    s_.poopT = 0;
    s_.poop = inc(s_.poop, MAX_POOP);
  }

  // Sickness from a dirty cage or an empty belly.
  s_.dirtyT = s_.poop >= MAX_POOP ? s_.dirtyT + dt : 0;
  s_.starveT = s_.hunger == 0 ? s_.starveT + dt : 0;
  if (!s_.sick && (s_.dirtyT >= DIRTY_SICK_AFTER || s_.starveT >= STARVE_SICK_AFTER)) {
    s_.sick = true;
    s_.mistakes++;
  }

  // Neglect: both hearts empty. Long neglect costs care points; very long, the bird flies away.
  if (s_.hunger == 0 && s_.happy == 0) {
    s_.neglectT += dt;
    if ((s_.neglectMistakeT += dt) >= NEGLECT_MISTAKE_EVERY) {
      s_.neglectMistakeT = 0;
      s_.mistakes++;
    }
    if (s_.neglectT >= FLY_AWAY_AFTER) {
      s_.stage = Stage::Gone;
      return;
    }
  } else {
    s_.neglectT = s_.neglectMistakeT = 0;
  }

  // Lights left on at night.
  if (night && !s_.lightsOff) {
    if ((s_.lightsT += dt) >= LIGHTS_MISTAKE_AFTER && !s_.lightsMistakeTonight) {
      s_.lightsMistakeTonight = true;
      s_.mistakes++;
    }
  } else if (!night) {
    s_.lightsT = 0;
    s_.lightsMistakeTonight = false;
    s_.lightsOff = false;  // morning: the cover comes off
  }
}

Result Pet::act(Action a, uint32_t now) {
  if (s_.stage == Stage::Egg || s_.stage == Stage::Gone) return Result::Refused;
  if (s_.lightsOff && a != Action::Light) return Result::Sleeping;
  uint16_t base = baseWeight();
  switch (a) {
    case Action::Seed:
      if (s_.hunger >= MAX_STAT) return Result::Refused;
      s_.hunger++;
      s_.hungerT = 0;
      s_.weight++;
      return Result::Ok;
    case Action::Millet:
      s_.happy = inc(s_.happy, MAX_STAT);
      s_.weight += 3;
      return Result::Ok;
    case Action::Bath:
      if (!s_.poop) return Result::NotNeeded;
      s_.poop = 0;
      s_.poopT = s_.dirtyT = 0;
      return Result::Ok;
    case Action::Vet:
      if (!s_.sick) return Result::NotNeeded;
      s_.sick = false;
      s_.dirtyT = s_.starveT = 0;
      return Result::Ok;
    case Action::Light:
      s_.lightsOff = !s_.lightsOff;
      return Result::Ok;
    case Action::Scritch:
      if (now - s_.lastScritch >= SCRITCH_COOLDOWN || s_.lastScritch == 0) {
        s_.happy = inc(s_.happy, MAX_STAT);
        s_.lastScritch = now;
      }
      if (s_.affection < 1000) s_.affection += 10;
      return Result::Ok;
    case Action::GameWin:
      s_.happy = inc(s_.happy, MAX_STAT);
      if (s_.weight > base + 1) s_.weight -= 2;
      if (s_.affection < 1000) s_.affection += 5;
      return Result::Ok;
    case Action::GameLose:
      if (s_.weight > base) s_.weight -= 1;
      return Result::Ok;
  }
  return Result::Refused;
}

void Pet::setName(const char* name) {
  strncpy(s_.name, name, sizeof s_.name - 1);
  s_.name[sizeof s_.name - 1] = 0;
}

void Pet::setTune(const uint8_t* notes, uint8_t len) {
  if (len > TUNE_MAX) len = TUNE_MAX;
  memcpy(s_.tune, notes, len);
  s_.tuneLen = len;
  s_.tuneLevel = 0;
  s_.tuneLastPractice = 0;
}

bool Pet::practiceTune(uint32_t now) {
  if (!s_.tuneLen || s_.tuneLevel >= TUNE_LEARNED) return false;
  if (s_.tuneLastPractice && now - s_.tuneLastPractice < PRACTICE_GAP) return false;
  s_.tuneLevel++;
  s_.tuneLastPractice = now;
  if (s_.affection < 1000) s_.affection += 5;
  return true;
}

bool Pet::needsAttention(uint32_t now) const {
  if (s_.stage == Stage::Egg || s_.stage == Stage::Gone) return false;
  return s_.hunger == 0 || s_.happy == 0 || s_.poop >= 2 || s_.sick ||
         (isNight(now) && !s_.lightsOff);
}

Mood Pet::mood(uint32_t now) const {
  if (s_.sick) return Mood::Sick;
  if (s_.lightsOff || asleep(now)) return Mood::Asleep;
  if (s_.hunger == 0 || s_.happy == 0) return Mood::Sad;
  if (s_.hunger >= 2 && s_.happy >= 2) return Mood::Content;
  return Mood::Meh;
}
