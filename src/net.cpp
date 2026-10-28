#include "net.h"
#include <DNSServer.h>
#include <ESPmDNS.h>
#include <WiFi.h>
#include <esp_sntp.h>
#include <lwip/etharp.h>
#include <lwip/tcpip.h>
#include <map>
#include "config.h"

namespace {
// Guards nets/netCount/host and the shared scan results: web handlers run on another task.
SemaphoreHandle_t mu = xSemaphoreCreateRecursiveMutex();
struct NL {
  NL() { xSemaphoreTakeRecursive(mu, portMAX_DELAY); }
  ~NL() { xSemaphoreGiveRecursive(mu); }
};
volatile bool mdnsDirty = false;
bool wasUp = false;
SavedNetwork nets[MAX_NETWORKS];
int netCount = 0;
DNSServer dns;
bool ap = false, staUp = false;
char ssidAp[16], passAp[9];
String host = "okame";
uint32_t lostSince = 0, nextConnect = 0;
int tryIdx = 0;

// Visitor scan state.
std::map<uint64_t, uint32_t> seen;  // MAC -> last seen (millis / 1000)
uint32_t nextScan = 0, scanHost = 1, sweeps = 0;
bool sweeping = true;
bool visitorFlag = false;

void startAp() {
  if (ap) return;
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(ssidAp, passAp);
  dns.start(53, "*", WiFi.softAPIP());
  ap = true;
  log_i("setup Wi-Fi %s up", ssidAp);
}

void stopAp() {
  if (!ap) return;
  dns.stop();
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_STA);
  ap = false;
}

// Try saved networks one at a time, strongest first after a scan.
void connectNext() {
  char ssid[33], pass[65];
  {
  NL l;
  if (!netCount) return;
  int n = WiFi.scanComplete();
  if (n == WIFI_SCAN_FAILED) {
    WiFi.scanNetworks(true);
    nextConnect = millis() + 1000;
    return;
  }
  if (n == WIFI_SCAN_RUNNING) {
    nextConnect = millis() + 1000;
    return;
  }
  int best = -1, bestRssi = -1000;
  for (int i = 0; i < n; i++)
    for (int k = 0; k < netCount; k++)
      if (WiFi.SSID(i) == nets[k].ssid && WiFi.RSSI(i) > bestRssi) {
        best = k;
        bestRssi = WiFi.RSSI(i);
      }
  WiFi.scanDelete();
  if (best < 0) best = tryIdx++ % netCount;  // hidden networks: just try in turn
  strlcpy(ssid, nets[best].ssid, sizeof ssid);
  strlcpy(pass, nets[best].pass, sizeof pass);
  }
  log_i("joining %s", ssid);
  WiFi.begin(ssid, pass);
}

void startMdns() {
  String h;
  {
    NL l;
    h = host;
  }
  MDNS.end();
  if (MDNS.begin(h.c_str())) MDNS.addService("http", "tcp", 80);
}

uint64_t macKey(const eth_addr* e) {
  uint64_t k = 0;
  for (int i = 0; i < 6; i++) k = (k << 8) | e->addr[i];
  return k;
}

// Probe a few addresses on the /24 with ARP and record whoever answered.
void scanStep() {
  IPAddress me = WiFi.localIP();
  uint32_t nowS = millis() / 1000;
  LOCK_TCPIP_CORE();
  netif* nif = netif_default;
  for (int i = 0; i < ARP_TABLE_SIZE; i++) {
    ip4_addr_t* ip;
    netif* n;
    eth_addr* eth;
    if (etharp_get_entry(i, &ip, &n, &eth)) {
      uint64_t k = macKey(eth);
      auto it = seen.find(k);
      bool fresh = it == seen.end() || nowS - it->second > 24 * 3600;
      if (fresh && sweeps >= 2 && ip4_addr_get_u32(ip) != ip4_addr_get_u32(netif_ip4_gw(nif))) {
        visitorFlag = true;
        log_i("visitor %s", ip4addr_ntoa(ip));
      }
      seen[k] = nowS;
    }
  }
  for (int i = 0; i < 6 && nif; i++) {
    if (scanHost == me[3]) scanHost++;
    if (scanHost > 254) {
      scanHost = 1;
      sweeps++;
      sweeping = false;  // pause until the next sweep
      for (auto it = seen.begin(); it != seen.end();)
        it = (nowS - it->second > 3 * 24 * 3600) ? seen.erase(it) : std::next(it);
      break;
    }
    ip4_addr_t target;
    IP4_ADDR(&target, me[0], me[1], me[2], scanHost++);
    etharp_request(nif, &target);
  }
  UNLOCK_TCPIP_CORE();
}
}  // namespace

namespace net {

void begin() {
  uint8_t mac[6];
  WiFi.macAddress(mac);
  snprintf(ssidAp, sizeof ssidAp, "OKAME-%03X", ((mac[4] << 8) | mac[5]) & 0xFFF);
  strlcpy(passAp, storage::apPassword().c_str(), sizeof passAp);
  netCount = storage::loadNetworks(nets, MAX_NETWORKS);
  WiFi.persistent(false);
  WiFi.setAutoReconnect(true);
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);  // USB powered; modem sleep makes .local lookups and the page flaky
  WiFi.setHostname(host.c_str());
  configTzTime(OKAME_TZ, "pool.ntp.org", "time.google.com");
  lostSince = millis();
  WiFi.scanNetworks(true);
}

void loop() {
  uint32_t now = millis();
  bool up = WiFi.status() == WL_CONNECTED;
  if (up != staUp) {
    staUp = up;
    if (up) wasUp = true;
    if (up) {
      log_i("online %s", WiFi.localIP().toString().c_str());
      stopAp();
      startMdns();
      nextScan = now + 5000;
    } else {
      lostSince = now;
    }
  }
  if (!up) {
    bool grace = wasUp && now - lostSince < 20000;        // let the stack's own reconnect work first
    bool phoneOnAp = ap && WiFi.softAPgetStationNum() > 0;  // don't hop channels under a phone mid-setup
    if (!grace && !phoneOnAp && (int32_t)(now - nextConnect) >= 0) {
      nextConnect = now + (ap ? 30000 : 15000);
      connectNext();
    }
    int saved;
    {
      NL l;
      saved = netCount;
    }
    if (!ap && now - lostSince > (saved ? 5 * 60 * 1000u : AP_AFTER_MS)) startAp();
  }
  if (mdnsDirty && up) {
    mdnsDirty = false;
    startMdns();
  }
  if (ap) dns.processNextRequest();
  if (up && (int32_t)(now - nextScan) >= 0) {
    if (!sweeping) {
      sweeping = true;  // a sweep takes ~20 s, then rests 5 minutes
    }
    scanStep();
    nextScan = now + (sweeping ? 400 : 5 * 60 * 1000);
  }
}

bool online() { return staUp; }
bool apActive() { return ap; }
const char* apSsid() { return ssidAp; }
const char* apPass() { return passAp; }
bool timeValid() { return time(nullptr) > 1700000000; }
String hostname() {
  NL l;
  return host;
}

void setHostname(const String& petName) {
  String h;
  for (char c : petName) {
    if (isalnum((unsigned char)c)) h += (char)tolower(c);
    else if ((c == ' ' || c == '-') && h.length() && h[h.length() - 1] != '-') h += '-';
  }
  while (h.endsWith("-")) h.remove(h.length() - 1);
  if (h.isEmpty()) h = "okame";
  NL l;
  if (h == host) return;
  host = h;
  mdnsDirty = true;  // restarted from the main loop, not from a web handler
}

int networks(SavedNetwork* out) {
  NL l;
  memcpy(out, nets, sizeof(SavedNetwork) * netCount);
  return netCount;
}

bool addNetwork(const char* ssid, const char* pass) {
  NL l;
  if (!*ssid || strlen(ssid) > 32 || strlen(pass) > 64) return false;
  int idx = -1;
  for (int i = 0; i < netCount; i++)
    if (!strcmp(nets[i].ssid, ssid)) idx = i;
  if (idx < 0) {
    if (netCount >= MAX_NETWORKS) return false;
    idx = netCount++;
  }
  strlcpy(nets[idx].ssid, ssid, sizeof nets[idx].ssid);
  strlcpy(nets[idx].pass, pass, sizeof nets[idx].pass);
  storage::saveNetworks(nets, netCount);
  nextConnect = 0;
  return true;
}

bool removeNetwork(const char* ssid) {
  NL l;
  for (int i = 0; i < netCount; i++)
    if (!strcmp(nets[i].ssid, ssid)) {
      nets[i] = nets[--netCount];
      storage::saveNetworks(nets, netCount);
      return true;
    }
  return false;
}

String scanJson() {
  NL l;
  int n = WiFi.scanComplete();
  if (n < 0) {
    if (n == WIFI_SCAN_FAILED) WiFi.scanNetworks(true);
    return "null";
  }
  String out = "[";
  bool first = true;
  for (int i = 0; i < n; i++) {
    String e;
    for (uint8_t c : WiFi.SSID(i)) {
      if (c == '"' || c == '\\') e += '\\', e += (char)c;
      else if (c < 0x20) e += ' ';
      else e += (char)c;
    }
    if (e.isEmpty()) continue;  // hidden networks
    if (!first) out += ',';
    first = false;
    out += "{\"ssid\":\"" + e + "\",\"rssi\":" + String(WiFi.RSSI(i)) + "}";
  }
  out += "]";
  WiFi.scanDelete();
  WiFi.scanNetworks(true);
  return out;
}

bool takeVisitor() {
  bool v = visitorFlag;
  visitorFlag = false;
  return v;
}

}  // namespace net
