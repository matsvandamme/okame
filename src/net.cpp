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
  if (!netCount) return;
  int n = WiFi.scanComplete();
  if (n == WIFI_SCAN_FAILED) {
    WiFi.scanNetworks(true);
    return;
  }
  if (n == WIFI_SCAN_RUNNING) return;
  int best = -1, bestRssi = -1000;
  for (int i = 0; i < n; i++)
    for (int k = 0; k < netCount; k++)
      if (WiFi.SSID(i) == nets[k].ssid && WiFi.RSSI(i) > bestRssi) {
        best = k;
        bestRssi = WiFi.RSSI(i);
      }
  WiFi.scanDelete();
  if (best < 0) best = tryIdx++ % netCount;  // hidden networks: just try in turn
  log_i("joining %s", nets[best].ssid);
  WiFi.begin(nets[best].ssid, nets[best].pass);
}

void startMdns() {
  MDNS.end();
  if (MDNS.begin(host.c_str())) MDNS.addService("http", "tcp", 80);
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
    if (now >= nextConnect && !ap) {
      nextConnect = now + 15000;
      connectNext();
    } else if (ap && now >= nextConnect) {
      // Keep trying saved networks occasionally even while the setup Wi-Fi is up.
      nextConnect = now + 30000;
      connectNext();
    }
    if (!ap && now - lostSince > AP_AFTER_MS) startAp();
  }
  if (ap) dns.processNextRequest();
  if (up && now >= nextScan) {
    nextScan = now + 400;
    scanStep();
  }
}

bool online() { return staUp; }
bool apActive() { return ap; }
const char* apSsid() { return ssidAp; }
const char* apPass() { return passAp; }
bool timeValid() { return time(nullptr) > 1700000000; }
String hostname() { return host; }

void setHostname(const String& petName) {
  String h;
  for (char c : petName) {
    if (isalnum((unsigned char)c)) h += (char)tolower(c);
    else if ((c == ' ' || c == '-') && h.length() && h[h.length() - 1] != '-') h += '-';
  }
  while (h.endsWith("-")) h.remove(h.length() - 1);
  if (h.isEmpty()) h = "okame";
  if (h == host) return;
  host = h;
  if (staUp) startMdns();
}

int networks(SavedNetwork* out) {
  memcpy(out, nets, sizeof(SavedNetwork) * netCount);
  return netCount;
}

bool addNetwork(const char* ssid, const char* pass) {
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
  for (int i = 0; i < netCount; i++)
    if (!strcmp(nets[i].ssid, ssid)) {
      nets[i] = nets[--netCount];
      storage::saveNetworks(nets, netCount);
      return true;
    }
  return false;
}

String scanJson() {
  int n = WiFi.scanComplete();
  if (n < 0) {
    if (n == WIFI_SCAN_FAILED) WiFi.scanNetworks(true);
    return "null";
  }
  String out = "[";
  for (int i = 0; i < n; i++) {
    if (i) out += ',';
    String s = WiFi.SSID(i);
    s.replace("\\", "\\\\");
    s.replace("\"", "\\\"");
    out += "{\"ssid\":\"" + s + "\",\"rssi\":" + String(WiFi.RSSI(i)) + "}";
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
