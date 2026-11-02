// Helpers shared by the ui_*.cpp files: the per-species sprite tables, the friends, the accessory offsets, timing.
#pragma once
#include <stdio.h>
#include "sprites.h"
#include "ui.h"

namespace uidetail {
constexpr uint32_t SHORT_MAX_MS = 500;    // shorter = short press
constexpr uint32_t DOUBLE_GAP_MS = 350;   // second press within this = double press
constexpr uint32_t SCRITCH_HOLD_MS = 800;
constexpr uint32_t PAIR_HOLD_MS = 5000;
constexpr uint32_t MENU_IDLE_MS = 6000;
constexpr uint32_t STATS_IDLE_MS = 8000;
constexpr uint32_t ANIM_MS = 2400;
constexpr uint32_t CARD_EVERY_MS = 15 * 60 * 1000;  // the countdown card, now and then on the main screen
constexpr uint32_t SAY_MIN_MS = 30 * 60 * 1000, SAY_MAX_MS = 90 * 60 * 1000;  // between spoken words
constexpr uint32_t SAY_SCROLL_MS = 100, SAY_LEAD_MS = 700, SAY_HOLD_MS = 1200, SAY_STATIC_MS = 4000;
constexpr int SAY_FIT = LCD_W - 4;  // text width that fits the bubble without scrolling
constexpr int STATS_PAGES = 5;

const Sprite* const MENU_ICONS[] = {&spr::ico_seed, &spr::ico_millet, &spr::ico_bath, &spr::ico_vet,
                                    &spr::ico_light, &spr::ico_game, &spr::ico_stats};
const char* const MENU_LABELS[] = {"SEED", "MILLET", "BATH", "VET", "LIGHT", "PLAY", "STATS"};

inline bool blink(uint32_t nowMs, uint32_t period = 500) { return (nowMs / period) % 2 == 0; }

// Each species' sprites. Per life stage (chick, juvenile, adult), indexed by Pose:
// Idle, Blink, Up, Flat, Peck, Sleep, Happy. Stages without a pose of their own reuse the nearest one.
struct Look {
  const Sprite* egg;
  const Sprite* crack;
  const Sprite* stage[3][7];
  bool sideways;  // drawn in profile, head on the right; the owl faces the front
};
inline const Look LOOKS[] = {
    {&spr::egg, &spr::egg_crack,
     {{&spr::chick_idle, &spr::chick_blink, &spr::chick_idle, &spr::chick_sleep, &spr::chick_peck, &spr::chick_sleep,
       &spr::chick_happy},
      {&spr::juv_idle, &spr::juv_blink, &spr::juv_idle, &spr::juv_flat, &spr::juv_peck, &spr::juv_sleep, &spr::juv_happy},
      {&spr::adult_idle, &spr::adult_blink, &spr::adult_up, &spr::adult_flat, &spr::adult_peck, &spr::adult_sleep,
       &spr::adult_happy}},
     true},
    {&spr::budgie_egg, &spr::budgie_egg_crack,
     {{&spr::budgie_chick_idle, &spr::budgie_chick_blink, &spr::budgie_chick_idle, &spr::budgie_chick_sleep,
       &spr::budgie_chick_peck, &spr::budgie_chick_sleep, &spr::budgie_chick_happy},
      {&spr::budgie_juv_idle, &spr::budgie_juv_blink, &spr::budgie_juv_idle, &spr::budgie_juv_blink,
       &spr::budgie_juv_peck, &spr::budgie_juv_blink, &spr::budgie_juv_happy},
      {&spr::budgie_adult_idle, &spr::budgie_adult_blink, &spr::budgie_adult_up, &spr::budgie_adult_flat,
       &spr::budgie_adult_peck, &spr::budgie_adult_flat, &spr::budgie_adult_happy}},
     true},
    {&spr::owl_egg, &spr::owl_egg_crack,
     {{&spr::owl_chick_idle, &spr::owl_chick_blink, &spr::owl_chick_idle, &spr::owl_chick_blink, &spr::owl_chick_peck,
       &spr::owl_chick_blink, &spr::owl_chick_happy},
      {&spr::owl_juv_idle, &spr::owl_juv_blink, &spr::owl_juv_up, &spr::owl_juv_flat, &spr::owl_juv_peck,
       &spr::owl_juv_blink, &spr::owl_juv_happy},
      {&spr::owl_adult_idle, &spr::owl_adult_blink, &spr::owl_adult_up, &spr::owl_adult_flat, &spr::owl_adult_peck,
       &spr::owl_adult_sleep, &spr::owl_adult_happy}},
     false},
};
inline const Look& lookOf(Species sp) { return LOOKS[uint8_t(sp) < uint8_t(Species::Count) ? uint8_t(sp) : 0]; }
inline int stageIndex(Stage st) { return st == Stage::Chick ? 0 : st == Stage::Juvenile ? 1 : 2; }

// Friends: how a visit looks. Sprites face right; `back` is where the flapping wing sits.
constexpr uint32_t FRIEND_MS = 7000, FRIEND_IN_MS = 1500, FRIEND_OUT_MS = 5600;
constexpr uint32_t FRIEND_MIN_MS = 2 * 3600 * 1000u, FRIEND_MAX_MS = 4 * 3600 * 1000u;  // between visits
constexpr int FRIEND_PER_DAY = 3;
constexpr uint32_t PIGEON_MS = 2400;  // the pigeon's part before a note or a hug
constexpr uint32_t MSG_INTRO_MS = PIGEON_MS + 1000;  // pigeon, then the envelope bounces, then the text
struct FriendLook {
  const Sprite* sprite;
  int8_t backX, backY;
};
inline const FriendLook FRIENDS[FRIEND_COUNT] = {
    {&spr::fr_budgie, 2, 4}, {&spr::fr_pigeon, 3, 4}, {&spr::fr_enaga, 5, 1}, {&spr::fr_mejiro, 4, 2}, {&spr::fr_owl, 0, 5},
};

  // Top-left of each accessory in the facing-right sprite (index = Accessory); the chick wears the bow as a hair bow.
// Omamori hangs on the chest; the hachimaki ties round the forehead, its ends trailing behind.
// The budgie has the cockatiel's build without the crest; the owl faces the front, so things sit in the middle.
inline const int8_t OFF[uint8_t(Species::Count)][3][HAT_COUNT][2] = {
  {/*chick*/ {{0, 0}, {0, 0}, {3, 3}, {2, 5}, {7, 6}, {0, 0}, {5, 6}, {2, 3}},
   /*juv  */ {{0, 0}, {11, 9}, {7, 4}, {6, 7}, {12, 8}, {3, 4}, {9, 8}, {6, 4}},
   /*adult*/ {{0, 0}, {13, 11}, {10, 5}, {9, 8}, {15, 10}, {6, 4}, {11, 10}, {9, 5}}},
  {/*chick*/ {{0, 0}, {3, 0}, {3, 3}, {2, 5}, {7, 6}, {1, 0}, {5, 6}, {2, 2}},
   /*juv  */ {{0, 0}, {11, 9}, {7, 4}, {6, 7}, {12, 8}, {3, 3}, {9, 8}, {6, 4}},
   /*adult*/ {{0, 0}, {12, 10}, {10, 4}, {9, 8}, {14, 9}, {5, 3}, {11, 10}, {9, 4}}},
  {/*chick*/ {{0, 0}, {3, 0}, {4, 4}, {2, 7}, {4, 7}, {6, 0}, {4, 7}, {2, 2}},
   /*juv  */ {{0, 0}, {4, 8}, {6, 5}, {3, 8}, {5, 8}, {4, 0}, {5, 8}, {3, 3}},
   /*adult*/ {{0, 0}, {5, 10}, {8, 7}, {4, 10}, {6, 10}, {5, 0}, {6, 10}, {3, 4}}},
};
}  // namespace uidetail
