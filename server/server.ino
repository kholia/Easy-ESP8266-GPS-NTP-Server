#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <SoftwareSerial.h>
#include "DateTime.h"
#include "GPS.h"
#include "NTPClock.h"
#include "ClockPID.h"
#include "NTPServer.h"
#include "settings.h"
#include "platform-clock.h"
#include <ESP8266WebServer.h>

#define BAUD_SERIAL 9600
#define BAUD_LOGGER 115200
#define RXBUFFERSIZE 1024
#define PPSPIN D6

ESP8266WebServer webserver(80);
SoftwareSerial logger(3, 1);
GPSDateTime gps(&Serial);
NTPClock localClock;
WiFiUDP ntpSocket, logSocket;
IPAddress logDestination;
NTPServer server(&ntpSocket, &localClock);

uint32_t lastPPS = 0;
uint8_t lastLed = 0;

//ISR for PPS interrupt
void IRAM_ATTR handleInterrupt() {
  lastPPS = COUNTERFUNC();
  lastLed = !lastLed;
  digitalWrite(LED_BUILTIN, lastLed);
}

bool gpsLocked = false;

void handleRoot()
{
  if (gpsLocked)
    webserver.send(200, "text/html", "<h1>You are connected. GPS is locked!</h1>");
  else
    webserver.send(200, "text/html", "<h1>You are connected. GPS is NOT locked :-(</h1>");
}

uint32_t compileTime;
void setup() {
  DateTime compile = DateTime(__DATE__, __TIME__);

  Serial.begin(BAUD_SERIAL);
  Serial.setRxBufferSize(RXBUFFERSIZE);
  // Move hardware serial to RX:GPIO13 TX:GPIO15
  Serial.swap();
  // https://nmeachecksum.eqth.net/
  Serial.write("$PUBX,40,ZDA,0,1,0,0*45\r\n"); // Enable ZDA NMEA messages
  delay(1000);
  Serial.write("$PUBX,40,ZDA,0,1,0,0*45\r\n"); // Enable ZDA NMEA messages
  delay(1000);
  Serial.write("$PUBX,40,ZDA,0,1,0,0*45\r\n"); // Enable ZDA NMEA messages
  delay(1000);
  Serial.write("$PUBX,40,ZDA,0,1,0,0*45\r\n"); // Enable ZDA NMEA messages
  delay(1000);
  Serial.write("$PUBX,40,ZDA,0,1,0,0*45\r\n"); // Enable ZDA NMEA messages
  delay(1000);
  // use SoftwareSerial on regular RX(3)/TX(1) for logging
  logger.begin(BAUD_LOGGER);
  logger.enableIntTx(false);
  logger.println("\n\nUsing SoftwareSerial for logging");

  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);
  pinMode(PPSPIN, INPUT);
  digitalWrite(PPSPIN, LOW);
  attachInterrupt(digitalPinToInterrupt(PPSPIN), handleInterrupt, RISING);

  compileTime = compile.ntptime();
  localClock.setTime(COUNTERFUNC(), compileTime);
  // allow for compile timezone to be 12 hours ahead
  compileTime -= 12 * 60 * 60;

  logger.print("Creating SSID: ");
  logger.println(ssid);
  WiFi.mode(WIFI_AP);
  WiFi.hostname(ssid);
  WiFi.softAP(ssid, ssidPass);
  IPAddress myIP = WiFi.softAPIP();
  WiFi.setSleepMode(WIFI_NONE_SLEEP, 0); // no sleeping for minimum latency

  logger.print("WiFi AP created, IP address: ");
  logger.println(myIP);

  logDestination.fromString(logDestinationIP);
  ntpSocket.begin(123);
  logSocket.begin(1234);
  // webserver.on("/", handleRoot);
  // webserver.begin();

  while (Serial.available()) { // throw away all the text received while starting up
    Serial.read();
  }
}

uint8_t median(int64_t one, int64_t two, int64_t three) {
  if (one > two) {
    if (one > three) {
      if (two > three) {
        // 1 2 3
        return 2 - 1;
      } else {
        // 1 3 2
        return 3 - 1;
      }
    } else {
      // 3 1 2
      return 1 - 1;
    }
  } else {
    if (two > three) {
      if (one > three) {
        // 2 1 3
        return 1 - 1;
      } else {
        // 2 3 1
        return 3 - 1;
      }
    } else {
      // 3 2 1
      return 2 - 1;
    }
  }
}

#define WAIT_COUNT 3
uint8_t settime = 0;
uint8_t wait = WAIT_COUNT - 1;
struct {
  int64_t offset;
  uint32_t pps;
  uint32_t gpstime;
} samples[WAIT_COUNT];

void updateTime(uint32_t gpstime) {
  if (lastPPS == 0) {
    return;
  }

  if (settime) {
    int64_t offset = localClock.getOffset(lastPPS, gpstime, 0);
    samples[wait].offset = offset;
    samples[wait].pps = lastPPS;
    samples[wait].gpstime = gpstime;
    if (ClockPID.full() && wait) {
      wait--;
    } else {
      uint8_t median_index = wait;
      if (wait == 0) {
        median_index = median(samples[0].offset, samples[1].offset, samples[2].offset);
      }
      ClockPID.add_sample(samples[median_index].pps, samples[median_index].gpstime, samples[median_index].offset);
      localClock.setRefTime(samples[median_index].gpstime);
      localClock.setPpb(ClockPID.out() * 1000000000.0);
      wait = WAIT_COUNT - 1; // (2+1)*16=48s, 80MHz wraps at 53s

      double offsetHuman = samples[median_index].offset / (double)4294967296.0;
      logSocket.beginPacket(logDestination, 51413);
      logSocket.print(samples[median_index].pps);
      logSocket.print(" ");
      logSocket.print(offsetHuman, 9);
      logSocket.print(" ");
      logSocket.print(ClockPID.d(), 9);
      logSocket.print(" ");
      logSocket.print(ClockPID.d_chi(), 9);
      logSocket.print(" ");
      logSocket.print(localClock.getPpb());
      logSocket.print(" ");
      logSocket.println(samples[median_index].gpstime);
      logSocket.endPacket();
    }
  } else {
    localClock.setTime(lastPPS, gpstime);
    ClockPID.add_sample(lastPPS, gpstime, 0);
    settime = 1;
    logSocket.beginPacket(logDestination, 51413);
    logSocket.print("S "); // clock set message
    logSocket.print(lastPPS);
    logSocket.print(" ");
    logSocket.println(gpstime);
    logSocket.endPacket();
  }
  lastPPS = 0;
}

uint32_t lastUpdate = 0;
void loop() {
  server.poll();
  uint32_t cyclesNow = COUNTERFUNC();
  if ((cyclesNow - lastUpdate) >= COUNTSPERSECOND) {
    uint32_t s, s_fb;
    // update the local clock's cycle count
    localClock.getTime(cyclesNow, &s, &s_fb);
    lastUpdate = cyclesNow;
  }
  server.poll();
  if (Serial.available()) {
    if (gps.decode()) {
      uint32_t gpstime = gps.GPSnow().ntptime();
      if (gpstime < compileTime) {
        logSocket.beginPacket(logDestination, 51413);
        logSocket.print("B "); // gps clock bad message (for example, on startup before GPS almanac)
        logSocket.println(gpstime);
        logSocket.endPacket();
      } else {
        updateTime(gpstime);
        logSocket.beginPacket(logDestination, 51413);
        logSocket.print("GOOD TIME FROM GPS! ");
        logSocket.println(gpstime);
        logSocket.endPacket();
        gpsLocked = true;
      }
    } /* else {
      uint32_t gpstime = gps.GPSnow().ntptime();
      logSocket.beginPacket(logDestination, 51413);
      logSocket.print("[DECODED from GPS] ");
      logSocket.println(gpstime);
      logSocket.endPacket();
    } */
  }
  server.poll();
}
