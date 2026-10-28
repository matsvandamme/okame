#include "ota.h"
#include <ArduinoJson.h>
#include <esp_crt_bundle.h>
#include <esp_http_client.h>
#include <esp_ota_ops.h>
#include <mbedtls/base64.h>
#include <mbedtls/pk.h>
#include <mbedtls/sha256.h>
#include "config.h"
#include "ota_pubkey.h"

namespace {
volatile int prog = -1;
volatile bool running = false, wanted = false;
char statusText[64] = "never checked";
uint32_t lastCheckEpoch = 0;
bool markedValid = false;

void setStatus(const char* s) {
  strlcpy(statusText, s, sizeof statusText);
  log_i("ota: %s", s);
}

esp_http_client_handle_t openUrl(const char* url) {
  esp_http_client_config_t cfg = {};
  cfg.url = url;
  cfg.crt_bundle_attach = esp_crt_bundle_attach;
  cfg.timeout_ms = 15000;
  cfg.buffer_size = 4096;
  cfg.buffer_size_tx = 2048;
  cfg.max_redirection_count = 5;
  esp_http_client_handle_t c = esp_http_client_init(&cfg);
  if (!c) return nullptr;
  // GitHub answers with redirects; follow them before reading the body.
  for (int i = 0; i < 6; i++) {
    if (esp_http_client_open(c, 0) != ESP_OK) break;
    esp_http_client_fetch_headers(c);
    int code = esp_http_client_get_status_code(c);
    if (code == 301 || code == 302 || code == 303 || code == 307 || code == 308) {
      esp_http_client_set_redirection(c);
      esp_http_client_close(c);
      continue;
    }
    if (code == 200) return c;
    log_w("ota: HTTP %d for %s", code, url);
    break;
  }
  esp_http_client_cleanup(c);
  return nullptr;
}

bool fetchText(const char* url, String& out, size_t max) {
  esp_http_client_handle_t c = openUrl(url);
  if (!c) return false;
  char buf[512];
  int n;
  while ((n = esp_http_client_read(c, buf, sizeof buf)) > 0 && out.length() < max) out.concat(buf, n);
  esp_http_client_close(c);
  esp_http_client_cleanup(c);
  return n >= 0;
}

bool verifySignature(const String& message, const char* sigB64) {
  uint8_t sig[128];
  size_t sigLen = 0;
  if (mbedtls_base64_decode(sig, sizeof sig, &sigLen, (const uint8_t*)sigB64, strlen(sigB64))) return false;
  uint8_t hash[32];
  mbedtls_sha256((const uint8_t*)message.c_str(), message.length(), hash, 0);
  mbedtls_pk_context pk;
  mbedtls_pk_init(&pk);
  bool ok = mbedtls_pk_parse_public_key(&pk, (const uint8_t*)OTA_PUBKEY_PEM, sizeof OTA_PUBKEY_PEM) == 0 &&
            mbedtls_pk_verify(&pk, MBEDTLS_MD_SHA256, hash, sizeof hash, sig, sigLen) == 0;
  mbedtls_pk_free(&pk);
  return ok;
}

void run() {
  String body;
  if (!fetchText(OKAME_MANIFEST_URL, body, 2048)) return setStatus("no release found");
  JsonDocument doc;
  if (deserializeJson(doc, body)) return setStatus("bad manifest");
  int version = doc["version"] | 0;
  uint32_t size = doc["size"] | 0;
  const char* sha = doc["sha256"] | "";
  const char* sig = doc["sig"] | "";
  const char* name = doc["name"] | "?";
  if (version <= FW_VERSION) return setStatus("up to date");
  String msg = String("okame:") + version + ":" + size + ":" + sha;
  if (strlen(sha) != 64 || !verifySignature(msg, sig)) return setStatus("REJECTED: bad signature");

  const esp_partition_t* part = esp_ota_get_next_update_partition(nullptr);
  if (!part || size == 0 || size > part->size) return setStatus("image too large");
  esp_http_client_handle_t c = openUrl(OKAME_IMAGE_URL);
  if (!c) return setStatus("download failed");
  esp_ota_handle_t h;
  if (esp_ota_begin(part, size, &h) != ESP_OK) {
    esp_http_client_cleanup(c);
    return setStatus("cannot start update");
  }
  mbedtls_sha256_context sctx;
  mbedtls_sha256_init(&sctx);
  mbedtls_sha256_starts(&sctx, 0);
  static uint8_t buf[4096];
  uint32_t got = 0;
  int n;
  prog = 0;
  while ((n = esp_http_client_read(c, (char*)buf, sizeof buf)) > 0) {
    if (got + n > size || esp_ota_write(h, buf, n) != ESP_OK) {
      n = -1;
      break;
    }
    mbedtls_sha256_update(&sctx, buf, n);
    got += n;
    prog = (int)(100ull * got / size);
  }
  esp_http_client_close(c);
  esp_http_client_cleanup(c);
  uint8_t digest[32];
  mbedtls_sha256_finish(&sctx, digest);
  mbedtls_sha256_free(&sctx);
  char hex[65];
  for (int i = 0; i < 32; i++) sprintf(hex + i * 2, "%02x", digest[i]);
  if (n < 0 || got != size || strcasecmp(hex, sha) != 0) {
    esp_ota_abort(h);
    prog = -1;
    return setStatus("REJECTED: image does not match signature");
  }
  if (esp_ota_end(h) != ESP_OK || esp_ota_set_boot_partition(part) != ESP_OK) {
    prog = -1;
    return setStatus("could not activate image");
  }
  snprintf(statusText, sizeof statusText, "installed %s, restarting", name);
  prog = 100;
  delay(1500);
  esp_restart();
}

void task(void*) {
  run();
  prog = -1;
  running = false;
  vTaskDelete(nullptr);
}
}  // namespace

namespace ota {

void begin() {
  const esp_partition_t* p = esp_ota_get_running_partition();
  esp_ota_img_states_t st;
  if (esp_ota_get_state_partition(p, &st) == ESP_OK && st == ESP_OTA_IMG_PENDING_VERIFY)
    setStatus("new version on trial");
  else
    markedValid = true;
}

void healthy() {
  if (markedValid) return;
  esp_ota_mark_app_valid_cancel_rollback();
  markedValid = true;
  setStatus("new version confirmed");
}

void checkNow() { wanted = true; }

void loop(bool online, bool idle, uint32_t epoch) {
  if (running || !online) return;
  bool due = millis() > OTA_FIRST_CHECK_MS && (lastCheckEpoch == 0 || epoch - lastCheckEpoch >= OTA_CHECK_EVERY_S);
  if (!(wanted || (due && idle)) || !markedValid) return;
  wanted = false;
  lastCheckEpoch = epoch ? epoch : 1;
  running = true;
  setStatus("checking");
  xTaskCreatePinnedToCore(task, "ota", 12288, nullptr, 1, nullptr, 0);
}

int progress() { return prog; }
const char* status() { return statusText; }
uint32_t lastCheck() { return lastCheckEpoch; }

}  // namespace ota
