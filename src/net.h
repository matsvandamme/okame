#pragma once
#include <Arduino.h>
#include "storage.h"

// Wi-Fi: joins the strongest saved network; opens the setup Wi-Fi when none is reachable.
namespace net {
void begin();
void loop();
bool online();           // joined a saved network
bool apActive();
const char* apSsid();
const char* apPass();
bool timeValid();
String hostname();
void setHostname(const String& petName);  // "<name>.local" once named
int networks(SavedNetwork* out);          // copy of the saved list
bool addNetwork(const char* ssid, const char* pass);
bool removeNetwork(const char* ssid);
String scanJson();                        // nearby networks for the setup page
// Visitor detection: true once per newly seen device on the local network.
bool takeVisitor();
}  // namespace net
