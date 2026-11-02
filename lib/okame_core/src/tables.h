// Generated from assets/tables.json by tools/gen_tables.py. Do not edit: change the JSON and rerun.
#pragma once
#include <stdint.h>

namespace tables {
constexpr const char* const SPECIES[] = {"cockatiel", "budgie", "owl"};
constexpr const char* const STAGES[] = {"egg", "chick", "juvenile", "adult", "gone"};
constexpr const char* const VARIANTS[] = {"grey", "pearl", "pied", "scruffy"};
constexpr const char* const MOODS[] = {"content", "meh", "sad", "sick", "asleep"};
constexpr const char* const RESULTS[] = {"ok", "refused", "not-needed", "sleeping", "away"};
constexpr const char* const OCCASIONS[] = {"", "hina", "sakura", "kodomo", "tanabata", "halloween", "sinterklaas", "christmas", "newyear", "birthday", "anniversary", "home", "yearend"};
constexpr const char* const FRIENDS[] = {"budgie", "pigeon", "enaga", "mejiro", "owl"};
constexpr const char* const PLACES[] = {"park", "cafe", "beach", "shrine", "windmill", "frites"};
constexpr const char* const PLACE_LCD[] = {"THE PARK", "THE CAFE", "THE BEACH", "THE SHRINE", "THE WINDMILL", "THE FRIETKOT"};
constexpr uint8_t PLACE_MIN_DAYS[] = {0, 0, 7, 7, 14, 14};
constexpr const char* const TOYS[] = {"swing", "bell", "mirror", "ladder", "perch"};
constexpr uint16_t TOY_PRICE[] = {30, 20, 25, 30, 20};
constexpr const char* const HATS[] = {"none", "bow", "sunglasses", "scarf", "medal", "sakura", "omamori", "hachimaki"};
constexpr uint16_t PARTY_DAYS[] = {7, 10, 30, 50, 100, 200, 365};
constexpr uint16_t HAT_PRICE = 40, TREAT_PRICE = 5;
constexpr uint8_t HAT_GIFTABLE = 6;  // accessories below this index can be gifted or bought
}  // namespace tables
