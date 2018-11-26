#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include "private_wifi.h"
#include "weather_icons.h"
#include "private_weather_api.h"

const char* ssid = WIFI_SSID;
const char* password = WIFI_WPA_KEY;
const char* mqtt_server = "raspberrypi3";



String sWeatherAPI =  WEATHER_API_KEY;
String sWeatherLOC =  WEATHER_API_LOC;
const String sWeatherURL =  "https://api.darksky.net/forecast/";
const String sWeatherFIO =  "api.darksky.net";
const String sWeatherLNG =  "de";
const String WeekDayNames[7] = {"So","Mo", "Di", "Mi", "Do", "Fr", "Sa"}; // {"Sun", "Mon", "Tue", "Wed", "Thr", "Fri", "Sat"};

#define TIME_TO_SLEEP 300
#define uS_TO_S_FACTOR 1000000

// mapping suggestion for ESP32, e.g. LOLIN32, see .../variants/.../pins_arduino.h for your board
// NOTE: there are variants with different pins for SPI ! CHECK SPI PINS OF YOUR BOARD
// BUSY -> 4, RST -> 16, DC -> 17, CS -> SS(5), CLK -> SCK(18), DIN -> MOSI(23), GND -> GND, 3.3V -> 3.3V

#include <GxEPD2_3C.h>
#include <Fonts/FreeMono9pt7b.h>

WiFiClient espClient;
PubSubClient client(espClient);
long lastMsg = 0;
char msg[50];
int value = 0;

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);


//#define ENABLE_GxEPD2_GFX 1
GxEPD2_3C<GxEPD2_750c, GxEPD2_750c::HEIGHT> display(GxEPD2_750c(/*CS=5*/ SS, /*DC=*/ 17, /*RST=*/ 16, /*BUSY=*/ 4));
void DisplayWXicon(int x, int y, String IconName, uint16_t color = GxEPD_BLACK);
void helloWorld();
void printCross();
void printUpdateTime(uint16_t x, uint16_t y);

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
  printUpdateTime(5, display.height() - 5);
  DisplayWXicon(0,0,"rain - day",GxEPD_BLACK);
  display.nextPage();

  display.powerOff();
  Serial.println("setup done");
  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
  esp_deep_sleep_start();
}

void loop() {
  // put your main code here, to run repeatedly:
}



void helloWorld()
{
  display.setRotation(0);
  display.setFullWindow();
  display.firstPage();
  display.fillScreen(GxEPD_WHITE);
}

void printCross()
{
  display.drawLine(display.width() / 2, 0, display.width() / 2, display.height(), GxEPD_BLACK);
  display.drawLine(0, display.height() / 2, display.width(), display.height() / 2, GxEPD_BLACK);

}

void printUpdateTime(uint16_t x, uint16_t y){

  display.setFont(&FreeMono9pt7b);
  display.setTextColor(GxEPD_BLACK);
  display.setCursor(x, y);
  display.print("Update: ");
  display.println(timeClient.getFormattedTime());
}

void DisplayWXicon(int x, int y, String IconName, uint16_t color) {
  if      (IconName == "01d" || IconName == "clear - day") display.drawBitmap(x, y, gImage_01d, 48, 48, color);
  else if (IconName == "01n" || IconName == "clear - night") display.drawBitmap(x, y, gImage_01n, 48, 48, color);
  else if (IconName == "02d" || IconName == "partly - cloudy - day") display.drawBitmap(x, y, gImage_02d, 48, 48, color);
  else if (IconName == "02n" || IconName == "partly - cloudy - night") display.drawBitmap(x, y, gImage_02n, 48, 48, color);
  else if (IconName == "03d" || IconName == "cloudy - day") display.drawBitmap(x, y, gImage_03d, 48, 48, color);
  else if (IconName == "03n" || IconName == "cloudy - night") display.drawBitmap(x, y, gImage_03n, 48, 48, color);
  else if (IconName == "04d") display.drawBitmap(x, y, gImage_04d, 48, 48, color); // dark cloud
  else if (IconName == "04n") display.drawBitmap(x, y, gImage_04n, 48, 48, color);
  else if (IconName == "09d" || IconName == "rain - day") display.drawBitmap(x, y, gImage_09d, 48, 48, color);
  else if (IconName == "09n" || IconName == "rain - night") display.drawBitmap(x, y, gImage_09n, 48, 48, color);
  else if (IconName == "10d") display.drawBitmap(x, y, gImage_10d, 48, 48, color); //rain and sun
  else if (IconName == "10n") display.drawBitmap(x, y, gImage_10n, 48, 48, color);
  else if (IconName == "11d" || IconName == "storm - day") display.drawBitmap(x, y, gImage_11d, 48, 48, color);
  else if (IconName == "11n" || IconName == "storm - night") display.drawBitmap(x, y, gImage_11n, 48, 48, color);
  else if (IconName == "13d" || IconName == "snow - day") display.drawBitmap(x, y, gImage_13d, 48, 48, color);
  else if (IconName == "13n" || IconName == "snow - night") display.drawBitmap(x, y, gImage_13n, 48, 48, color);
  else if (IconName == "50d" || IconName == "50n"  || IconName == "fog") display.drawBitmap(x, y, gImage_50n, 48, 48, color);
  else     display.drawBitmap(x, y, gImage_nodata, 48, 48, color);
}

bool DownloadForecast() {
  String jsonFioString = "";
  FB_GetWeatherJson(&jsonFioString);
  if (!jsonFioString.length()) {
    String sWeatherJSONSource = sWeatherURL + sWeatherAPI + "/" + sWeatherLOC + "?units=si&lang=" + sWeatherLNG;
    if (!bGetJSONString(sWeatherFIO, sWeatherJSONSource, &jsonFioString)) return false;
    if (jsonFioString.length() > 0) FB_SetWeatherJson(jsonFioString);
  } else   Serial.print("  FireBase JSON Loaded " +  (String)(jsonFioString.length()) + " chrs,");
  if (!jsonFioString.length()) {
    Serial.println("JSON EMPTY");
    return false;
  }  else   {
    if (!showWeather_conditionsFIO(jsonFioString )) Serial.println("Failed to get Conditions Data");
  }
  jsonFioString = "";
  return true;
}

bool FB_GetWeatherJson(String* jsonW) {
  String sAux1 = "/Weather/" + sWeatherLOC + "/", sAux2;
  int iLength, iBlockSize = 9999;
  sAux1.replace(".", "'");
  float timeUploaded = Firebase.getFloat(sAux1 + "Time");
  if (Firebase.failed()) {
    Serial.print("getting json_Time failed :");
    Serial.println(Firebase.error());
    return false;
  }
  if ((tNow - timeUploaded) > (iRefreshPeriod * 60)) {
    Serial.print("[getting json too old. Rejected.]");
    return false; //Too old
  }
  delay(10);
  iLength = Firebase.getFloat(sAux1 + "Size");
  if (Firebase.failed()) {
    Serial.print("getting json_length failed :");
    Serial.println(Firebase.error());
    return false;
  }
  delay(10);
  sAux2 = "";
  for (int i = 0; i < (int)(1 + (iLength / iBlockSize)); i++ ) {
    sAux2 = sAux2 + Firebase.getString(sAux1 + "json/" + (String)(i));
    if (Firebase.failed()) {
      Serial.print("setting json failed:");
      Serial.println(Firebase.error());
      return false;
    }
    delay(10);
  }
  sAux2.trim();
  String s1 = "\\" + (String)(char(34));
  String s2 = (String)(char(34));
  sAux2.replace(s1, s2);
  sAux2.replace("\\n", "");
  *jsonW = sAux2;
  /// Get, now play with your toys
  delay(10);
  String sLocality = Firebase.getString(sAux1 + "Locality");
  if ((sLocality.length() > 2) && (sCustomText.length() < 2)) {
    sCustomText = sLocality;
    DynamicJsonBuffer jsonBuffer(256);
    JsonObject& root = jsonBuffer.parseObject(sJsonDev);
    if (root.success()) {
      FBUpdate2rootStr(root, "vars", "CustomText", sCustomText);
    }
  }
  delay(10);
  return true;
}
bool bGetJSONString(String sHost, String sJSONSource, String * jsonString) {
  int httpPort = 443;
  unsigned long timeout ;
  SecureClient.stop();
  Serial.print("  Connecting to " + String(sHost) );
  if (!SecureClient.connect(const_cast<char*>(sHost.c_str()), httpPort)) {
    Serial.println(" **Connection failed**");
    return false;
  }
  SecureClient.print(String("GET ") + sJSONSource + " HTTP/1.1\r\n" + "Host: " + sHost + "\r\n" + "Connection: close\r\n\r\n");
  timeout = millis();
  while (SecureClient.available() == 0) {
    if (millis() - timeout > 10000) {
      Serial.println(">>> Client Connection Timeout...Stopping");
      SecureClient.stop();
      return false;
    }
  }
  Serial.print(" done. Get json,");
  while (SecureClient.available()) {
    *jsonString = SecureClient.readStringUntil('\r');
  }
  Serial.print("done," + String(jsonString->length()) + " bytes long.");
  SecureClient.stop();
  return true;
}
