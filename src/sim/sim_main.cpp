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
    const char* hats[] = {"", "bow", "crown", "beret", "party", "sakura"};
    Stage stages[] = {Stage::Chick, Stage::Juvenile, Stage::Adult};
    const char* sn[] = {"chick", "juv", "adult"};
    for (int st = 0; st < 3; st++)
      for (int h = 1; h < 6; h++) {
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
