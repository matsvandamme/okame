#pragma once
#include <Arduino.h>

// Link to the Okame relay (Telegram bot). A background task long-polls the relay; the main loop applies the
// commands (care actions, notes, pictures, tunes, pairing codes) and reports results on the next poll.
namespace cloud {
void begin();
void loop();                 // main loop: apply received commands
bool connected();            // reached the relay in the last minute
String status();             // one-line diagnostics for the serial console
void setKey(const String& hex);
String lastMessageJson();    // {"from","text","translation","at"} or "null", for the phone page
// Tell the relay something happened here (a JSON object with "type"), e.g. {"type":"love"}. Sent on the next poll;
// kept (up to 16) while offline.
void event(const String& json);
String lastHugJson();
String weatherJson();        // {"kind","temp","place"} from the relay, or "null"        // {"from","at"} of the last hug from Telegram, or "null"
}  // namespace cloud
