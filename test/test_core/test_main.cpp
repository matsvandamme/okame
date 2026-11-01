#include <stdio.h>
#include <string.h>
#include <unity.h>
#include "games.h"
#include "pet.h"
#include "ui.h"

using namespace rules;

static int g_hour = 12;
static int hourFn(uint32_t) { return g_hour; }
static const uint32_t T0 = 1800000000;

void setUp() { g_hour = 12; }
void tearDown() {}

static Pet hatched() {
  Pet p(hourFn);
  p.reset(T0);
  p.tick(T0 + HATCH_AFTER);
  return p;
}

void test_egg_hatches_after_two_minutes() {
  Pet p(hourFn);
  p.reset(T0);
  p.tick(T0 + HATCH_AFTER - MIN);
  TEST_ASSERT_EQUAL(Stage::Egg, p.state().stage);
  p.tick(T0 + HATCH_AFTER);
  TEST_ASSERT_EQUAL(Stage::Chick, p.state().stage);
  TEST_ASSERT_EQUAL(30, p.state().weight);
}

void test_hunger_drops_while_awake() {
  Pet p = hatched();
  uint8_t h = p.state().hunger;
  p.tick(p.state().lastTick + HUNGER_EVERY);
  TEST_ASSERT_EQUAL(h - 1, p.state().hunger);
}

void test_hunger_drops_half_as_fast_at_night() {
  Pet p = hatched();
  g_hour = 23;
  uint8_t h = p.state().hunger;
  p.tick(p.state().lastTick + HUNGER_EVERY);
  TEST_ASSERT_EQUAL(h, p.state().hunger);
  p.tick(p.state().lastTick + HUNGER_EVERY);
  TEST_ASSERT_EQUAL(h - 1, p.state().hunger);
}

void test_catch_up_is_capped_at_twelve_hours() {
  Pet p = hatched();
  PetState st = p.state();
  st.hunger = st.happy = MAX_STAT;
  p.load(st);
  p.tick(p.state().lastTick + 10 * DAY);
  // 12 h at 100 min per heart would empty 7 hearts; we only had 4 and can't fly away.
  TEST_ASSERT_EQUAL(0, p.state().hunger);
  TEST_ASSERT_NOT_EQUAL(Stage::Gone, p.state().stage);
}

void test_flies_away_after_three_days_of_neglect() {
  Pet p = hatched();
  uint32_t t = p.state().lastTick;
  for (int i = 0; i < 9; i++) {  // keep ticking in 12 h chunks: the device stayed online
    t += 12 * HOUR;
    p.tick(t);
  }
  TEST_ASSERT_EQUAL(Stage::Gone, p.state().stage);
}

void test_seed_refused_when_full() {
  Pet p = hatched();
  uint32_t t = p.state().lastTick;
  while (p.state().hunger < MAX_STAT) TEST_ASSERT_EQUAL(Result::Ok, p.act(Action::Seed, t));
  TEST_ASSERT_EQUAL(Result::Refused, p.act(Action::Seed, t));
}

void test_bath_cleans_and_vet_cures() {
  Pet p = hatched();
  uint32_t t = p.state().lastTick;
  TEST_ASSERT_EQUAL(Result::NotNeeded, p.act(Action::Bath, t));
  TEST_ASSERT_EQUAL(Result::NotNeeded, p.act(Action::Vet, t));
  PetState st = p.state();
  st.poop = 3;
  st.sick = true;
  p.load(st);
  TEST_ASSERT_EQUAL(Result::Ok, p.act(Action::Bath, t));
  TEST_ASSERT_EQUAL(0, p.state().poop);
  TEST_ASSERT_EQUAL(Result::Ok, p.act(Action::Vet, t));
  TEST_ASSERT_FALSE(p.state().sick);
}

void test_dirty_cage_makes_sick() {
  Pet p = hatched();
  PetState st = p.state();
  st.poop = MAX_POOP;
  st.hunger = st.happy = MAX_STAT;
  p.load(st);
  p.tick(p.state().lastTick + DIRTY_SICK_AFTER);
  TEST_ASSERT_TRUE(p.state().sick);
  TEST_ASSERT_EQUAL(1, p.state().mistakes);
}

void test_lights_block_actions_and_lift_in_morning() {
  Pet p = hatched();
  uint32_t t = p.state().lastTick;
  g_hour = 23;
  p.act(Action::Light, t);
  TEST_ASSERT_EQUAL(Result::Sleeping, p.act(Action::Seed, t));
  g_hour = 9;
  p.tick(t + MIN);
  TEST_ASSERT_FALSE(p.state().lightsOff);
}

void test_lights_on_at_night_is_a_mistake_once() {
  Pet p = hatched();
  g_hour = 23;
  p.tick(p.state().lastTick + 3 * HOUR);
  TEST_ASSERT_EQUAL(1, p.state().mistakes);
}

void test_good_care_gives_pied_adult() {
  Pet p = hatched();
  PetState st = p.state();
  st.affection = 500;
  st.born = st.lastTick - ADULT_AT + MIN;
  st.stage = Stage::Juvenile;
  st.hunger = st.happy = MAX_STAT;
  p.load(st);
  p.tick(p.state().lastTick + MIN);
  TEST_ASSERT_EQUAL(Stage::Adult, p.state().stage);
  TEST_ASSERT_EQUAL(Variant::Pied, p.state().variant);
}

void test_poor_care_gives_scruffy_adult() {
  Pet p = hatched();
  PetState st = p.state();
  st.mistakes = 9;
  st.born = st.lastTick - ADULT_AT + MIN;
  st.stage = Stage::Juvenile;
  p.load(st);
  p.tick(p.state().lastTick + MIN);
  TEST_ASSERT_EQUAL(Variant::Scruffy, p.state().variant);
}

void test_tune_learned_over_three_days() {
  Pet p = hatched();
  const uint8_t notes[] = {0, 1, 2, 1};
  p.setTune(notes, 4);
  uint32_t t = p.state().lastTick;
  TEST_ASSERT_TRUE(p.practiceTune(t));
  TEST_ASSERT_FALSE(p.practiceTune(t + HOUR));  // same day doesn't count
  TEST_ASSERT_TRUE(p.practiceTune(t + DAY));
  TEST_ASSERT_TRUE(p.practiceTune(t + 2 * DAY));
  TEST_ASSERT_EQUAL(TUNE_LEARNED, p.state().tuneLevel);
  TEST_ASSERT_FALSE(p.practiceTune(t + 3 * DAY));
}

void test_scritch_has_cooldown_for_happiness() {
  Pet p = hatched();
  PetState st = p.state();
  st.happy = 1;
  p.load(st);
  uint32_t t = p.state().lastTick;
  p.act(Action::Scritch, t);
  p.act(Action::Scritch, t + MIN);
  TEST_ASSERT_EQUAL(2, p.state().happy);
  TEST_ASSERT_EQUAL(20, p.state().affection);
}

void test_load_rejects_other_version() {
  Pet p = hatched();
  PetState st = p.state();
  st.version = 99;
  TEST_ASSERT_FALSE(p.load(st));
}

void test_duet_perfect_play_wins() {
  Duet d;
  uint32_t t = 0;
  d.start(42, t);
  while (d.phase() != Duet::Phase::Done) {
    t += 50;
    d.update(t);
    if (d.phase() == Duet::Phase::Listening)
      for (int i = d.entered(); i < d.length(); i++) d.input(d.note(i) ? Press::Long : Press::Short, t);
  }
  TEST_ASSERT_EQUAL(3, d.wins());
  TEST_ASSERT_TRUE(d.won());
}

void test_duet_timeout_loses() {
  Duet d;
  uint32_t t = 0;
  d.start(1, t);
  while (d.phase() != Duet::Phase::Done) {
    t += 100;
    d.update(t);
  }
  TEST_ASSERT_FALSE(d.won());
}

void test_peekaboo_hits_only_while_peeking() {
  Peekaboo p;
  uint32_t t = 0;
  p.start(9, t);
  p.input(t + 10);  // too early
  TEST_ASSERT_EQUAL(Peekaboo::Phase::Miss, p.phase());
  while (p.phase() != Peekaboo::Phase::Done) {
    t += 20;
    p.update(t);
    if (p.phase() == Peekaboo::Phase::Peeking) p.input(t);
  }
  TEST_ASSERT_EQUAL(4, p.hits());
  TEST_ASSERT_TRUE(p.won());
}

// ---------------------------------------------------------------- 0.4

void test_civil_days() {
  TEST_ASSERT_EQUAL_INT32(0, civil({1970, 1, 1}));
  TEST_ASSERT_EQUAL_INT32(20755, civil({2026, 10, 29}));  // same as Python date arithmetic
  TEST_ASSERT_EQUAL_INT32(1, civil({2027, 1, 1}) - civil({2026, 12, 31}));  // year wrap
  TEST_ASSERT_EQUAL_INT32(29, civil({2028, 3, 1}) - civil({2028, 2, 1}));   // leap year
}

void test_days_to_home() {
  Pet p = hatched();
  Ui ui(p);
  TEST_ASSERT_EQUAL(-1, ui.daysToHome({2026, 11, 20}));  // not set
  p.extraMut().home = {2026, 12, 2};
  TEST_ASSERT_EQUAL(12, ui.daysToHome({2026, 11, 20}));
  TEST_ASSERT_EQUAL(0, ui.daysToHome({2026, 12, 2}));
  TEST_ASSERT_EQUAL(-1, ui.daysToHome({2026, 12, 3}));  // passed
  TEST_ASSERT_EQUAL(-1, ui.daysToHome({0, 0, 0}));      // clock unknown
}

void test_new_egg_keeps_relay_dates_but_not_scores() {
  Pet p = hatched();
  p.extraMut().home = {2026, 12, 2};
  p.extraMut().bestDance = 7;
  p.learnWord("HI", T0);
  p.reset(T0 + DAY);
  TEST_ASSERT_EQUAL(2026, p.extra().home.y);
  TEST_ASSERT_EQUAL(0, p.extra().bestDance);
  TEST_ASSERT_EQUAL(0, p.wordCount());
}

void test_words_learn_forget_and_cap() {
  Pet p = hatched();
  TEST_ASSERT_TRUE(p.learnWord("OYASUMI", T0));
  TEST_ASSERT_FALSE(p.learnWord("OYASUMI", T0 + 1));  // already known
  char w[8];
  for (int i = 0; i < 10; i++) {
    snprintf(w, sizeof w, "W%d", i);
    p.learnWord(w, T0 + 10 + i);
  }
  TEST_ASSERT_EQUAL(PetExtra::WORDS, p.wordCount());  // the oldest were forgotten
  TEST_ASSERT_TRUE(p.forgetWord("W9"));
  TEST_ASSERT_FALSE(p.forgetWord("NOPE"));
}

void test_omikuji_once_a_day_and_collection() {
  Pet p = hatched();
  int32_t day = civil({2026, 11, 20});
  int g = p.drawOmikuji(day, 0);
  TEST_ASSERT_EQUAL(0, g);  // 0 -> 大吉
  TEST_ASSERT_EQUAL(-1, p.drawOmikuji(day, 99));
  TEST_ASSERT_FALSE(p.canDrawOmikuji(day));
  // One of each grade over six days unlocks the omamori.
  uint32_t rolls[] = {20, 40, 70, 85, 97};
  for (int i = 0; i < 5; i++) p.drawOmikuji(day + 1 + i, rolls[i]);
  TEST_ASSERT_EQUAL_HEX8(0x3f, p.extra().omikujiSeen);
  TEST_ASSERT_TRUE(p.state().unlocked & (1u << HAT_OMAMORI));
}

void test_gift_never_gives_earned_only_accessories() {
  Pet p = hatched();
  for (uint32_t d = 0; d < 40; d++) p.openGift(T0 + d * DAY, d * 2);  // even rnd = always an accessory if any
  TEST_ASSERT_FALSE(p.state().unlocked & (1u << HAT_OMAMORI));
  TEST_ASSERT_FALSE(p.state().unlocked & (1u << HAT_HACHIMAKI));
}

void test_janken_outcomes() {
  TEST_ASSERT_EQUAL(1, Janken::outcome(Janken::Rock, Janken::Scissors));
  TEST_ASSERT_EQUAL(1, Janken::outcome(Janken::Scissors, Janken::Paper));
  TEST_ASSERT_EQUAL(1, Janken::outcome(Janken::Paper, Janken::Rock));
  TEST_ASSERT_EQUAL(-1, Janken::outcome(Janken::Rock, Janken::Paper));
  TEST_ASSERT_EQUAL(0, Janken::outcome(Janken::Paper, Janken::Paper));
}

void test_janken_ends_at_two_wins() {
  Janken j;
  uint32_t t = 1000;
  j.start(42, t);
  for (int i = 0; i < 20 && j.phase() != Janken::Phase::Done; i++) {
    j.input(Janken::Rock, t);
    t += Janken::REVEAL_MS;
    j.update(t);
  }
  TEST_ASSERT_EQUAL(Janken::Phase::Done, j.phase());
  TEST_ASSERT_TRUE(j.wins() == 2 || j.losses() == 2 || j.wins() + j.losses() <= Janken::MAX_THROWS);
}

void test_janken_times_out() {
  Janken j;
  j.start(1, 0);
  j.update(Janken::TIMEOUT_MS + 1);
  TEST_ASSERT_EQUAL(Janken::Phase::Done, j.phase());
  TEST_ASSERT_FALSE(j.won());
}

static void danceShow(Dance& d, uint32_t& t) {
  while (d.phase() == Dance::Phase::Showing) { t += 50; d.update(t); }
}

void test_dance_copy_grows_and_mistake_ends() {
  Dance d;
  uint32_t t = 0;
  d.start(7, t);
  danceShow(d, t);
  for (int i = 0; i < d.length(); i++) d.input(d.move(i), t += 100);
  TEST_ASSERT_EQUAL(Dance::Phase::Good, d.phase());
  TEST_ASSERT_EQUAL(3, d.score());
  t += Dance::RESULT_MS; d.update(t);
  TEST_ASSERT_EQUAL(4, d.length());
  danceShow(d, t);
  d.input(Dance::Move((d.move(0) + 1) % 4), t += 100);  // wrong first move
  TEST_ASSERT_EQUAL(Dance::Phase::Fail, d.phase());
  t += Dance::RESULT_MS; d.update(t);
  TEST_ASSERT_EQUAL(Dance::Phase::Done, d.phase());
  TEST_ASSERT_EQUAL(3, d.score());
}

void test_occasions() {
  PetExtra x{};
  x.dates[0] = {6, 1, 1};
  TEST_ASSERT_EQUAL(Occasion::Birthday, occasion({2027, 6, 1}, x));
  TEST_ASSERT_EQUAL(Occasion::YearEnd, occasion({2026, 12, 31}, x));  // no あけおめ before midnight
  TEST_ASSERT_EQUAL(Occasion::NewYear, occasion({2027, 1, 1}, x));
  TEST_ASSERT_EQUAL(Occasion::NewYear, occasion({2027, 1, 3}, x));
  TEST_ASSERT_EQUAL(Occasion::None, occasion({2027, 1, 4}, x));
  TEST_ASSERT_EQUAL(Occasion::Sinterklaas, occasion({2026, 12, 5}, x));
  TEST_ASSERT_EQUAL(Occasion::Sakura, occasion({2027, 4, 10}, x));
  TEST_ASSERT_EQUAL(Occasion::None, occasion({0, 0, 0}, x));
}

void test_focus_survives_a_wifi_blip() {
  Pet p = hatched();
  Ui ui(p);
  Env e;
  e.online = true;
  e.epoch = T0 + HATCH_AFTER;
  e.nowMs = 1000;
  ui.update(e);
  TEST_ASSERT_TRUE(ui.startFocus(1, e.nowMs));
  e.online = false;  // the router hiccups
  for (int i = 0; i < 20; i++) { e.nowMs += 1000; ui.update(e); }
  TEST_ASSERT_EQUAL(Screen::Focus, ui.screen());
  e.online = true;
  for (int i = 0; i < 45; i++) { e.nowMs += 1000; ui.update(e); }
  TEST_ASSERT_EQUAL(1, p.extra().focusCount);
  TEST_ASSERT_EQUAL(1, ui.takeFocusDone());
}

void test_janken_timeout_is_not_a_loss() {
  Janken j;
  j.start(1, 0);
  j.update(Janken::TIMEOUT_MS + 1);
  TEST_ASSERT_TRUE(j.timedOut());
}

// ---- 0.5: species and friends

void test_species_and_friends_survive_a_new_egg() {
  Pet p = hatched();
  TEST_ASSERT_EQUAL(Species::Cockatiel, p.species());  // a missing "pet3" means the cockatiel she has
  TEST_ASSERT_TRUE(p.recordVisit(FRIEND_ENAGA));
  TEST_ASSERT_FALSE(p.recordVisit(FRIEND_ENAGA));
  p.setSpecies(Species::Owl);
  p.reset(T0 + DAY);
  TEST_ASSERT_EQUAL(Species::Owl, p.species());
  TEST_ASSERT_EQUAL(1 << FRIEND_ENAGA, p.birds().met);
  TEST_ASSERT_EQUAL(2, p.birds().visits[FRIEND_ENAGA]);
  p.setSpecies(Species(7));
  TEST_ASSERT_EQUAL(Species::Cockatiel, p.species());
  PetBirds rolled = p.birds();  // saved for another egg: a 0.4 rollback started a new one, which is a cockatiel
  rolled.eggStart = 12345;
  p.loadBirds(rolled);
  TEST_ASSERT_EQUAL(Species::Cockatiel, p.species());
  TEST_ASSERT_EQUAL(1 << FRIEND_ENAGA, p.birds().met);  // the friends stay
  p.setSpecies(Species::Owl);
  PetBirds same = p.birds();
  p.loadBirds(same);
  TEST_ASSERT_EQUAL(Species::Owl, p.species());
  PetBirds other{};
  other.version = 99;
  other.species = 1;
  p.loadBirds(other);  // another layout: ignored
  TEST_ASSERT_EQUAL(Species::Owl, p.species());
}

static int visitsOf(const Pet& p) {
  int n = 0;
  for (int i = 0; i < FRIEND_COUNT; i++) n += p.birds().visits[i];
  return n;
}

static Env dayEnv(Date d, int hour) {
  Env e;
  e.online = true;
  e.epoch = T0 + HATCH_AFTER;
  e.nowMs = 1000;
  e.today = d;
  e.hour = hour;
  return e;
}

void test_friends_visit_at_most_three_times_a_day() {
  Pet p = hatched();
  g_hour = 14;
  Ui ui(p, 3);
  Env e = dayEnv({2026, 12, 10}, 14);  // December afternoon: the budgie, the shima-enaga, or the pigeon off duty
  bool other = false;
  for (int i = 0; i < 24 * 720; i++) {
    e.nowMs += 5000;
    ui.update(e);
    if (ui.screen() == Screen::Anim && (ui.visiting() == FRIEND_MEJIRO || ui.visiting() == FRIEND_OWL)) other = true;
  }
  TEST_ASSERT_EQUAL(3, visitsOf(p));
  TEST_ASSERT_FALSE(other);
  e.today = {2026, 12, 11};  // a new day, new visits
  for (int i = 0; i < 6 * 720; i++) { e.nowMs += 5000; ui.update(e); }
  TEST_ASSERT_TRUE(visitsOf(p) > 3);
}

void test_no_friend_out_of_season_or_of_its_own_kind() {
  Pet p = hatched();
  p.setSpecies(Species::Budgie);  // a budgie's afternoon in November: only the pigeon drops by, never a budgie
  Ui ui(p, 3);
  Env e = dayEnv({2026, 11, 10}, 14);
  for (int i = 0; i < 12 * 720; i++) { e.nowMs += 5000; ui.update(e); }
  TEST_ASSERT_EQUAL(0, p.birds().visits[FRIEND_BUDGIE]);
  TEST_ASSERT_EQUAL(visitsOf(p), p.birds().visits[FRIEND_PIGEON]);
  int before = visitsOf(p);
  e.today = {2026, 11, 13};  // an owl evening (every other day)
  e.hour = 20;
  for (int i = 0; i < 5 * 720; i++) { e.nowMs += 5000; ui.update(e); }
  TEST_ASSERT_EQUAL(visitsOf(p) - before, p.birds().visits[FRIEND_OWL]);
  TEST_ASSERT_TRUE(p.birds().visits[FRIEND_OWL] > 0);
}

void test_no_friend_during_focus() {
  Pet p = hatched();
  Ui ui(p, 3);
  Env e = dayEnv({2026, 12, 10}, 14);
  ui.update(e);
  TEST_ASSERT_TRUE(ui.startFocus(50, e.nowMs));
  for (int i = 0; i < 49 * 12; i++) { e.nowMs += 5000; ui.update(e); }
  TEST_ASSERT_EQUAL(Screen::Focus, ui.screen());
  TEST_ASSERT_EQUAL(0, visitsOf(p));
}

void test_daikichi_brings_the_owl() {
  Pet p = hatched();
  Ui ui(p, 3);
  Env e = dayEnv({2026, 11, 10}, 14);
  ui.update(e);
  ui.showOmikuji(0, e.nowMs);
  for (int i = 0; i < 70; i++) { e.nowMs += 100; ui.update(e); }  // the fortune plays for 6 s
  TEST_ASSERT_EQUAL(Screen::Anim, ui.screen());
  TEST_ASSERT_EQUAL(FRIEND_OWL, ui.visiting());
  TEST_ASSERT_EQUAL(FRIEND_OWL, ui.takeNewFriend());
}

void test_the_pigeon_brings_the_post() {
  Pet p = hatched();
  Ui ui(p, 3);
  ui.showMessage("Matthieu", "HELLO", 1000);
  TEST_ASSERT_EQUAL(FRIEND_PIGEON, ui.takeNewFriend());  // met once
  TEST_ASSERT_EQUAL(-1, ui.takeNewFriend());
  ui.showHug("Matthieu", 90000);
  TEST_ASSERT_EQUAL(-1, ui.takeNewFriend());
  TEST_ASSERT_EQUAL(2, p.birds().visits[FRIEND_PIGEON]);
}


// ---------------------------------------------------------------- 0.6: training, old age, golden

// A content adult born `days` ago, all needs met, at noon.
static Pet adultAged(uint32_t days, uint16_t mistakes = 0, uint16_t affection = 0) {
  Pet p = hatched();
  PetState st = p.state();
  st.stage = Stage::Adult;
  st.born = T0 - days * DAY;
  st.hunger = st.happy = MAX_STAT;
  st.mistakes = mistakes;
  st.affection = affection;
  st.lastTick = T0;
  p.load(st);
  return p;
}

void test_fuss_only_when_needs_met_and_fades_without_a_mistake() {
  Pet p = adultAged(5);
  p.tick(T0 + MIN);
  TEST_ASSERT_FALSE(p.maybeFuss(T0 + MIN, 100, 0));  // first call only schedules
  uint32_t t = p.life().nextFuss;
  TEST_ASSERT_TRUE(t >= T0 + MIN + HOUR);
  PetState st = p.state();
  st.hunger = 1;
  p.load(st);
  TEST_ASSERT_FALSE(p.maybeFuss(t, 100, 0));  // hungry: a real need, not a fuss
  st.hunger = MAX_STAT;
  p.load(st);
  TEST_ASSERT_TRUE(p.maybeFuss(t, 100, 0));
  TEST_ASSERT_TRUE(p.fussing(t + MIN));
  TEST_ASSERT_FALSE(p.needsAttention(t + MIN));  // the relay never nags about a tantrum
  uint16_t m = p.state().mistakes;
  TEST_ASSERT_FALSE(p.fussing(t + Pet::FUSS_LASTS));  // ignored: it fades
  p.maybeFuss(t + Pet::FUSS_LASTS, 100, 0);
  TEST_ASSERT_EQUAL(0, p.life().fussAt);
  TEST_ASSERT_EQUAL(m, p.state().mistakes);
  g_hour = 23;
  TEST_ASSERT_FALSE(p.maybeFuss(p.life().nextFuss, 100, 0));  // never at night
}

void test_calm_no_teaches_tricks_and_training_means_fewer_fusses() {
  Pet p = adultAged(5);
  uint32_t now = T0;
  p.maybeFuss(now, 100, 0);
  int fusses = 0;
  for (int day = 100; day < 103; day++)
    for (int i = 0; i < 8; i++) {
      now = p.life().nextFuss > now ? p.life().nextFuss : now + HOUR;
      if (p.maybeFuss(now, day, i)) fusses++;
    }
  TEST_ASSERT_EQUAL(6, fusses);  // untrained: twice a day
  TEST_ASSERT_EQUAL(-1, p.calmFuss(now + Pet::FUSS_LASTS + 1));  // too late, it already faded
  for (int k = 0; k < TRICK_COUNT; k++) {
    PetLife l = p.life();
    l.fussAt = now;
    p.loadLife(l);
    TEST_ASSERT_EQUAL(k, p.calmFuss(now + MIN));
  }
  TEST_ASSERT_EQUAL(TRICK_COUNT, p.life().training);
  TEST_ASSERT_EQUAL(TRICK_COUNT, p.trickCount());
  TEST_ASSERT_TRUE(p.pickTrick(7) >= 0);
  fusses = 0;
  for (int day = 200; day < 206; day++)
    for (int i = 0; i < 8; i++) {
      now = p.life().nextFuss > now ? p.life().nextFuss : now + HOUR;
      if (p.maybeFuss(now, day, i)) fusses++;
    }
  TEST_ASSERT_EQUAL(2, fusses);  // fully trained: every third day
  PetLife l = p.life();
  l.fussAt = now;
  p.loadLife(l);
  uint8_t joy = p.state().happy;
  TEST_ASSERT_EQUAL(Result::Ok, p.spoilFuss(now + MIN));
  TEST_ASSERT_TRUE(p.state().happy >= joy);
  TEST_ASSERT_EQUAL(TRICK_COUNT, p.life().training);  // giving in teaches nothing
}

void test_age_parties_once_and_only_the_latest_after_a_gap() {
  Pet p = adultAged(4);
  TEST_ASSERT_EQUAL(0, p.partyDue(T0));
  TEST_ASSERT_EQUAL(7, p.partyDue(T0 + 3 * DAY));
  p.markParty(7);
  TEST_ASSERT_EQUAL(0, p.partyDue(T0 + 4 * DAY));
  TEST_ASSERT_EQUAL(30, p.partyDue(T0 + 40 * DAY));  // missed 10: only 30 plays, once
  p.markParty(30);
  TEST_ASSERT_EQUAL(0, p.partyDue(T0 + 45 * DAY));
  Pet egg(hourFn);
  egg.reset(T0);
  TEST_ASSERT_EQUAL(0, egg.partyDue(T0 + 400 * DAY));
}

void test_senior_at_thirty_days_and_the_golden_rule() {
  // Manon's case: already an adult when 0.6 arrives, with old mistakes that don't count against her.
  Pet p = adultAged(29, 5, 250);
  p.tick(T0 + MIN);
  TEST_ASSERT_EQUAL(1, p.life().baselined);
  TEST_ASSERT_EQUAL(5, p.life().adultMistakes);
  TEST_ASSERT_FALSE(p.senior());
  PetState st = p.state();
  st.mistakes = 6;  // one slip since: still golden
  st.born = T0 - 30 * DAY;  // and today she turns 30 days old
  p.load(st);
  p.tick(T0 + 2 * MIN);
  TEST_ASSERT_TRUE(p.senior());
  TEST_ASSERT_TRUE(p.golden());
  // Two slips, or too little affection: a plain senior.
  Pet q = adultAged(29, 0, 250);
  q.tick(T0 + MIN);
  st = q.state();
  st.mistakes = 2;
  st.born = T0 - 30 * DAY;
  q.load(st);
  q.tick(T0 + 2 * MIN);
  TEST_ASSERT_TRUE(q.senior());
  TEST_ASSERT_FALSE(q.golden());
  Pet r = adultAged(30, 0, 150);
  r.tick(T0 + MIN);
  TEST_ASSERT_TRUE(r.senior());
  TEST_ASSERT_FALSE(r.golden());
}

void test_senior_eats_less_often() {
  Pet p = adultAged(31);
  p.tick(T0 + MIN);
  TEST_ASSERT_TRUE(p.senior());
  uint8_t h = p.state().hunger;
  p.tick(p.state().lastTick + HUNGER_EVERY);
  TEST_ASSERT_EQUAL(h, p.state().hunger);
  p.tick(p.state().lastTick + HUNGER_EVERY / 4);
  TEST_ASSERT_EQUAL(h - 1, p.state().hunger);
}

void test_life_on_a_new_egg_and_rollback() {
  Pet p = adultAged(31);
  p.tick(T0 + MIN);
  PetLife l = p.life();
  l.training = 3;
  l.tricks = 7;
  l.alarm = {7, 30, 0x1f, 1};
  l.clockOnCover = 1;
  p.loadLife(l);
  p.reset(T0 + DAY);
  TEST_ASSERT_EQUAL(0, p.life().training);
  TEST_ASSERT_EQUAL(0, p.life().senior);
  TEST_ASSERT_EQUAL(0, p.life().baselined);
  TEST_ASSERT_EQUAL(7, p.life().alarm.h);  // settings stay
  TEST_ASSERT_EQUAL(1, p.life().clockOnCover);
  PetLife other{};
  other.version = 9;
  other.training = 2;
  p.loadLife(other);  // another layout: ignored
  TEST_ASSERT_EQUAL(0, p.life().training);
}


void test_left_or_right_five_rounds() {
  LeftRight g;
  uint32_t t = 1000;
  g.start(9, t);
  for (int i = 0; i < 10 && g.phase() != LeftRight::Phase::Done; i++) {
    g.input(true, t);
    TEST_ASSERT_EQUAL(LeftRight::Phase::Turn, g.phase());
    g.input(false, t);  // a second tap during the turn is ignored
    t += LeftRight::TURN_MS;
    g.update(t);
  }
  TEST_ASSERT_EQUAL(LeftRight::Phase::Done, g.phase());
  TEST_ASSERT_EQUAL(LeftRight::ROUNDS, g.round());
  TEST_ASSERT_EQUAL(g.hits() >= 3, g.won());
  LeftRight idle;
  idle.start(1, 0);
  idle.update(LeftRight::TIMEOUT_MS + 1);
  TEST_ASSERT_TRUE(idle.timedOut());
}

void test_party_plays_on_the_main_screen_once() {
  Pet p = adultAged(7);
  Ui ui(p, 3);
  Env e = dayEnv({2026, 11, 13}, 14);
  e.epoch = T0;
  for (int i = 0; i < 5; i++) { e.nowMs += 100; ui.update(e); }
  TEST_ASSERT_EQUAL(Screen::Anim, ui.screen());
  TEST_ASSERT_EQUAL(7, ui.takeParty());
  TEST_ASSERT_EQUAL(0, ui.takeParty());
  for (int i = 0; i < 200; i++) { e.nowMs += 100; ui.update(e); }
  TEST_ASSERT_EQUAL(0, ui.takeParty());
  TEST_ASSERT_EQUAL(7, p.life().partyDays);
}

void test_alarm_greets_on_its_days_only() {
  Pet p = adultAged(5);
  p.lifeMut().alarm = {7, 30, 0b0011111, 1};  // weekdays
  p.lifeMut().partyDays = 7;
  Ui ui(p, 3);
  Env e = dayEnv({2026, 11, 14}, 7);  // a Saturday
  e.epoch = T0;
  e.minute = 30;
  for (int i = 0; i < 3; i++) { e.nowMs += 100; ui.update(e); }
  TEST_ASSERT_NOT_EQUAL(Screen::Anim, ui.screen());
  e.today = {2026, 11, 16};  // Monday
  e.nowMs += 100;
  ui.update(e);
  TEST_ASSERT_EQUAL(Screen::Anim, ui.screen());
  for (int i = 0; i < 80; i++) { e.nowMs += 100; ui.update(e); }  // the greeting ends; same minute: not again
  TEST_ASSERT_NOT_EQUAL(Screen::Anim, ui.screen());
}

void test_calm_no_shows_the_new_trick() {
  Pet p = adultAged(5);
  p.lifeMut().partyDays = 7;
  Ui ui(p, 3);
  Env e = dayEnv({2026, 11, 13}, 14);
  e.epoch = T0;
  ui.update(e);
  p.lifeMut().fussAt = T0;
  int k = p.calmFuss(T0 + MIN);
  TEST_ASSERT_EQUAL(TRICK_WAVE, k);
  ui.calmed(k, e.nowMs);
  for (int i = 0; i < 30; i++) { e.nowMs += 100; ui.update(e); }
  TEST_ASSERT_EQUAL(Screen::Anim, ui.screen());  // the trick follows the calm-down
  TEST_ASSERT_TRUE(ui.showTrick(TRICK_WAVE, e.nowMs));
  TEST_ASSERT_FALSE(ui.showTrick(TRICK_HANG, e.nowMs));  // not learned
}


void test_life_belongs_to_its_egg_and_reveals_survive_a_reboot() {
  Pet p = adultAged(31, 0, 250);
  p.tick(T0 + MIN);
  TEST_ASSERT_TRUE(p.golden());
  PetLife saved = p.life();
  TEST_ASSERT_EQUAL(p.state().eggStart, saved.eggStart);
  // A reboot before the reveal could play (at night): the flag is saved, the moment still comes.
  Pet q = adultAged(31, 0, 250);
  q.loadLife(saved);
  Ui ui(q, 3);
  Env e = dayEnv({2026, 11, 13}, 14);
  e.epoch = T0 + MIN;
  for (int i = 0; i < 3; i++) { e.nowMs += 100; ui.update(e); }
  TEST_ASSERT_EQUAL(2, ui.takeMilestone());
  TEST_ASSERT_EQUAL(1, q.life().goldenShown);
  // Another egg (a rollback started one): only the settings carry over.
  saved.eggStart = 12345;
  saved.alarm = {6, 45, 1, 1};
  Pet r = adultAged(5);
  r.loadLife(saved);
  TEST_ASSERT_FALSE(r.golden());
  TEST_ASSERT_EQUAL(0, r.life().training);
  TEST_ASSERT_EQUAL(6, r.life().alarm.h);
}

void test_alarm_waits_for_a_busy_bird_and_never_repeats() {
  Pet p = adultAged(5);
  p.lifeMut().alarm = {7, 30, 0x7f, 1};
  p.lifeMut().partyDays = 7;
  Ui ui(p, 3);
  Env e = dayEnv({2026, 11, 16}, 7);
  e.epoch = T0;
  e.minute = 29;
  ui.update(e);
  ui.startGame(Ui::Game::LeftRight, e.nowMs);
  e.minute = 30;
  for (int i = 0; i < 3; i++) { e.nowMs += 100; ui.update(e); }
  TEST_ASSERT_EQUAL(Screen::LeftRight, ui.screen());  // a game isn't cut off
  for (int i = 0; i < 130; i++) { e.nowMs += 100; ui.update(e); }  // nobody plays: the game ends
  e.minute = 33;
  e.nowMs += 100;
  ui.update(e);
  TEST_ASSERT_EQUAL(Screen::Anim, ui.screen());  // greeted, a little late
  int32_t day = p.life().alarmDay;
  Ui again(p, 4);  // a reboot in the same minutes
  for (int i = 0; i < 3; i++) { e.nowMs += 100; again.update(e); }
  TEST_ASSERT_EQUAL(Screen::Main, again.screen());
  TEST_ASSERT_EQUAL(day, p.life().alarmDay);
}


// ---------------------------------------------------------------- 0.7: seeds, shop, toys, outings, family

void test_seeds_from_care_capped_and_from_play() {
  Pet p = adultAged(5);
  uint32_t now = T0;
  for (int i = 0; i < 15; i++) p.act(Action::Scritch, now + i);
  TEST_ASSERT_EQUAL(10, p.home().points);  // ten a day from care
  p.act(Action::Scritch, now + DAY);
  TEST_ASSERT_EQUAL(11, p.home().points);  // a new day
  p.recordGame(true, 0);
  p.recordGame(false, 0);
  TEST_ASSERT_EQUAL(17, p.home().points);
  p.markParty(7);
  p.markParty(7);
  TEST_ASSERT_EQUAL(37, p.home().points);
  TEST_ASSERT_EQUAL(Result::Ok, p.act(Action::Light, now + DAY));
  TEST_ASSERT_EQUAL(37, p.home().points);  // covering the cage isn't care
}

void test_shop_toys_hats_and_treats() {
  Pet p = adultAged(5);
  TEST_ASSERT_EQUAL(-1, p.buyToy(TOY_BELL));
  p.earn(100);
  TEST_ASSERT_EQUAL(0, p.buyToy(TOY_BELL));
  TEST_ASSERT_EQUAL(TOY_BELL + 1, p.home().toy);  // straight into the cage
  TEST_ASSERT_EQUAL(-2, p.buyToy(TOY_BELL));
  TEST_ASSERT_EQUAL(80, p.home().points);
  TEST_ASSERT_EQUAL(0, p.buyHat(HAT_CROWN));
  TEST_ASSERT_TRUE(p.wear(HAT_CROWN));
  TEST_ASSERT_EQUAL(-2, p.buyHat(HAT_CROWN));
  TEST_ASSERT_EQUAL(-3, p.buyHat(HAT_OMAMORI));  // earned only
  TEST_ASSERT_FALSE(p.placeToy(TOY_SWING + 1));    // not bought
  TEST_ASSERT_TRUE(p.placeToy(0));
  PetState st = p.state();
  st.happy = 1;
  p.load(st);
  TEST_ASSERT_EQUAL(0, p.buyTreat(T0));
  TEST_ASSERT_EQUAL(2, p.state().happy);
  TEST_ASSERT_EQUAL(35, p.home().points);
  p.placeToy(TOY_BELL + 1);
  TEST_ASSERT_TRUE(p.playToy(T0));
  TEST_ASSERT_FALSE(p.playToy(T0 + 10 * MIN));  // once an hour
  TEST_ASSERT_TRUE(p.playToy(T0 + HOUR));
}

void test_outing_pauses_home_and_brings_a_souvenir() {
  Pet p = adultAged(8);
  p.tick(T0);
  TEST_ASSERT_FALSE(p.canGoOut(PLACE_WINDMILL, T0));  // 14 days old first
  g_hour = 21;
  TEST_ASSERT_FALSE(p.canGoOut(PLACE_PARK, T0));      // too late in the day
  g_hour = 12;
  TEST_ASSERT_TRUE(p.goOut(PLACE_BEACH, T0, 0));
  TEST_ASSERT_TRUE(p.away(T0 + MIN));
  TEST_ASSERT_EQUAL(Result::Away, p.act(Action::Seed, T0 + MIN));
  TEST_ASSERT_FALSE(p.goOut(PLACE_PARK, T0 + MIN, 0));
  uint8_t hunger = p.state().hunger;
  p.tick(T0 + 59 * MIN);
  TEST_ASSERT_EQUAL(hunger, p.state().hunger);       // time stands still for its hearts
  TEST_ASSERT_EQUAL(-1, p.homeFrom());
  p.tick(T0 + 61 * MIN);
  TEST_ASSERT_FALSE(p.away(T0 + 61 * MIN));
  TEST_ASSERT_EQUAL(PLACE_BEACH, p.homeFrom());
  TEST_ASSERT_EQUAL(1 << PLACE_BEACH, p.home().souvenirs);
  TEST_ASSERT_EQUAL(10, p.home().points);
  p.greetedHome();
  TEST_ASSERT_EQUAL(-1, p.homeFrom());
  TEST_ASSERT_TRUE(p.canGoOut(PLACE_PARK, T0 + 62 * MIN));
}

void test_a_new_generation_keeps_the_family_and_the_home() {
  Pet p = adultAged(10);
  PetState st = p.state();
  st.variant = Variant::Pied;
  strcpy(st.name, "MANON");
  p.load(st);
  p.setSpecies(Species::Cockatiel);
  p.earn(50);
  TEST_ASSERT_FALSE(p.canFindPartner(T0));  // not before 20 days, or old age
  st.born = T0 - 21 * DAY;
  p.load(st);
  TEST_ASSERT_TRUE(p.findPartner(T0, 100));
  TEST_ASSERT_FALSE(p.eggDue(100));
  TEST_ASSERT_TRUE(p.eggDue(101));  // the next morning
  p.nextGeneration(T0 + DAY);
  TEST_ASSERT_EQUAL(Stage::Egg, p.state().stage);
  TEST_ASSERT_EQUAL(0, p.state().name[0]);
  TEST_ASSERT_EQUAL(1, p.home().familyCount);
  TEST_ASSERT_EQUAL_STRING("MANON", p.home().family[0].name);
  TEST_ASSERT_EQUAL(uint8_t(Variant::Pied), p.home().family[0].variant);
  TEST_ASSERT_EQUAL(22, p.home().family[0].days);
  TEST_ASSERT_EQUAL(1, p.home().generation);
  TEST_ASSERT_EQUAL(50, p.home().points);  // the home stays
  TEST_ASSERT_EQUAL(0, p.home().partnerAt);
  for (int i = 0; i < 8; i++) p.nextGeneration(T0 + 2 * DAY);
  TEST_ASSERT_EQUAL(FAMILY_MAX, p.home().familyCount);  // the album keeps the latest birds
  PetHome other{};
  other.version = 9;
  other.points = 5;
  p.loadHome(other);
  TEST_ASSERT_EQUAL(50, p.home().points);
}


void test_outing_on_screen_sends_one_postcard() {
  Pet p = adultAged(10);
  p.lifeMut().partyDays = 10;
  Ui ui(p, 3);
  Env e = dayEnv({2026, 11, 13}, 11);
  e.epoch = T0;
  ui.update(e);
  TEST_ASSERT_TRUE(ui.goOut(PLACE_PARK, e.nowMs));
  for (int i = 0; i < 40; i++) { e.nowMs += 100; ui.update(e); }
  TEST_ASSERT_EQUAL(Screen::Main, ui.screen());
  TEST_ASSERT_FALSE(ui.startGame(Ui::Game::Janken, e.nowMs));  // nobody home to play
  TEST_ASSERT_FALSE(ui.showTrick(-1, e.nowMs));
  e.epoch = T0 + 2 * HOUR;
  p.tick(e.epoch);
  int cards = 0;
  for (int i = 0; i < 70; i++) {
    e.nowMs += 100;
    ui.update(e);
    if (int c = ui.takePostcard()) { cards++; TEST_ASSERT_EQUAL(PLACE_PARK + 1, c); }
  }
  TEST_ASSERT_EQUAL(1, cards);
  TEST_ASSERT_EQUAL(-1, p.homeFrom());
  TEST_ASSERT_EQUAL(1 << PLACE_PARK, p.home().souvenirs);
}

void test_partner_then_an_egg_the_next_morning() {
  Pet p = adultAged(21);
  p.lifeMut().partyDays = 10;
  PetState st = p.state();
  strcpy(st.name, "MANON");
  p.load(st);
  Ui ui(p, 3);
  Env e = dayEnv({2026, 11, 13}, 15);
  e.epoch = T0;
  ui.update(e);
  TEST_ASSERT_TRUE(p.findPartner(e.epoch, civil(e.today)));
  ui.partnerVisit(e.nowMs);
  for (int i = 0; i < 80; i++) { e.nowMs += 100; ui.update(e); }
  TEST_ASSERT_EQUAL(Stage::Adult, p.state().stage);  // not today
  TEST_ASSERT_FALSE(ui.takeNewGeneration());
  e.today = {2026, 11, 14};
  e.hour = 9;
  for (int i = 0; i < 3; i++) { e.nowMs += 100; ui.update(e); }
  TEST_ASSERT_TRUE(ui.takeNewGeneration());
  TEST_ASSERT_EQUAL(Stage::Egg, p.state().stage);
  TEST_ASSERT_EQUAL_STRING("MANON", p.home().family[0].name);
}


void test_a_restarted_egg_cancels_the_partner_and_keeps_bought_hats() {
  Pet p = adultAged(21);
  p.earn(100);
  TEST_ASSERT_EQUAL(0, p.buyHat(HAT_BOW));
  TEST_ASSERT_TRUE(p.findPartner(T0, 100));
  p.reset(T0 + HOUR);  // a new egg before the morning
  TEST_ASSERT_EQUAL(0, p.home().partnerAt);
  TEST_ASSERT_FALSE(p.eggDue(200));
  TEST_ASSERT_TRUE(p.state().unlocked & (1 << HAT_BOW));  // bought: the family keeps it
  for (int i = 0; i < 20; i++) p.recordGame(true, 0);
  TEST_ASSERT_EQUAL(100 - HAT_PRICE + Pet::PLAY_POINTS_PER_DAY, (int)p.home().points);  // games pay 30 a day at most
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_egg_hatches_after_two_minutes);
  RUN_TEST(test_hunger_drops_while_awake);
  RUN_TEST(test_hunger_drops_half_as_fast_at_night);
  RUN_TEST(test_catch_up_is_capped_at_twelve_hours);
  RUN_TEST(test_flies_away_after_three_days_of_neglect);
  RUN_TEST(test_seed_refused_when_full);
  RUN_TEST(test_bath_cleans_and_vet_cures);
  RUN_TEST(test_dirty_cage_makes_sick);
  RUN_TEST(test_lights_block_actions_and_lift_in_morning);
  RUN_TEST(test_lights_on_at_night_is_a_mistake_once);
  RUN_TEST(test_good_care_gives_pied_adult);
  RUN_TEST(test_poor_care_gives_scruffy_adult);
  RUN_TEST(test_tune_learned_over_three_days);
  RUN_TEST(test_scritch_has_cooldown_for_happiness);
  RUN_TEST(test_load_rejects_other_version);
  RUN_TEST(test_duet_perfect_play_wins);
  RUN_TEST(test_duet_timeout_loses);
  RUN_TEST(test_peekaboo_hits_only_while_peeking);
  RUN_TEST(test_civil_days);
  RUN_TEST(test_days_to_home);
  RUN_TEST(test_new_egg_keeps_relay_dates_but_not_scores);
  RUN_TEST(test_words_learn_forget_and_cap);
  RUN_TEST(test_omikuji_once_a_day_and_collection);
  RUN_TEST(test_gift_never_gives_earned_only_accessories);
  RUN_TEST(test_janken_outcomes);
  RUN_TEST(test_janken_ends_at_two_wins);
  RUN_TEST(test_janken_times_out);
  RUN_TEST(test_dance_copy_grows_and_mistake_ends);
  RUN_TEST(test_occasions);
  RUN_TEST(test_focus_survives_a_wifi_blip);
  RUN_TEST(test_janken_timeout_is_not_a_loss);
  RUN_TEST(test_species_and_friends_survive_a_new_egg);
  RUN_TEST(test_friends_visit_at_most_three_times_a_day);
  RUN_TEST(test_no_friend_out_of_season_or_of_its_own_kind);
  RUN_TEST(test_no_friend_during_focus);
  RUN_TEST(test_daikichi_brings_the_owl);
  RUN_TEST(test_the_pigeon_brings_the_post);
  RUN_TEST(test_fuss_only_when_needs_met_and_fades_without_a_mistake);
  RUN_TEST(test_calm_no_teaches_tricks_and_training_means_fewer_fusses);
  RUN_TEST(test_age_parties_once_and_only_the_latest_after_a_gap);
  RUN_TEST(test_senior_at_thirty_days_and_the_golden_rule);
  RUN_TEST(test_senior_eats_less_often);
  RUN_TEST(test_life_on_a_new_egg_and_rollback);
  RUN_TEST(test_left_or_right_five_rounds);
  RUN_TEST(test_party_plays_on_the_main_screen_once);
  RUN_TEST(test_alarm_greets_on_its_days_only);
  RUN_TEST(test_calm_no_shows_the_new_trick);
  RUN_TEST(test_life_belongs_to_its_egg_and_reveals_survive_a_reboot);
  RUN_TEST(test_alarm_waits_for_a_busy_bird_and_never_repeats);
  RUN_TEST(test_seeds_from_care_capped_and_from_play);
  RUN_TEST(test_shop_toys_hats_and_treats);
  RUN_TEST(test_outing_pauses_home_and_brings_a_souvenir);
  RUN_TEST(test_a_new_generation_keeps_the_family_and_the_home);
  RUN_TEST(test_outing_on_screen_sends_one_postcard);
  RUN_TEST(test_partner_then_an_egg_the_next_morning);
  RUN_TEST(test_a_restarted_egg_cancels_the_partner_and_keeps_bought_hats);
  return UNITY_END();
}
