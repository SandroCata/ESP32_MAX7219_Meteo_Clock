#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <time.h>
#include <MD_Parola.h>
#include <SPI.h>
#include "Font_Data.h"

/*
PIN PHYSICAL LINKING (ESP32 -> MAX7219):

VIN -> VCC
GND -> GND
D19 -> DIN
D5 -> CS
D18 -> CLK

*/

#define HARDWARE_TYPE MD_MAX72XX::FC16_HW
#define MAX_DEVICES 4

#define CLK_PIN   18 // or SCK
#define DATA_PIN  19 // or MOSI
#define CS_PIN    5  // or SS

#define SPEED_TIME  75
#define PAUSE_TIME  0
#define MAX_MESG  20

#define NTP0 "0.ch.pool.ntp.org"
#define NTP1 "1.ch.pool.ntp.org"

//Found this reference on setting TZ: http://www.gnu.org/software/libc/manual/html_node/TZ-Variable.html

#define TZ "CET-1CEST,M3.5.0/2,M10.5.0/3" //Italian time (change according to your country)


// Display configuration
MD_Parola P = MD_Parola(HARDWARE_TYPE, DATA_PIN, CLK_PIN, CS_PIN, MAX_DEVICES);

// WiFi credentials
char* ssid = "YOUR_WIFI_SSID";
char* password = "YOUR_WIFI_PASSW";

//Api key to be taken anywhere (I used OpenWeatherMap, created an account freely and used my api key)
const char* weatherApiKey = "YOUR_API_KEY";
const char* weatherApiUrl = "http://api.openweathermap.org/data/2.5/weather?q=Rome,IT&appid=YOUR_API_KEY&units=metric";

// Global variables
int brightness = 1; // LED light intesity (from 0 to 15)
uint16_t  h, m, s;
char szTime[9];    // mm:ss\0
char weatherDescription[21] = "";
float temperature = 0.0; // Temperature in Celsius degrees
unsigned long lastWeatherUpdate = 0;
int weatherIcon = 0; // Set as Clear as default
bool showTemperature = false; // Alternate betwwen meteo icon and temperature
bool toggleWeatherIcon = false; // Switch between main and alternative icon
bool toggleTemperature = false; // Switch between main and alternative icon


//get the current local time (according to what is passed in psz)
void getTime(char *psz, bool f = true) {
    time_t now = time(nullptr);
    struct tm* p_tm = localtime(&now);
    h = p_tm->tm_hour;
    m = p_tm->tm_min;
    sprintf(psz, "%02d%c%02d", h, (f ? ':' : ' '), m);
}

void getTimentp()
{
  //Get the local time accordingly to local time zone (I used Italian but if you want to change it just modify TZ, NTP0 and NTP1 according to you country)
  configTzTime(TZ, NTP0, NTP1, NULL);
  while(!time(nullptr)){
        delay(100);
  }
}

void fetchWeather() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(weatherApiUrl);
    int httpResponseCode = http.GET();
    if (httpResponseCode == 200) {
      String payload = http.getString();
      StaticJsonDocument<1024> doc;
      deserializeJson(doc, payload);

      String description = doc["weather"][0]["main"].as<String>();
      description.toCharArray(weatherDescription, 21);
      temperature = doc["main"]["temp"].as<float>();

      //Set the meteo char code 
      int weatherChar = 0; // Default on Clear
      if (description.equalsIgnoreCase("Clear")) {
        weatherChar = 0; // Clear
      } else if (description.equalsIgnoreCase("Rain") || description.equalsIgnoreCase("Drizzle")) {
        weatherChar = 1; // Rain
      } else if (description.equalsIgnoreCase("Clouds")) {
        weatherChar = 2; // Clouds
      } else if (description.equalsIgnoreCase("Snow")) {
        weatherChar = 3; // Snow
      } else if (description.equalsIgnoreCase("Thunderstorm")) {
        weatherChar = 4; // Thunderstorm
      } else if (description.equalsIgnoreCase("Mist") || description.equalsIgnoreCase("Fog") || description.equalsIgnoreCase("Haze") || description.equalsIgnoreCase("Sand") || 
                description.equalsIgnoreCase("Squall") || description.equalsIgnoreCase("Ash") || description.equalsIgnoreCase("Tornado") || description.equalsIgnoreCase("Dust") || 
                description.equalsIgnoreCase("Smoke")) {
        weatherChar = 5; // Atmosphere (mostly Mist or Fog)
      }
      //record in weatherIcon the icon to be used as index char in "meteo" custom font
      weatherIcon = weatherChar;
    }
    http.end();
  }
}


void setup(void) {
  Serial.begin(115200);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
      delay(100);
  }
  WiFi.mode(WIFI_STA);
  getTimentp();

  P.begin(3);
  
  P.setZone(0, 0, 0);
  P.setZone(1, 1, 3);

  P.setFont(0, meteo);
  P.setFont(1, timeFont);

  P.setIntensity(brightness); 

  P.displayZoneText(1, szTime, PA_LEFT, SPEED_TIME, PAUSE_TIME, PA_PRINT, PA_NO_EFFECT);

  fetchWeather();
  lastWeatherUpdate = millis();
  getTime(szTime);
}

void loop(void) {
  static uint32_t lastTime = 0;
  static uint32_t lastToggleTime = 0;
  static uint32_t lastIconToggleTime = 0;
  static uint32_t lastTempToggleTime = 0;
  static bool flasher = false;

  P.displayAnimate();

  //Time update every second
  if (millis() - lastTime >= 1000) {
    lastTime = millis();
    getTime(szTime, flasher);
    flasher = !flasher;
    P.displayReset(0);
    P.displayReset(1);
  }

  //Meteo and temperature update every 15 min
  if (millis() - lastWeatherUpdate >= 900000) {
    fetchWeather();
    lastWeatherUpdate = millis();
    P.displayReset(0);
  }

  // Switch between meteo icon and temperature every 5 seconds
  if (millis() - lastToggleTime >= 5000) {
    lastToggleTime = millis();
    showTemperature = !showTemperature;
    P.displayReset(0);
  }

  //Update variable for meteo and temperature animation
  if (!showTemperature) {
        if (millis() - lastIconToggleTime >= 1000) {
            lastIconToggleTime = millis();
            toggleWeatherIcon = !toggleWeatherIcon;
        }
  }else {
    if (millis() - lastTempToggleTime >= 1000) {
            lastTempToggleTime = millis();
            toggleTemperature = !toggleTemperature;
        }
  }

  // Show meteo icon or temperature
  if (showTemperature) {
    if (toggleTemperature) {
      // Show numeric value of temperature
      char tempStr[6];
      snprintf(tempStr, sizeof(tempStr), "%.1fC", temperature);
      P.displayZoneText(0, tempStr, PA_RIGHT, SPEED_TIME, 0, PA_PRINT, PA_NO_EFFECT);
    } else {
      // Show char at index 7 of meteo font
      char tempIconChar[2] = {(char)7, '\0'};
      P.displayZoneText(0, tempIconChar, PA_CENTER, SPEED_TIME, 0, PA_PRINT, PA_NO_EFFECT);
    }
  } else {
        int iconToDisplay = toggleWeatherIcon ? weatherIcon : (weatherIcon + 21);
        char iconChar[2] = {(char)iconToDisplay, '\0'};
        P.displayZoneText(0, iconChar, PA_CENTER, SPEED_TIME, 0, PA_PRINT, PA_NO_EFFECT);
    }

}
