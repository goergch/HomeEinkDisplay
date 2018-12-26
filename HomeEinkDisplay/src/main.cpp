#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include "private_wifi.h"
#include "weather_icons.h"
#include <HttpClient.h>
#include <ArduinoJson.h>
#include <TimeLib.h>


const char* ssid = WIFI_SSID;
const char* password = WIFI_WPA_KEY;
const char* mqtt_server = "raspberrypi3.fritz.box";

#define ENERGY_VALUES_COUNT 25

typedef struct {
  int hour = 0;
  float value = 0.0;
} EnergyValue;

EnergyValue energyValues[ENERGY_VALUES_COUNT];
// String sWeatherAPI =  WEATHER_API_KEY;
// String sWeatherLOC =  WEATHER_API_LOC;
// const String sWeatherURL =  "https://api.darksky.net/forecast/";
// const String sWeatherFIO =  "api.darksky.net";
// const String sWeatherLNG =  "de";
// const String WeekDayNames[7] = {"So","Mo", "Di", "Mi", "Do", "Fr", "Sa"}; // {"Sun", "Mon", "Tue", "Wed", "Thr", "Fri", "Sat"};

#define TIME_TO_SLEEP 300
#define uS_TO_S_FACTOR 1000000
#define VTGMEASSURES 24
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

#define ANALYZEHOURS 48
long int aHour[ANALYZEHOURS];
int aHumid[ANALYZEHOURS];
double aTempH[ANALYZEHOURS];
double aPrecip[ANALYZEHOURS];
double aPrecipProb[ANALYZEHOURS];
double aCloudCover[ANALYZEHOURS];
String aIcon[ANALYZEHOURS];
long int tSunrise, tSunset;
String sSummaryDay, sSummaryWeek, sSummaryNote, sCustomText;
double dCurrTemp;
int tzOffset = 1 * 3600; //CET


RTC_DATA_ATTR bool bFBModified;
RTC_DATA_ATTR int32_t iBootWOWifi;
RTC_DATA_ATTR long int tFirstBoot;
RTC_DATA_ATTR long int tLastFBUpdate;
RTC_DATA_ATTR long int lSecsOn;
RTC_DATA_ATTR long int lBoots;
RTC_DATA_ATTR int32_t iVtgVal[VTGMEASSURES];
RTC_DATA_ATTR time_t tVtgTime[VTGMEASSURES];


int moisture_1 = -1;
int moisture_2 = -1;
bool moisture_1_received, moisture_2_received;
const String moisture_1_key = "fhem/plants/Pflanze1/moisture";
const String moisture_2_key = "fhem/plants/Pflanze2/moisture";

GxEPD2_3C<GxEPD2_750c, GxEPD2_750c::HEIGHT> display(GxEPD2_750c(/*CS=5*/ SS, /*DC=*/ 17, /*RST=*/ 16, /*BUSY=*/ 4));
void DisplayWXicon(int x, int y, String IconName, uint16_t color = GxEPD_BLACK);
void helloWorld();
void printCross();
void printUpdateTime(uint16_t x, uint16_t y);
void SendToSleep(int mins);
void mqtt_callback(char* topic, byte* payload, unsigned int length);
void finishAndSleep();
void printMoisture(uint16_t x, uint16_t y);
void GetEnergyOverview();
bool parseEnergyOverview(String jsonString);
void PrintEnergyOverview(uint16_t x, uint16_t y);

void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println("setup");
  // delay(10000);
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
  String clientId = "HomeDisplay-";
  clientId += String(random(0xffff), HEX);

  client.setServer(mqtt_server, 1883);
  client.setCallback(mqtt_callback);

  Serial.println("connect to MQTT");
  if (client.connect(clientId.c_str())) {
      Serial.println("connected to MQTT");
      // Once connected, publish an announcement...
      // ... and resubscribe
      client.subscribe(moisture_1_key.c_str());
      client.subscribe(moisture_2_key.c_str());

	    client.loop();

    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
    }
    GetEnergyOverview();

}



void loop() {

  client.loop();

  if(moisture_1_received && moisture_2_received){
    finishAndSleep();
  }
  // put your main code here, to run repeatedly:
}

void finishAndSleep(){
  do{
    helloWorld();
    printCross();
    printUpdateTime(5, display.height() - 5);
    DisplayWXicon(0,0,"rain - day",GxEPD_BLACK);
    printMoisture(100,100);
    PrintEnergyOverview(300,20);
  }while(display.nextPage());

  //
  display.powerOff();
  Serial.println("printed...sleeping now");
  SendToSleep(2);
}

void printMoisture(uint16_t x, uint16_t y){
  display.setFont(&FreeMono9pt7b);
  display.setTextColor(GxEPD_BLACK);
  display.setCursor(x, y);
  display.print("Moisture 1: ");
  display.print(moisture_1);
  display.println("%");

  display.setCursor(x, y + 16);
  display.print("Moisture 2: ");
  display.print(moisture_2);
  display.println("%");
}

void PrintEnergyOverview(uint16_t x, uint16_t y){
  Serial.println("Printing energy overview");
  display.setFont(NULL);
  display.setTextColor(GxEPD_BLACK);
  for(int i = 0; i< 24;i++ ){
      display.setCursor(x, y + i * 10);
      display.print((energyValues[i].hour + 1));
      display.print(": ");
      display.print(energyValues[i].value,1);

      Serial.print((energyValues[i].hour + 1));
      Serial.print(": ");
      Serial.println(energyValues[i].value,1);
  }
}

void mqtt_callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
    String value;
  for (int i = 0; i < length; i++) {
    value += (char)payload[i];
  }
  Serial.println(value);

  if(moisture_1_key == topic){
    moisture_1 = value.toInt();
    moisture_1_received = true;
  }else if(moisture_2_key == topic){
    moisture_2 = value.toInt();
    moisture_2_received = true;
  }

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

void GetEnergyOverview(){
  	HTTPClient httpClient;
    String address = "http://raspberrypi3bp:8086/query";
    String params = "q=SELECT difference(first(\"value\")) AS \"sum_value\" FROM \"telegraf\".\"autogen\".\"mqtt_consumer\" WHERE \"topic\"='fhem/energy/total/kwh/state' AND time < now() and time > now() - 1d GROUP BY time(1h)";
    params.replace(" ", "%20");
    httpClient.begin(address + "?" + params);

    httpClient.setAuthorization("chronograf", "chronograf");
    Serial.println("Get Energy Overview");
    int httpCode = httpClient.GET();

    if (httpCode > 0) {
      if(httpCode == 200){
        parseEnergyOverview(httpClient.getString());
      }else{
        Serial.println(httpClient.getString());
      }
    }
    else {
      Serial.printf("Request failed: %s\n", httpClient.errorToString(httpCode).c_str());
    }

    httpClient.end();

}

bool parseEnergyOverview(String jsonString){
    DynamicJsonBuffer jsonBuffer(1024);
    JsonObject& root = jsonBuffer.parseObject(jsonString);
    if (!root.success()) {
      Serial.println(F("jsonBuffer.parseObject() failed"));
      return false;
    }
    JsonVariant values = root["results"][0]["series"][0]["values"];

    int count = values.size();
    for(int i= 0; i < count; i++){
      if( i >= ENERGY_VALUES_COUNT) {
        Serial.println("Too much values in energy summary");
      }
      String timeString = values[i][0];
      String valueString = values[i][1];

      int Year, Month, Day, Hour, Minute, Second ;
      sscanf(timeString.c_str(), "%d-%d-%dT%d:%d:%dZ", &Year, &Month, &Day, &Hour, &Minute, &Second);

      energyValues[i].hour = Hour;
      energyValues[i].value = valueString.toFloat();


    }

    return true;
}

// bool showWeather_conditionsFIO(String jsonFioString ) {
//   String sAux;
//   time_t tLocal;
//   Serial.print("  Creating object," );
//   DynamicJsonBuffer jsonBuffer(1024);
//   Serial.print("Parsing,");
//   JsonObject& root = jsonBuffer.parseObject(jsonFioString);
//   Serial.print("done.");
//   if (!root.success()) {
//     Serial.println(F("jsonBuffer.parseObject() failed"));
//     SendToSleep(5);
//     return false;
//   }
//   Serial.print("->Vars," );
//   dCurrTemp = root["currently"]["temperature"];
//   tzOffset = root["offset"];
//   tzOffset *= 3600;
//   tSunrise = root["daily"]["data"][0]["sunriseTime"];
//   tSunrise += tzOffset;
//   tSunset = root["daily"]["data"][0]["sunsetTime"];
//   tSunset += tzOffset;
//   for (int i = 0; i < ANALYZEHOURS; i++) {
//     tLocal = root["hourly"]["data"][i]["time"];
//     aHour[i] = tLocal + tzOffset;
//     // if ((hour(aHour[i]) < hour(tSunrise)) || (hour(aHour[i]) > hour(tSunset))) {
//     //   sAux = "-night";
//     // } else {
//     //   sAux = "-day";
//     // }
//     String sTemp = root["hourly"]["data"][i]["icon"];
//     if (sTemp.length() < 8) sTemp = sTemp + sAux;
//     aIcon[i] = sTemp;
//     aTempH[i] = root["hourly"]["data"][i]["temperature"];
//     aHumid[i] = root["hourly"]["data"][i]["humidity"];
//     aPrecip[i] = root["hourly"]["data"][i]["precipIntensity"];
//     aPrecipProb[i] = root["hourly"]["data"][i]["precipProbability"];
//     aCloudCover[i] = root["hourly"]["data"][i]["cloudCover"];
//     String stmp1 = root["hourly"]["summary"];
//     sSummaryDay = stmp1;
//     String stmp2 = root["daily"]["summary"];
//     sSummaryWeek = stmp2;
//   }
//   Serial.println("Done." );
//   return true;
// }
//

//
void SendToSleep(int mins) {
  Serial.print("  [-> To sleep... " + (String)mins + " mins");
  if (WiFi.status() == WL_CONNECTED)  WiFi.disconnect();
  WiFi.mode(WIFI_OFF);Serial.println("].");
  delay(500);
   ESP.deepSleep(mins * 60000000);
}
