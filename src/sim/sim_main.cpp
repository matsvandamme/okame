// Desktop simulator: renders every screen state to PNG (out/screens/) for review,
// render textures and the manual. Build/run: pio run -e sim && .pio/build/sim/program
#include <stdio.h>
#include <string.h>
#include <string>
#include <vector>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#include "render.h"
#include "ui.h"

static int g_hour = 14;
static int hourFn(uint32_t) { return g_hour; }
static std::string g_out = "out/screens";

static void savePng(const Frame& f, const std::string& name, int scale = 4) {
  std::vector<uint32_t> px(PX_W * PX_H);
  renderRgb888(f, px.data());
  std::vector<uint8_t> rgb(PX_W * scale * PX_H * scale * 3);
  for (int y = 0; y < PX_H * scale; y++)
    for (int x = 0; x < PX_W * scale; x++) {
      uint32_t c = px[(y / scale) * PX_W + x / scale];
      uint8_t* p = &rgb[(y * PX_W * scale + x) * 3];
      p[0] = c >> 16;
      p[1] = c >> 8;
      p[2] = c;
    }
  std::string path = g_out + "/" + name + ".png";
  stbi_write_png(path.c_str(), PX_W * scale, PX_H * scale, 3, rgb.data(), PX_W * scale * 3);
  printf("%s\n", path.c_str());
}

struct Sim {
  Pet pet{hourFn};
  Ui ui{pet, 7};
  Env env;
  uint32_t t = 100000;
  Sim() {
    env.online = true;
    env.epoch = 1800000000;
    env.apSsid = "OKAME-F3A";
    env.apPass = "48213907";
    pet.reset(env.epoch);
  }
  void stage(Stage s, Variant v = Variant::Grey) {
    PetState st = pet.state();
    st.stage = s;
    st.variant = v;
    st.born = env.epoch - (s == Stage::Adult ? 5 : 1) * rules::DAY;
    st.hunger = st.happy = 3;
    st.weight = s == Stage::Chick ? 31 : s == Stage::Juvenile ? 62 : 94;
    pet.load(st);
  }
  void tweak(void (*f)(PetState&)) {
    PetState st = pet.state();
    f(st);
    pet.load(st);
  }
  void run(uint32_t ms, uint32_t step = 50) {
    for (uint32_t e = 0; e < ms; e += step) {
      t += step;
      env.nowMs = t;
      ui.update(env);
    }
  }
  void press(uint32_t holdMs) {
    env.buttonDown = true;
    run(holdMs, 10);
    env.buttonDown = false;
    run(20, 10);
  }
  void shot(const char* name) { savePng(ui.frame(), name); }
};

int main(int argc, char** argv) {
  if (argc > 1) g_out = argv[1];

  {  // Egg + hatching
    Sim s;
    s.run(500);
    s.shot("01_egg");
    s.stage(Stage::Chick);  // hatching starts the animation
    s.run(1950);
    s.shot("02_hatch_crack");
    s.run(800);
    s.shot("03_hatch_chick");
  }
  {  // Each stage on the main screen
    Sim s;
    s.stage(Stage::Chick);
    s.run(1500);
    s.shot("10_main_chick");
    s.stage(Stage::Juvenile);
    s.run(1500);
    s.shot("11_main_juvenile");
    const char* names[] = {"12_main_adult_grey", "13_main_adult_pearl", "14_main_adult_pied",
                           "15_main_adult_scruffy"};
    for (int v = 0; v < 4; v++) {
      s.stage(Stage::Adult, Variant(v));
      s.run(1500);
      s.shot(names[v]);
    }
  }
  {  // Needs
    Sim s;
    s.stage(Stage::Adult, Variant::Pearl);
    s.tweak([](PetState& st) { st.poop = 2; });
    s.run(1200);
    s.shot("20_main_poop");
    s.tweak([](PetState& st) { st.poop = 0; st.hunger = 0; });
    s.run(1200);
    s.shot("21_main_sad");
    s.tweak([](PetState& st) { st.sick = true; st.hunger = 2; });
    s.run(1200);
    s.shot("22_main_sick");
    g_hour = 23;
    s.tweak([](PetState& st) { st.sick = false; });
    s.run(1200);
    s.shot("23_main_night_lights_on");
    s.tweak([](PetState& st) { st.lightsOff = true; });
    s.run(1200);
    s.shot("24_lights_off");
    g_hour = 14;
  }
  {  // Menu
    Sim s;
    s.stage(Stage::Adult, Variant::Pearl);
    s.run(500);
    const char* names[] = {"30_menu_seed", "31_menu_millet", "32_menu_bath", "33_menu_vet",
                           "34_menu_light", "35_menu_play", "36_menu_stats"};
    s.press(100);
    s.run(400);
    for (int i = 0; i < 7; i++) {
      s.shot(names[i]);
      s.press(100);
      s.run(100);
    }
  }
  {  // Stats
    Sim s;
    s.stage(Stage::Adult, Variant::Pearl);
    s.tweak([](PetState& st) { st.hunger = 3; st.happy = 2; st.tuneLen = 5; st.tuneLevel = 2; });
    s.run(300);
    s.ui.forceScreen(Screen::Stats, s.t);
    const char* names[] = {"40_stats_food", "41_stats_joy", "42_stats_age", "43_stats_weight",
                           "44_stats_tune"};
    for (int i = 0; i < 5; i++) {
      s.ui.setStatsPage(i);
      s.run(60);
      s.shot(names[i]);
    }
  }
  {  // Animations
    Sim s;
    s.stage(Stage::Adult, Variant::Pearl);
    s.run(300);
    struct { Anim a; const char* n; uint32_t at; } anims[] = {
        {Anim::Seed, "50_anim_seed", 650},      {Anim::Millet, "51_anim_millet", 650},
        {Anim::Bath, "52_anim_bath", 900},      {Anim::Vet, "53_anim_vet", 400},
        {Anim::Refused, "54_anim_refused", 400}, {Anim::NotNeeded, "55_anim_not_needed", 400},
        {Anim::Greet, "56_anim_greet", 300},    {Anim::Visitor, "57_anim_visitor", 300},
        {Anim::Whistle, "58_anim_whistle", 700}, {Anim::GameWin, "59_anim_win", 400},
        {Anim::GameLose, "60_anim_lose", 400},  {Anim::Scritch, "61_anim_scritch", 300},
    };
    for (auto& a : anims) {
      s.ui.forceAnim(a.a, s.t);
      s.run(a.at);
      s.shot(a.n);
    }
  }
  {  // Games
    Sim s;
    s.stage(Stage::Adult, Variant::Pearl);
    s.run(200);
    s.ui.duet().start(3, s.t);
    s.ui.forceScreen(Screen::Duet, s.t);
    s.run(1300);
    s.shot("70_duet_showing");
    s.run(6000);
    s.shot("71_duet_listening");
    s.ui.peek().start(5, s.t);
    s.ui.forceScreen(Screen::Peek, s.t);
    s.run(400);
    s.shot("72_peek_hidden");
    while (s.ui.peek().phase() != Peekaboo::Phase::Peeking) s.run(50);
    s.run(100);
    s.shot("73_peek_peeking");
  }
  {  // System screens
    Sim s;
    s.stage(Stage::Adult, Variant::Pearl);
    s.env.online = false;
    s.run(1000);
    s.shot("80_sleep_offline");
    {
      Sim e;  // a fresh egg, sleeping before it ever saw Wi-Fi
      e.env.online = false;
      e.run(1000);
      e.shot("80b_sleep_egg");
    }
    s.env.apMode = true;
    s.t = (s.t / 6000 + 1) * 6000;
    s.run(300);
    s.shot("81_setup_ssid");
    s.run(3000);
    s.shot("82_setup_password");
    s.env.apMode = false;
    s.env.online = true;
    s.env.pairActive = true;
    s.env.pairCode = 4721;
    s.run(300);
    s.shot("83_pair_code");
    s.env.pairActive = false;
    s.env.otaProgress = 62;
    s.run(300);
    s.shot("84_ota");
    s.env.otaProgress = -1;
    s.tweak([](PetState& st) { st.stage = Stage::Gone; });
    s.run(2600);
    s.shot("85_gone");
  }
  {  // Hats on every stage, Seed Rain and the gift
    Sim s;
    const char* hats[] = {"", "bow", "crown", "beret", "party", "sakura", "omamori", "hachimaki"};
    Stage stages[] = {Stage::Chick, Stage::Juvenile, Stage::Adult};
    const char* sn[] = {"chick", "juv", "adult"};
    for (int st = 0; st < 3; st++)
      for (int h = 1; h < HAT_COUNT; h++) {
        s.stage(stages[st], Variant::Pearl);
        PetState ps = s.pet.state();
        ps.hat = h;
        s.pet.load(ps);
        s.run(400);
        char name[64];
        snprintf(name, sizeof name, "90_hat_%s_%s", sn[st], hats[h]);
        s.shot(name);
      }
    s.ui.rain().start(3, s.t, 18);
    s.ui.forceScreen(Screen::Rain, s.t);
    s.run(2500);
    s.shot("91_rain");
    {
      PetState ps = s.pet.state();
      ps.hat = 2;
      s.pet.load(ps);
    }
    s.ui.showGift(2, s.t);
    s.run(700);
    s.shot("92_gift_box");
    s.run(1200);
    s.shot("93_gift_open");
    s.ui.showMessage("Matthieu", "I MISS YOU \x01", s.t);
    s.run(800);
    s.shot("94_mail");
    s.run(2600);
    s.shot("95_message");
  }
  {  // 0.4: love and hugs
    Sim s;
    s.stage(Stage::Adult, Variant::Pearl);
    s.run(300);
    s.ui.love(s.t);
    s.run(900);
    s.shot("a01_love");
    s.ui.showHug("Matthieu", s.t);
    s.run(700);
    s.shot("a02_hug_from");
    s.run(1700);
    s.shot("a03_hug_heart");
    s.run(400);
    s.shot("a04_hug_beat");
  }
  {  // 0.4: countdown to Matthieu's return
    Sim s;
    s.stage(Stage::Adult, Variant::Pearl);
    s.env.today = {2026, 11, 20};
    s.pet.extraMut().home = {2026, 12, 2};
    s.pet.extraMut().greetedDay = civil(s.env.today) - 1;  // first time today: crosses yesterday off
    s.pet.extraMut().lastCardDays = 13;                    // ...because yesterday's card said 13
    s.run(200);
    s.run(1500);
    s.shot("a10_countdown_cross");
    s.run(1500);
    s.shot("a11_countdown");
    s.pet.extraMut().home = {2026, 11, 20};
    s.pet.extraMut().greetedDay = 0;
    s.run(2100);
    s.run(600);
    s.shot("a12_home_text");
    s.run(1800);
    s.shot("a13_home_party");
  }
  {  // 0.4: words Matthieu taught
    Sim s;
    s.stage(Stage::Adult, Variant::Pearl);
    s.run(300);
    s.ui.say("I LOVE YOU", true, s.t);
    s.run(400);
    s.shot("a20_say_learning");
    s.ui.say("OYASUMI", false, s.t);
    s.run(400);
    s.shot("a21_say_word");
    s.ui.say("GOEDEMORGEN HARUKI", false, s.t);
    s.run(1500);
    s.shot("a22_say_scroll");
  }
  {  // 0.4: janken, dance, omikuji
    Sim s;
    s.stage(Stage::Adult, Variant::Pearl);
    s.run(300);
    s.ui.startGame(Ui::Game::Janken, s.t);
    s.run(700);
    s.shot("a30_janken_ken");
    s.ui.gameInput(Press::Three, s.t);  // paper
    s.run(900);
    s.shot("a31_janken_reveal");
    s.run(1200);
    s.run(1300);
    s.shot("a32_janken_pon");
    s.ui.startGame(Ui::Game::Dance, s.t);
    s.run(1000);
    s.shot("a33_dance_show");
    s.run(3500);
    s.shot("a34_dance_your_turn");
    s.ui.gameInput(Press::Up, s.t);
    s.run(100);
    s.shot("a35_dance_echo_hop");
    for (int g = 0; g < 6; g++) {
      s.ui.forceScreen(Screen::Main, s.t);
      s.ui.showOmikuji(g, s.t);
      if (g == 0) {
        s.run(700);
        s.shot("a40_omikuji_shake");
        s.run(1100);
        s.shot("a41_omikuji_stick");
        s.run(1000);
      } else {
        s.run(2800);
      }
      char name[32];
      snprintf(name, sizeof name, "a4%d_omikuji_%d", g + 2, g);
      s.shot(name);
    }
    s.run(1700);
    s.shot("a48_omikuji_kyo_comfort");
  }
  {  // 0.4: seasons, special days and weather
    struct { Date d; const char* n; } days[] = {
        {{2027, 6, 1}, "a50_occ_birthday"}, {{2027, 3, 3}, "a51_occ_hina"},     {{2027, 3, 30}, "a52_occ_sakura"},
        {{2027, 5, 5}, "a53_occ_kodomo"},    {{2027, 7, 7}, "a54_occ_tanabata"}, {{2026, 10, 31}, "a55_occ_halloween"},
        {{2026, 12, 6}, "a56_occ_sint"},     {{2026, 12, 25}, "a57_occ_xmas"},   {{2027, 1, 1}, "a58_occ_newyear"},     {{2026, 12, 31}, "a59_occ_yearend"},
    };
    for (auto& d : days) {
      Sim s;
      s.stage(Stage::Adult, Variant::Pearl);
      s.pet.extraMut().dates[0] = {6, 1, 1};  // a made-up birthday; the real one is sent by the relay
      snprintf(s.pet.extraMut().who, 13, "HARUKI");
      s.env.today = d.d;
      s.run(800);
      std::string n = d.n;
      s.shot((n + "_hello").c_str());
      if (n == "a50_occ_birthday") {
        s.run(900);
        s.shot((n + "_name").c_str());
      }
      s.run(3000);
      s.shot((n + "_party").c_str());
      s.run(3000);
      s.shot((n + "_main").c_str());
    }
    Weather w[] = {Weather::Rain, Weather::Snow, Weather::Hot, Weather::Cold};
    const char* wn[] = {"a60_wx_rain", "a61_wx_snow", "a62_wx_hot", "a63_wx_cold"};
    for (int i = 0; i < 4; i++) {
      Sim s;
      s.stage(i % 2 ? Stage::Chick : Stage::Adult, Variant::Pearl);
      s.ui.setWeather(w[i], s.t);
      s.run(900);
      s.shot(wn[i]);
    }
  }
  {  // 0.4: focus buddy
    Sim s;
    s.stage(Stage::Adult, Variant::Pearl);
    s.run(300);
    s.ui.startFocus(25, s.t);
    s.run(60000);
    s.shot("a70_focus");
    s.run(24 * 60000 - 20000);
    s.shot("a71_focus_seconds");
    s.run(21000);
    s.run(800);
    s.shot("a72_focus_done");
    s.run(2600);
    s.shot("a73_focus_break");
  }
  {  // 0.4: Seed Rain against Matthieu's Feather Catch score
    Sim s;
    s.stage(Stage::Adult, Variant::Pearl);
    s.pet.extraMut().rivalRain = 14;
    s.run(300);
    s.ui.startGame(Ui::Game::Rain, s.t);
    s.run(800);
    s.shot("a80_rain_vs");
  }
  {  // 0.5: the budgie and the owl through every stage and mood
    const char* sp[] = {"", "budgie", "owl"};
    for (int k = 1; k < 3; k++) {
      Sim s;
      s.pet.setSpecies(Species(k));
      s.pet.reset(s.env.epoch);
      char n[64];
      auto shot = [&](const char* what) {
        snprintf(n, sizeof n, "b%d_%s_%s", k, sp[k], what);
        s.shot(n);
      };
      s.run(500);
      shot("00_egg");
      s.stage(Stage::Chick);
      s.run(1950);
      shot("01_hatch_crack");
      s.run(800);
      shot("02_hatch_happy");
      s.run(1500);
      shot("10_chick");
      s.ui.remoteAction(Action::Seed, Result::Ok, s.t);
      s.run(400);
      shot("11_chick_peck");
      s.stage(Stage::Juvenile);
      s.run(1500);
      shot("20_juv");
      s.ui.forceAnim(Anim::Greet, s.t);
      s.run(300);
      shot("21_juv_up");
      s.ui.forceAnim(Anim::GameWin, s.t);
      s.run(300);
      shot("22_juv_happy");
      s.ui.remoteAction(Action::Seed, Result::Ok, s.t);
      s.run(400);
      shot("23_juv_peck");
      s.tweak([](PetState& st) { st.hunger = 0; });
      s.run(2600);
      shot("24_juv_sad");
      for (int v = 0; v < 4; v++) {
        s.stage(Stage::Adult, Variant(v));
        s.run(1500);
        const char* vn[] = {"30_adult_grey", "31_adult_pearl", "32_adult_pied", "33_adult_scruffy"};
        shot(vn[v]);
      }
      s.ui.forceAnim(Anim::Greet, s.t);
      s.run(300);
      shot("34_adult_up");
      s.ui.forceAnim(Anim::GameWin, s.t);
      s.run(300);
      shot("35_adult_happy");
      s.ui.remoteAction(Action::Seed, Result::Ok, s.t);
      s.run(400);
      shot("36_adult_peck");
      s.tweak([](PetState& st) { st.hunger = 0; });
      s.run(2600);
      shot("37_adult_sad");
      g_hour = 23;
      s.tweak([](PetState& st) { st.hunger = 3; });
      s.run(1200);
      shot("38_adult_asleep");
      g_hour = 14;
      s.ui.setWeather(Weather::Rain, s.t);
      s.run(900);
      shot("40_rain");
      s.ui.setWeather(Weather::Hot, s.t);
      s.run(900);
      shot("41_hot");
      s.ui.setWeather(Weather::Clear, s.t);
      s.ui.say("OYASUMI", false, s.t);
      s.run(600);
      shot("42_say");
      s.ui.startFocus(25, s.t);
      s.run(2000);
      shot("43_focus");
      s.ui.stopFocus(s.t);
      s.run(300);
      const char* hats[] = {"", "bow", "crown", "beret", "party", "sakura", "omamori", "hachimaki"};
      Stage stages[] = {Stage::Chick, Stage::Juvenile, Stage::Adult};
      const char* sn[] = {"chick", "juv", "adult"};
      for (int st = 0; st < 3; st++)
        for (int h = 1; h < HAT_COUNT; h++) {
          s.stage(stages[st], Variant::Pearl);
          s.tweak([](PetState& ps) { ps.hunger = ps.happy = 3; });
          PetState ps = s.pet.state();
          ps.hat = h;
          s.pet.load(ps);
          s.run(400);
          char what[40];
          snprintf(what, sizeof what, "9%d_hat_%s_%s", st, sn[st], hats[h]);
          shot(what);
        }
    }
  }
  {  // 0.5: friends visiting a cockatiel, and the pigeon bringing the post
    const char* fn[] = {"budgie", "pigeon", "enaga", "mejiro", "owl"};
    for (int f = 0; f < FRIEND_COUNT; f++) {
      Sim s;
      s.stage(Stage::Adult, Variant::Pearl);
      s.run(300);
      for (int k = 0; k < 40; k++) s.run(700);  // let the bird wander off to the right first
      s.ui.friendVisit(Friend(f), s.t);
      char n[48];
      uint32_t at[] = {700, 1200, 2300, 3100, 3900, 6100};
      uint32_t done = 0;
      for (int i = 0; i < 6; i++) {
        s.run(at[i] - done);
        done = at[i];
        snprintf(n, sizeof n, "c%d_friend_%s_%d", f, fn[f], i);
        s.shot(n);
      }
    }
    Sim s;
    s.stage(Stage::Adult, Variant::Pearl);
    s.run(300);
    s.ui.showMessage("Matthieu", "GOOD MORNING", s.t);
    uint32_t at[] = {500, 1100, 1700, 2200, 2900, 4200};
    uint32_t done = 0;
    char n[48];
    for (int i = 0; i < 6; i++) {
      s.run(at[i] - done);
      done = at[i];
      snprintf(n, sizeof n, "c6_pigeon_note_%d", i);
      s.shot(n);
    }
    s.run(20000);
    s.ui.showHug("Matthieu", s.t);
    done = 0;
    uint32_t hat[] = {900, 1700, 3000, 5200};
    for (int i = 0; i < 4; i++) {
      s.run(hat[i] - done);
      done = hat[i];
      snprintf(n, sizeof n, "c7_pigeon_hug_%d", i);
      s.shot(n);
    }
  }
  {  // Frame sequence of the main screen for the web mock server
    Sim s;
    s.stage(Stage::Adult, Variant::Pearl);
    std::string path = g_out + "/../web/frames.txt";
    FILE* f = fopen(path.c_str(), "w");
    for (int i = 0; i < 120; i++) {
      s.run(150);
      uint8_t packed[LCD_W * LCD_H / 8];
      s.ui.frame().pack(packed);
      for (int k = 0; k < 100; k++) fprintf(f, "%02x", packed[k]);
      fputc('\n', f);
    }
    fclose(f);
  }
  return 0;
}
