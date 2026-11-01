#include "pet.h"
#include <string.h>

using namespace rules;

static uint8_t dec(uint8_t v) { return v ? v - 1 : 0; }
static uint8_t inc(uint8_t v, uint8_t max) { return v < max ? v + 1 : max; }

void Pet::reset(uint32_t now) {
  memset(&s_, 0, sizeof s_);
  // A new egg starts its own scores and words; the dates the relay set (birthday, return day) stay.
  PetExtra keep = x_;
  memset(&x_, 0, sizeof x_);
  x_.version = EXTRA_VERSION;
  x_.home = keep.home;
  memcpy(x_.dates, keep.dates, sizeof x_.dates);
  memcpy(x_.who, keep.who, sizeof x_.who);
  s_.version = VERSION;
  s_.stage = Stage::Egg;
  s_.hunger = s_.happy = 3;
  s_.eggStart = s_.lastTick = now;
  b_.eggStart = now;  // the species carries over to this egg
  // Training, old age and parties start over; the clock and alarm are settings and stay.
  PetLife keepLife = l_;
  memset(&l_, 0, sizeof l_);
  l_.version = LIFE_VERSION;
  l_.alarm = keepLife.alarm;
  l_.clockOnCover = keepLife.clockOnCover;
  l_.alarmDay = keepLife.alarmDay;
  l_.eggStart = now;
  // A partner who came for the bird before this egg doesn't count for the new one. Bought accessories stay.
  h_.partnerAt = 0;
  h_.partnerDay = 0;
  s_.unlocked = h_.hats;
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
    l_.adultMistakes = s_.mistakes;  // the golden form looks at care from here on
    l_.baselined = 1;
    if (s_.mistakes <= 2)
      s_.variant = s_.affection >= 300 ? Variant::Pied : Variant::Pearl;
    else if (s_.mistakes <= 6)
      s_.variant = Variant::Grey;
    else
      s_.variant = Variant::Scruffy;
    // Half the time a well-kept bird takes after its parent's pearl or pied feathers.
    Variant pv = Variant((h_.inherit & 7) - 1);
    if ((h_.inherit & 7) && s_.mistakes <= 6 && (pv == Variant::Pearl || pv == Variant::Pied) && (s_.eggStart / 60) % 2)
      s_.variant = pv;
  } else if (s_.stage == Stage::Adult && !l_.senior && ageSeconds(now) >= SENIOR_AT) {
    // A cosy old age. A bird that was cared for almost perfectly as an adult turns golden (lutino): the secret form.
    l_.senior = 1;
    // A golden parent makes it easier.
    bool goldParent = h_.inherit & 8;
    if (s_.mistakes - l_.adultMistakes <= (goldParent ? 3 : 1) && s_.affection >= (goldParent ? 150 : 200)) l_.golden = 1;
  }
}

void Pet::step(uint32_t now, uint32_t dt) {
  // A bird that was already grown up when 0.6 arrived: its care counts from today.
  if (!l_.eggStart) l_.eggStart = s_.eggStart;
  if (s_.stage == Stage::Adult && !l_.baselined) {
    l_.adultMistakes = s_.mistakes;
    l_.baselined = 1;
  }
  grow(now);
  if (s_.stage == Stage::Egg || s_.stage == Stage::Gone) return;
  if (l_.outing.place && !l_.outing.done) {
    if (now < l_.outing.back) return;  // out and about: nothing changes at home
    l_.outing.done = 1;                // back, with a souvenir and a little extra joy
    l_.outing.back = now;
    h_.souvenirs |= 1 << (l_.outing.place - 1);
    earn(10);
    s_.happy = inc(s_.happy, MAX_STAT);
    s_.hungerT = s_.happyT = 0;
  }

  bool night = isNight(now);
  uint32_t hungerEvery = night ? HUNGER_EVERY * 2 : HUNGER_EVERY;
  if (l_.senior) hungerEvery += hungerEvery / 4;  // a senior eats a little less often
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
  if (away(now)) return Result::Away;
  Result r = doAct(a, now);
  if (r == Result::Ok && a != Action::Light && a != Action::GameWin && a != Action::GameLose) {
    int32_t day = dayOf(now);  // a seed for each bit of care, ten a day
    if (h_.careDay != day) {
      h_.careDay = day;
      h_.carePoints = 0;
    }
    if (h_.carePoints < CARE_POINTS_PER_DAY) {
      h_.carePoints++;
      earn(1);
    }
  }
  return r;
}

Result Pet::doAct(Action a, uint32_t now) {
  if (s_.stage == Stage::Egg || s_.stage == Stage::Gone) return Result::Refused;
  if (a != Action::GameWin && a != Action::GameLose && ++s_.careCount >= 20) unlock(HAT_BERET);
  if (a == Action::Scritch) unlock(HAT_BOW);
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
  if (s_.tuneLevel >= TUNE_LEARNED) unlock(HAT_SAKURA);
  if (s_.affection < 1000) s_.affection += 5;
  return true;
}

bool Pet::needsAttention(uint32_t now) const {
  if (s_.stage == Stage::Egg || s_.stage == Stage::Gone || away(now)) return false;
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

void Pet::unlock(uint8_t a) {
  if (a == HAT_NONE || a >= HAT_COUNT || (s_.unlocked & (1u << a))) return;
  s_.unlocked |= 1u << a;
  pendingUnlock_ = a;
}

int Pet::newUnlock() {
  int u = pendingUnlock_;
  pendingUnlock_ = -1;
  return u;
}

bool Pet::wear(uint8_t hat) {
  if (hat != HAT_NONE && (hat >= HAT_COUNT || !(s_.unlocked & (1u << hat)))) return false;
  s_.hat = hat;
  return true;
}

bool Pet::giftReady(uint32_t now) const {
  if (s_.stage == Stage::Egg || s_.stage == Stage::Gone) return false;
  return s_.lastGift == 0 || now / DAY != s_.lastGift / DAY;
}

int Pet::openGift(uint32_t now, uint32_t rnd) {
  if (!giftReady(now)) return -2;
  s_.lastGift = now;
  // Half the time a still-locked accessory, otherwise a treat.
  uint8_t locked[HAT_COUNT];
  int n = 0;
  for (uint8_t a = 1; a < HAT_GIFTABLE; a++)
    if (!(s_.unlocked & (1u << a))) locked[n++] = a;
  if (n && rnd % 2 == 0) {
    unlock(locked[(rnd / 2) % n]);
    return pendingUnlock_ >= 0 ? pendingUnlock_ : -1;
  }
  s_.happy = inc(s_.happy, MAX_STAT);
  s_.hunger = inc(s_.hunger, MAX_STAT);
  return -1;
}

void Pet::recordGame(bool won, int rainScore) {
  int32_t day = dayOf(s_.lastTick);  // seeds from games: up to 30 a day
  if (h_.playDay != day) {
    h_.playDay = day;
    h_.playPoints = 0;
  }
  uint8_t win = won ? 5 : 1;
  if (h_.playPoints + win <= PLAY_POINTS_PER_DAY) {
    h_.playPoints += win;
    earn(win);
  }
  if (won) s_.gamesWon++;
  if (rainScore > s_.bestRain) s_.bestRain = rainScore;
  if (s_.gamesWon >= 3) unlock(HAT_CROWN);
  if (rainScore >= 15) unlock(HAT_PARTY);
}

// ---------------------------------------------------------------- 0.4 additions

int32_t civil(Date d) {
  // Howard Hinnant's days_from_civil.
  int y = d.y - (d.m <= 2);
  int era = (y >= 0 ? y : y - 399) / 400;
  unsigned yoe = (unsigned)(y - era * 400);
  unsigned doy = (153 * (d.m + (d.m > 2 ? -3 : 9)) + 2) / 5 + d.d - 1;
  unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + (int32_t)doe - 719468;
}

void Pet::loadExtra(const PetExtra& x) {
  if (x.version != EXTRA_VERSION) return;
  x_ = x;
  x_.who[sizeof x_.who - 1] = 0;
  for (auto& w : x_.words) w.text[PetExtra::WORD_LEN] = 0;
}

void Pet::loadBirds(const PetBirds& b) {
  if (b.version != BIRDS_VERSION) return;
  b_ = b;
  setSpecies(Species(b_.species));
  // Saved for an egg that isn't this one: an older firmware started a new egg in between, and it hatched a cockatiel.
  if (b_.eggStart != s_.eggStart) {
    b_.species = 0;
    b_.eggStart = s_.eggStart;
  }
  b_.met &= (1 << FRIEND_COUNT) - 1;
}

bool Pet::recordVisit(Friend f) {
  if (f >= FRIEND_COUNT) return false;
  if (b_.visits[f] < 0xffff) b_.visits[f]++;
  bool first = !(b_.met & (1 << f));
  b_.met |= 1 << f;
  return first;
}

int Pet::wordCount() const {
  int n = 0;
  for (auto& w : x_.words) n += w.text[0] != 0;
  return n;
}

bool Pet::learnWord(const char* text, uint32_t now) {
  if (!text || !*text) return false;
  for (auto& w : x_.words)
    if (!strncmp(w.text, text, PetExtra::WORD_LEN)) return false;
  // A free slot, else forget the oldest word.
  PetExtra::Word* slot = &x_.words[0];
  for (auto& w : x_.words) {
    if (!w.text[0]) { slot = &w; break; }
    if (w.at < slot->at) slot = &w;
  }
  strncpy(slot->text, text, PetExtra::WORD_LEN);
  slot->text[PetExtra::WORD_LEN] = 0;
  slot->at = now ? now : 1;
  return true;
}

bool Pet::forgetWord(const char* text) {
  for (auto& w : x_.words)
    if (w.text[0] && !strncmp(w.text, text, PetExtra::WORD_LEN)) {
      memset(&w, 0, sizeof w);
      return true;
    }
  return false;
}

int Pet::drawOmikuji(int32_t today, uint32_t rnd) {
  if (!canDrawOmikuji(today)) return -1;
  // 大吉 16%, 中吉 22%, 小吉 22%, 吉 20%, 末吉 14%, 凶 6%: mostly good news, like a real shrine.
  static const uint8_t CUM[] = {16, 38, 60, 80, 94, 100};
  uint32_t r = rnd % 100;
  int g = 0;
  while (r >= CUM[g]) g++;
  x_.omikujiDay = today;
  x_.omikujiGrade = g;
  x_.omikujiSeen |= 1u << g;
  earn(3);
  if (x_.omikujiSeen == 0x3f) unlock(HAT_OMAMORI);
  if (g == 0) s_.happy = inc(s_.happy, MAX_STAT);
  return g;
}

void Pet::recordDance(int score) {
  if (score > x_.bestDance) x_.bestDance = score;
  recordGame(score >= 5, 0);
}

void Pet::recordJanken(bool won) {
  if (won) x_.jankenWins++;
  recordGame(won, 0);
}

void Pet::recordFocus() {
  earn(5);
  x_.focusCount++;
  if (x_.focusCount % 10 == 0) unlock(HAT_HACHIMAKI);
  if (s_.affection < 1000) s_.affection += 5;
}

Occasion occasion(const Date& t, const PetExtra& x) {
  if (!t.y) return Occasion::None;
  for (int kind = 1; kind <= 2; kind++)
    for (const auto& d : x.dates)
      if (d.kind == kind && d.m == t.m && d.d == t.d) return kind == 1 ? Occasion::Birthday : Occasion::Anniversary;
  int md = t.m * 100 + t.d;
  if (md == 303) return Occasion::Hina;
  if (md >= 325 && md <= 410) return Occasion::Sakura;
  if (md == 505) return Occasion::Kodomo;
  if (md == 707) return Occasion::Tanabata;
  if (md == 1031) return Occasion::Halloween;
  if (md == 1205 || md == 1206) return Occasion::Sinterklaas;
  if (md >= 1224 && md <= 1226) return Occasion::Christmas;
  if (md == 1231) return Occasion::YearEnd;  // よいお年を: あけおめ only from midnight
  if (md <= 103) return Occasion::NewYear;
  return Occasion::None;
}

// ---------------------------------------------------------------- 0.6 additions

void Pet::loadLife(const PetLife& l) {
  if (l.version != LIFE_VERSION) return;
  if (l.eggStart && l.eggStart != s_.eggStart) {  // saved for another egg: only the settings carry over
    l_.alarm = l.alarm;
    l_.clockOnCover = l.clockOnCover;
    l_.alarmDay = l.alarmDay;
    l_.eggStart = s_.eggStart;
    return;
  }
  l_ = l;
  l_.eggStart = s_.eggStart;
  if (l_.training > TRICK_COUNT) l_.training = TRICK_COUNT;
  l_.tricks &= (1 << TRICK_COUNT) - 1;
}

bool Pet::maybeFuss(uint32_t now, int32_t today, uint32_t rnd) {
  if (s_.stage == Stage::Egg || s_.stage == Stage::Gone || asleep(now) || s_.lightsOff || s_.sick || away(now))
    return false;
  if (s_.hunger < 2 || s_.happy < 2 || s_.poop >= 2) return false;  // only when every need is met
  if (l_.fussAt && now - l_.fussAt < FUSS_LASTS) return false;
  l_.fussAt = 0;  // an ignored fuss just fades: never a care mistake
  if (!l_.nextFuss) {  // first time (or after an update): not straight away
    l_.nextFuss = now + HOUR + rnd % (2 * HOUR);
    return false;
  }
  if (now < l_.nextFuss) return false;
  if (l_.fussDay != today) {
    l_.fussDay = today;
    l_.fussToday = 0;
  }
  // Better trained, fewer tantrums: twice a day untrained, once from level 2, every third day when fully trained.
  int limit = l_.training >= TRICK_COUNT ? (today % 3 == 0) : l_.training >= 2 ? 1 : 2;
  if (l_.fussToday >= limit) return false;
  l_.fussToday++;
  l_.fussAt = now;
  l_.nextFuss = now + 3 * HOUR + rnd % (3 * HOUR);
  return true;
}

int Pet::calmFuss(uint32_t now) {
  if (!fussing(now)) return -1;
  l_.fussAt = 0;
  if (l_.training >= TRICK_COUNT) return -1;
  int trick = l_.training++;
  l_.tricks |= 1 << trick;
  if (s_.affection < 1000) s_.affection += 5;
  return trick;
}

Result Pet::spoilFuss(uint32_t now) {
  if (!fussing(now)) return Result::NotNeeded;
  l_.fussAt = 0;
  return act(Action::Millet, now);
}

int Pet::trickCount() const {
  int n = 0;
  for (int i = 0; i < TRICK_COUNT; i++) n += (l_.tricks >> i) & 1;
  return n;
}

int Pet::pickTrick(uint32_t rnd) const {
  int n = trickCount();
  if (!n) return -1;
  int k = rnd % n;
  for (int i = 0; i < TRICK_COUNT; i++)
    if ((l_.tricks >> i) & 1 && !k--) return i;
  return -1;
}

uint16_t Pet::partyDue(uint32_t now) const {
  if (s_.stage == Stage::Egg || s_.stage == Stage::Gone || !s_.born) return 0;
  uint32_t days = ageSeconds(now) / DAY;
  uint16_t due = 0;
  for (uint16_t d : PARTY_DAYS)
    if (days >= d && d > l_.partyDays) due = d;  // a late update celebrates only the latest milestone
  return due;
}

void Pet::recordLeftRight(int hits) {
  if (hits > l_.lrBest) l_.lrBest = hits;
  recordGame(hits >= 3, 0);
}

// ---------------------------------------------------------------- 0.7 additions

void Pet::loadHome(const PetHome& h) {
  if (h.version != HOME_VERSION) return;
  h_ = h;
  if (h_.familyCount > FAMILY_MAX) h_.familyCount = FAMILY_MAX;
  for (auto& m : h_.family) m.name[sizeof m.name - 1] = 0;
  h_.toys &= (1 << TOY_COUNT) - 1;
  s_.unlocked |= h_.hats;
  if (h_.toy > TOY_COUNT || (h_.toy && !(h_.toys & (1 << (h_.toy - 1))))) h_.toy = 0;
}

int Pet::buyTreat(uint32_t now) {
  if (s_.stage == Stage::Egg || s_.stage == Stage::Gone || s_.lightsOff || asleep(now) || away(now)) return -3;
  if (h_.points < TREAT_PRICE) return -1;
  h_.points -= TREAT_PRICE;
  s_.happy = inc(s_.happy, MAX_STAT);
  s_.hunger = inc(s_.hunger, MAX_STAT);
  return 0;
}

int Pet::buyToy(uint8_t toy) {
  if (toy >= TOY_COUNT) return -3;
  if (h_.toys & (1 << toy)) return -2;
  if (h_.points < TOY_PRICE[toy]) return -1;
  h_.points -= TOY_PRICE[toy];
  h_.toys |= 1 << toy;
  if (!h_.toy) h_.toy = toy + 1;  // straight into the cage
  return 0;
}

int Pet::buyHat(uint8_t hat) {
  if (hat == HAT_NONE || hat >= HAT_GIFTABLE) return -3;  // the earned-only charms aren't for sale
  if (s_.unlocked & (1u << hat)) return -2;
  if (h_.points < HAT_PRICE) return -1;
  h_.points -= HAT_PRICE;
  h_.hats |= 1u << hat;
  unlock(hat);
  return 0;
}

bool Pet::placeToy(uint8_t toy) {
  if (toy && (toy > TOY_COUNT || !(h_.toys & (1 << (toy - 1))))) return false;
  h_.toy = toy;
  return true;
}

bool Pet::playToy(uint32_t now) {
  if (!h_.toy || (l_.toyAt && now - l_.toyAt < TOY_COOLDOWN)) return false;
  l_.toyAt = now;
  s_.happy = inc(s_.happy, MAX_STAT);
  return true;
}

bool Pet::canGoOut(uint8_t place, uint32_t now) const {
  if (place >= PLACE_COUNT || s_.stage != Stage::Adult || l_.outing.place) return false;
  if (asleep(now) || s_.lightsOff || s_.sick || s_.hunger < 2 || s_.happy < 2) return false;
  int h = hour_(now);
  if (h < NIGHT_TO || h >= 20) return false;  // home well before bedtime
  return ageSeconds(now) / DAY >= PLACE_MIN_DAYS[place];
}

bool Pet::goOut(uint8_t place, uint32_t now, uint32_t rnd) {
  if (!canGoOut(place, now)) return false;
  l_.outing = {uint8_t(place + 1), 0, now, now + (60 + rnd % 31) * MIN};
  l_.fussAt = 0;
  return true;
}

bool Pet::canFindPartner(uint32_t now) const {
  return s_.stage == Stage::Adult && !h_.partnerAt && !away(now) && (l_.senior || ageSeconds(now) >= 20 * DAY);
}

bool Pet::findPartner(uint32_t now, int32_t today) {
  if (!canFindPartner(now)) return false;
  h_.partnerAt = now ? now : 1;
  h_.partnerDay = today;
  return true;
}

// The parent moves into the family album (it still visits now and then) and a new egg of the same kind starts.
void Pet::nextGeneration(uint32_t now) {
  if (h_.familyCount >= FAMILY_MAX) {  // the album keeps the most recent birds
    memmove(&h_.family[0], &h_.family[1], sizeof h_.family[0] * (FAMILY_MAX - 1));
    h_.familyCount = FAMILY_MAX - 1;
  }
  PetHome::Member& m = h_.family[h_.familyCount++];
  memset(&m, 0, sizeof m);
  strncpy(m.name, s_.name[0] ? s_.name : "OKAME", sizeof m.name - 1);
  m.species = b_.species;
  m.variant = uint8_t(s_.variant);
  m.golden = l_.golden;
  m.generation = h_.generation;
  uint32_t days = ageSeconds(now) / DAY;
  m.days = days > 0xffff ? 0xffff : days;
  h_.inherit = uint8_t(uint8_t(s_.variant) + 1) | (l_.golden ? 8 : 0);
  h_.generation++;
  reset(now);
}
