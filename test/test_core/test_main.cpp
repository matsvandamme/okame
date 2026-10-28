#include <unity.h>
#include "games.h"
#include "pet.h"

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
  return UNITY_END();
}
