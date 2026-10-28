#pragma once
#include <stdint.h>

#ifndef FW_VERSION
#define FW_VERSION 1          // monotonically increasing; OTA only installs higher
#endif
#ifndef FW_NAME
#define FW_NAME "0.1.0"
#endif

#define OKAME_TZ "CET-1CEST,M3.5.0,M10.5.0/3"
#define OKAME_REPO "matsvandamme/okame"
#define OKAME_MANIFEST_URL "https://github.com/" OKAME_REPO "/releases/latest/download/manifest.json"
#define OKAME_IMAGE_URL "https://github.com/" OKAME_REPO "/releases/latest/download/okame.bin"

constexpr uint32_t AP_AFTER_MS = 60 * 1000;           // no saved network for this long -> setup Wi-Fi
constexpr uint32_t SAVE_EVERY_MS = 5 * 60 * 1000;
constexpr uint32_t SAVE_MIN_GAP_MS = 10 * 1000;
constexpr uint32_t PAIR_VALID_MS = 2 * 60 * 1000;
constexpr uint32_t PAIR_REQUEST_GAP_MS = 10 * 1000;   // stops a page from spamming the LCD with codes
constexpr uint32_t SESSION_VALID_MS = 30 * 60 * 1000;
constexpr int PAIR_MAX_TRIES = 5;
constexpr uint32_t OTA_CHECK_EVERY_S = 24 * 3600;
constexpr uint32_t OTA_FIRST_CHECK_MS = 3 * 60 * 1000;
constexpr uint32_t MARK_VALID_AFTER_MS = 60 * 1000;
constexpr int MAX_NETWORKS = 5;
