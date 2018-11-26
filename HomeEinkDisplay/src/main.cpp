#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include "private_wifi.h"

const char* ssid = WIFI_SSID;
const char* password = WIFI_WPA_KEY;
const char* mqtt_server = "raspberrypi3";
#define TIME_TO_SLEEP 300
#define uS_TO_S_FACTOR 1000000

// mapping suggestion for ESP32, e.g. LOLIN32, see .../variants/.../pins_arduino.h for your board
// NOTE: there are variants with different pins for SPI ! CHECK SPI PINS OF YOUR BOARD
// BUSY -> 4, RST -> 16, DC -> 17, CS -> SS(5), CLK -> SCK(18), DIN -> MOSI(23), GND -> GND, 3.3V -> 3.3V

#include <GxEPD2_3C.h>
#include <Fonts/FreeMonoBold9pt7b.h>

WiFiClient espClient;
PubSubClient client(espClient);
long lastMsg = 0;
char msg[50];
int value = 0;

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);


//#define ENABLE_GxEPD2_GFX 1
GxEPD2_3C<GxEPD2_750c, GxEPD2_750c::HEIGHT> display(GxEPD2_750c(/*CS=5*/ SS, /*DC=*/ 17, /*RST=*/ 16, /*BUSY=*/ 4));

void helloWorld()
{
  //Serial.println("helloWorld");
  display.setRotation(0);
  display.setFont(&FreeMonoBold9pt7b);
  display.setTextColor(GxEPD_BLACK);
  uint16_t x = 10;
  uint16_t y = 10;
  display.setFullWindow();
  display.firstPage();
  // do
  {
    display.fillScreen(GxEPD_WHITE);
    display.setCursor(x, y);
    display.println("Hello World!");
  }
  // while ();
  //Serial.println("helloWorld done");
}

void printCross()
{
  display.drawLine(display.width() / 2, 0, display.width() / 2, display.height(), GxEPD_BLACK);
  display.drawLine(0, display.height() / 2, display.width(), display.height() / 2, GxEPD_BLACK);

}

void printUpdateTime(){
  uint16_t x = 10;
  uint16_t y = display.height() - 10;

  display.setFont(&FreeMonoBold9pt7b);
  display.setTextColor(GxEPD_BLACK);
  display.setCursor(x, y);
  display.println(timeClient.getFormattedTime());
}

void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println("setup");
  display.init(115200);

  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.print("Getting time ");
  timeClient.begin();
  timeClient.setTimeOffset(3600);
  while(!timeClient.update()) {
    timeClient.forceUpdate();
    Serial.print(".");
  }

  helloWorld();
  printCross();
  printUpdateTime();
  display.nextPage();

  display.powerOff();
  Serial.println("setup done");
  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
  esp_deep_sleep_start();
  // put your setup code here, to run once:
}

void loop() {
  // put your main code here, to run repeatedly:
}
