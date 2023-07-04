#define SRCREV "NTP.module.v101b"

#include <NTPClient.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>

// ATTENTION: https://github.com/mikalhart/TinyGPSPlus/issues/30
// GPS Time is 3 seconds fast on some NEO-6M GPS modules - this
// problem corrects itself after 12.5 minutes!

const char *ssid     = "server";
const char *password = "password";

WiFiUDP ntpUDP;
IPAddress ip(192, 168, 4, 1);
// const long utcOffsetInSeconds = (20 * 3600); // Brad
const long utcOffsetInSeconds = 19800; // Dhiru (IST -> 5.5 * 3600)
NTPClient timeClient(ntpUDP, ip, utcOffsetInSeconds);
int wificnt = 0;

void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.println("\n\n\n\n");
  Serial.println(SRCREV);
  Serial.println(SRCREV);
  Serial.println(SRCREV);
  Serial.println(SRCREV);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay (1000);
    wificnt = 0;
    if (wificnt++ > 12) { // 12 seconds to connect, then restart()
      Serial.printf("WiFi %s not found, ESP.restart();\n", ssid);
      delay(1000);
      ESP.restart();
    }
    Serial.print(".");
  }
  Serial.printf("\nWiFiCNT = %d ssid = %s local IP = ", wificnt, ssid);
  Serial.print(WiFi.localIP()); Serial.print("\n");

  timeClient.begin();
  delay(3000);

  Serial.printf("timeClient.forceUpdate(); returns %d\n", timeClient.forceUpdate());
}

int oldseconds, loops;
bool ret;
long OldGlobalEpoch;

void loop() {

  OldGlobalEpoch = timeClient.getEpochTime();

  while (OldGlobalEpoch == timeClient.getEpochTime()) {  // keep sending updates
    timeClient.forceUpdate();                            // until we click into the next second
  }

  // Serial.println(timeClient.getEpochTime());
  Serial.println(timeClient.getFormattedTime());

  delay(2000); // sleep
}
