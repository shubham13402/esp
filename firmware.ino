#ifdef ESP32
#include <WiFi.h>
#include <HTTPClient.h>
#else
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#endif
#include <time.h>
#include <SD.h>
#include <SPI.h>
#include "RTClib.h"
#include <HTTPUpdate.h>
#include <WiFiClientSecure.h>
#include "cert.h"
#include <WebServer.h>
#include <EEPROM.h>

HTTPClient http;

int TGSPin     = 34;
int BatteryPin = 33;

//Variables
int i = 0;
int statusCode;

//const char* wifi_ssid = "ESP1";
//const char* wifi_passphrase = "Sumit@1975";

const char* soft_ap_ssid = "Connectify";
const char* soft_ap_passphrase = "";

const char* ssid = "SKYAE43F";
const char* passphrase = "123456789";

String st;
String content;

//Function Declarations
bool   testWifi(void);
void   launchWeb(void);
void   createWebServer();
void   setupAP(void);
String esid;
String epass = "";

//Establishing Local server at port 80 whenever required
WebServer server(80);

String FirmwareVer = { "1.1" };
#define URL_fw_Version "https://raw.githubusercontent.com/shubham13402/esp/bin_version.txt"
#define URL_fw_Bin "https://raw.githubusercontent.com/shubham13402/esp/fw.bin"

//#define URL_fw_Version "http://cade-make.000webhostapp.com/version.txt"
//#define URL_fw_Bin "http://cade-make.000webhostapp.com/firmware.bin"

// global variables
unsigned long counter = millis();
String        bTemp[100], bHum[100], bGas[100], bBattery[100];
int           indx = 0;
unsigned long previousMillis   = 0; // will store last time LED was updated
unsigned long previousMillis_2 = 0;
const long    interval = 60000;
const long    mini_interval = 1000;
//-RTC-
int yr = 0;
int mt = 0;
int dy = 0;
int hr = 0;
int mi = 0;
int se = 0;

// NTP server to request time
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 0;
const int   daylightOffset_sec = 0;

//predeclarations
void          connect_wifi();
void          firmwareUpdate();
int           FirmwareVersionCheck();

struct Button { // You need to stop variables being incorrectly accessed during an interrupt
  const uint8_t PIN;
  uint32_t      numberKeyPresses;
  bool          pressed;
};

volatile Button button_boot = {0, 0, false }; // *****
/*void IRAM_ATTR isr(void* arg) {
    Button* s = static_cast<Button*>(arg);
    s->numberKeyPresses += 1;
    s->pressed = true;
  }*/

void IRAM_ATTR isr() {
  button_boot.numberKeyPresses += 1;
  button_boot.pressed = true;
}

RTC_DS3231 rtc;
char daysOfTheWeek[7][12] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};

File sdcard_file;
//DS3231  rtc(SDA, SCL);
int CS_pin = 5; // Pin 5 on esp

#include <Wire.h>
#include "SparkFunHTU21D.h"

//Create an instance of the object
HTU21D myHumidity;

void setup(){
  Serial.begin(115200);
  while (!Serial); delay(100); // *****
  pinMode(button_boot.PIN, INPUT);
  attachInterrupt(button_boot.PIN, isr, RISING);

  Serial.print("Active firmware version:");
  Serial.println(FirmwareVer);

  //connect_wifi();
  Serial.println("HTU21D Example!");

  myHumidity.begin();
  pinMode(CS_pin, OUTPUT);

  // SD Card Initialization
  if (SD.begin())  Serial.println("SD card is ready to use.");
  else             Serial.println("SD card initialization failed");

  Serial.print("   Date  ");
  Serial.print("      ");
  Serial.print("   Time  ");
  Serial.print("      ");
  Serial.print("  Temp   ");
  Serial.println("     ");
  Serial.print("   Hum   ");
  Serial.println("     ");
  Serial.print("   Gas   ");
  Serial.println("     ");
  Serial.print(" Battery ");
  Serial.println("     ");
  sdcard_file = SD.open("data.txt", FILE_WRITE);
  if (sdcard_file) {
    sdcard_file.print("Date  ");
    sdcard_file.print("      ");
    sdcard_file.print("   Time  ");
    sdcard_file.print("     ");
    sdcard_file.print("   Temp   ");
    sdcard_file.println("     ");
    sdcard_file.print("   Hum   ");
    sdcard_file.println("     ");
    sdcard_file.print("   Gas   ");
    sdcard_file.println("     ");
    sdcard_file.print("   Battery   ");
    sdcard_file.println("     ");
    sdcard_file.close(); // close the file
  }
  // if the file didn't open, print an error:
  else {
    Serial.println("error opening test.txt");
  }
  Serial.println("Disconnecting current wifi connection");
  //WiFi.disconnect();
  EEPROM.begin(512); //Initialasing EEPROM
  delay(10);
  pinMode(12, OUTPUT);
  Serial.println();
  //Serial.println();
  Serial.println("Startup");
  //---------------------------------------- Read eeprom for ssid and pass
  Serial.println("Reading EEPROM ssid");

  for (int i = 0; i < 32; ++i)
  {
    esid += char(EEPROM.read(i));
  }
  Serial.println();
  Serial.print("SSID: ");
  Serial.println(esid);
  Serial.println("Reading EEPROM pass");
  for (int i = 32; i < 96; ++i)
  {
    epass += char(EEPROM.read(i));
  }
  Serial.print("PASS: ");
  Serial.println(epass);
  WiFi.mode(WIFI_MODE_APSTA); //******
  Serial.print("Establishing connection to WiFi");
 // WiFi.begin(ssid, passphrase);
  WiFi.begin(esid.c_str(), epass.c_str());

  if (testWifi())
  {
    Serial.println("Succesfully Connected!!!");
    Serial.println("WiFi connected at: " + WiFi.localIP().toString());
  }
  else
  {
    Serial.println("Turning the HotSpot On");
    launchWeb();
    setupAP();// Setup HotSpot
  }
  Serial.println();
  Serial.println("Waiting.");
  while ((WiFi.status() != WL_CONNECTED))
  //Serial.println(WiFi.status());
  {
    Serial.print(".");
    delay(100);
    server.handleClient();
  }
  launchWeb();
  //Time Server
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  //getTime();
  pinMode(TGSPin, INPUT);
  pinMode(BatteryPin, INPUT);
}

void loop()
{
  server.handleClient();
  if (millis() - counter > 600000) //10 minutes interv
  { float humd = myHumidity.readHumidity();
    float temp = myHumidity.readTemperature();
    int gas = analogRead(TGSPin);
    float Battery = (1.65 * (analogRead(BatteryPin)) / 2048.0) * 2.0;
    Serial.print("Time:");
    Serial.print(millis());
    Serial.print(" Temperature:");
    Serial.print(temp, 1);
    Serial.print("C");
    Serial.print(" Humidity:");
    Serial.print(humd, 1);
    Serial.print("%");
    Serial.print(" Gas:");
    Serial.print(gas);
    Serial.print(" Battery:");
    Serial.print(Battery);
    Serial.print("V");
    Serial.println();
    DataLogging(temp, humd,  gas, Battery);
    if (WiFi.status() == WL_CONNECTED)
    {
      Serial.println("wifi connected");

      if (indx != 0)
      {
        for (int i = 0; i < indx; i++)
        {

          Thingspeaks( bTemp[i].toFloat(), bHum[i].toFloat(),  bGas[i].toFloat(), bBattery[i].toFloat());
        }
        indx = 0;
      }
      else
      {
        Thingspeaks(temp, humd,  gas, Battery);
      }
    }
    else

    {
      connect_wifi();
      bTemp[indx] = String(temp);
      bHum[indx] = String(humd);
      bGas[indx] = String(gas);
      bBattery[indx] = String(Battery);
      indx++;
    }
    counter = millis();
  }

  if (button_boot.pressed) { //to connect wifi via Android esp touch app
    Serial.println("Firmware update Starting..");
    firmwareUpdate();
    button_boot.pressed = false;
  }
  repeatedCall();
}

void DataLogging(float tmp, float hm, float gas, float Bat)
{
  DateTime now = rtc.now();
  Serial.print(now.day(), DEC);  Serial.print("-"); Serial.print(now.month(), DEC);  Serial.print("-"); sdcard_file.print(now.year(), DEC);
  Serial.print("  &   ");
  Serial.print(now.second(), DEC);  Serial.print(":"); Serial.print(now.minute(), DEC);  Serial.print(":"); Serial.print(now.hour(), DEC);

  Serial.print(",      ");
  Serial.println(tmp);
  Serial.print(",      ");
  Serial.println(hm);
  Serial.print(",      ");
  Serial.println(gas);
  Serial.print(",      ");
  Serial.println(Bat);
  sdcard_file = SD.open("data.txt", FILE_WRITE);
  if (sdcard_file) {
    sdcard_file.print(now.day(), DEC);  sdcard_file.print("-"); sdcard_file.print(now.month(), DEC);  sdcard_file.print("-"); sdcard_file.print(now.year(), DEC);
    sdcard_file.print("  &   ");
    sdcard_file.print(now.second(), DEC); sdcard_file.print(":"); sdcard_file.print(now.minute(), DEC); sdcard_file.print(":"); sdcard_file.print(now.hour(), DEC);
    sdcard_file.print(",     ");
    sdcard_file.println(tmp, 1);
    sdcard_file.print(",     ");
    sdcard_file.println(hm, 1);
    sdcard_file.print(",     ");
    sdcard_file.println(gas);
    sdcard_file.print(",     ");
    sdcard_file.println(Bat);
    sdcard_file.close(); // close the file
  }
  // if the file didn't open, print an error:
  else {
    Serial.println("error opening test.txt");
  }
  delay(3000);
}
void Thingspeaks(float tmp, float hm, float gas, float Bat)
{
  http.begin("https://api.thingspeak.com/update?api_key=SL3IS82UAT15J05K&field1=" + String(tmp) + "&field2=" + String(hm) + "&field3=" + String(gas) + "&field4=" + String(Bat));


  int httpCode = http.GET();                                  //Send the request
  Serial.println(httpCode);
  //   if (httpCode > 0) { //Check the returning code

  String payload = http.getString();   //Get the request response payload
  Serial.println(payload);             //Print the response payload
}

void firmwareUpdate(void) {
  WiFiClientSecure client;
  client.setCACert(rootCACertificate);
  httpUpdate.setLedPin(12, LOW);
  t_httpUpdate_return ret = httpUpdate.update(client, URL_fw_Bin);

  switch (ret) {
    case HTTP_UPDATE_FAILED:
      Serial.printf("HTTP_UPDATE_FAILD Error (%d): %s\n", httpUpdate.getLastError(), httpUpdate.getLastErrorString().c_str());
      break;

    case HTTP_UPDATE_NO_UPDATES:
      Serial.println("HTTP_UPDATE_NO_UPDATES");
      break;

    case HTTP_UPDATE_OK:
      Serial.println("HTTP_UPDATE_OK");
      break;
  }
}
int FirmwareVersionCheck(void) {
  String payload;
  int httpCode;
  String fwurl = "";
  fwurl += URL_fw_Version;
  fwurl += "?";
  fwurl += String(rand());
  Serial.println(fwurl);
  WiFiClientSecure * client = new WiFiClientSecure;

  if (client)
  {
    client -> setCACert(rootCACertificate);

    // Add a scoping block for HTTPClient https to make sure it is destroyed before WiFiClientSecure *client is
    HTTPClient https;

    if (https.begin( * client, fwurl))
    { // HTTPS
      Serial.print("[HTTPS] GET...\n");
      // start connection and send HTTP header
      delay(100);
      httpCode = https.GET();
      delay(100);
      if (httpCode == HTTP_CODE_OK) // if version received
      {
        payload = https.getString(); // save received version
      } else {
        Serial.print("error in downloading version file:");
        Serial.println(httpCode);
      }
      https.end();
    }
    delete client;
  }

  if (httpCode == HTTP_CODE_OK) // if version received
  {
    payload.trim();
    if (payload.equals(FirmwareVer)) {
      Serial.printf("\nDevice already on latest firmware version:%s\n", FirmwareVer);
      return 0;
    }
    else
    {
      Serial.println(payload);
      Serial.println("New firmware detected");
      return 1;
    }
  }
  return 0;
}
void connect_wifi() {
  //---------------------------------------- Read eeprom for ssid and pass
  Serial.println("Reading EEPROM ssid");

  for (int i = 0; i < 32; ++i)
  {
    esid += char(EEPROM.read(i));
  }
  Serial.println();
  Serial.print("SSID: ");
  Serial.println(esid);
  Serial.println("Reading EEPROM pass");

  for (int i = 32; i < 96; ++i)
  {
    epass += char(EEPROM.read(i));
  }
  Serial.print("PASS: ");
  Serial.println(epass);
  WiFi.begin(esid.c_str(), epass.c_str());
  if (testWifi())
  {
    Serial.println("Succesfully Connected!!!");
    // return;
  }
}

bool testWifi(void)
{
  int c = 0;
  Serial.println("Waiting for Wifi to connect");
  while ( c < 20 ) {
    if (WiFi.status() == WL_CONNECTED)
    {
      return true;
    }
    delay(1000);
    Serial.print("*");
    c++;
  }
  Serial.println("");
  Serial.println("Connect timed out, opening AP");
  return false;
}

void launchWeb()
{
  Serial.println("");
  if (WiFi.status() == WL_CONNECTED)
    Serial.println("WiFi connected");
  Serial.print("Local IP: ");
  Serial.println(WiFi.localIP());
  Serial.print("SoftAP IP: ");
  Serial.println(WiFi.softAPIP());
  while (WiFi.status() != WL_CONNECTED)
  Serial.println(WiFi.status());
  createWebServer();
  // Start the server
  server.begin();
  Serial.println("Server started");
}
void setupAP(void)
{
  //WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);
  int n = WiFi.scanNetworks();
  Serial.println("scan done");
  if (n == 0)
    Serial.println("no networks found");
  else
  {
    Serial.print(n);
    Serial.println(" networks found");
    for (int i = 0; i < n; ++i)
    {
      // Print SSID and RSSI for each network found
      Serial.print(i + 1);
      Serial.print(": ");
      Serial.print(WiFi.SSID(i));
      Serial.print(" (");
      Serial.print(WiFi.RSSI(i));
      Serial.print(")");
      //      Serial.println((WiFi.encryptionType(i) == ENC_TYPE_NONE) ? " " : "*");
      delay(10);
    }
  }
  Serial.println("");
  st = "<ol>";
  for (int i = 0; i < n; ++i)
  {
    // Print SSID and RSSI for each network found
    st += "<li>";
    st += WiFi.SSID(i);
    st += " (";
    st += WiFi.RSSI(i);
    st += ")";
    //    st += (WiFi.encryptionType(i) == ENC_TYPE_NONE) ? " " : "*";
    st += "</li>";
  }
  st += "</ol>";
  delay(100);
  //WiFi.softAP("ESP32TESTAP"); // *****
  WiFi.softAP(soft_ap_ssid, soft_ap_passphrase); // *****
  Serial.println("Initializing_softap_for_wifi credentials_modification");
  launchWeb();
  Serial.println("over");
}

void createWebServer()
{
  {
    server.on("/", []() {
      IPAddress ip = WiFi.softAPIP();
      String ipStr = String(ip[0]) + '.' + String(ip[1]) + '.' + String(ip[2]) + '.' + String(ip[3]);
      content = "<!DOCTYPE HTML>\r\n<html>Welcome to Wifi Credentials Update page";
      content += "<form action=\"/scan\" method=\"POST\"><input type=\"submit\" value=\"scan\"></form>";
      content += ipStr;
      content += "<p>";
      content += st;
      content += "</p><form method='get' action='setting'><label>SSID: </label><input name='ssid' length=32>";
      content += " <label>Password: </label><input name='pass' length=64>";

      content += "<input type='submit'></form>";
      content += "</html>";
      server.send(200, "text/html", content);
    });
    server.on("/scan", []() {
      //setupAP();
      IPAddress ip = WiFi.softAPIP();
      String ipStr = String(ip[0]) + '.' + String(ip[1]) + '.' + String(ip[2]) + '.' + String(ip[3]);
      content = "<!DOCTYPE HTML>\r\n<html>go back";
      server.send(200, "text/html", content);
    });
    server.on("/setting", []() {
      String qsid = server.arg("ssid");
      String qpass = server.arg("pass");

      delay(2000);
      Serial.println();
      Serial.println("------------------Previous Credentials-------------");
      Serial.print("SSID: ");
      Serial.println(esid);
      Serial.print("PASS: ");
      Serial.println(epass);

      if (qsid.length() > 0 && qpass.length() > 0) {
        Serial.println("clearing eeprom");
        for (int i = 0; i < 96; ++i) {
          EEPROM.write(i, 0);
        }
        Serial.println(qsid);
        Serial.println("");
        Serial.println(qpass);
        Serial.println("");
        Serial.println("writing eeprom ssid:");
        for (int i = 0; i < qsid.length(); ++i)
        {
          EEPROM.write(i, qsid[i]);
          Serial.print("Wrote: ");
          Serial.println(qsid[i]);
        }
        Serial.println("writing eeprom pass:");
        for (int i = 0; i < qpass.length(); ++i)
        {
          EEPROM.write(32 + i, qpass[i]);
          Serial.print("Wrote: ");
          Serial.println(qpass[i]);
        }
        EEPROM.commit();
        content = "{\"Success\":\"saved to eeprom... reset to boot into new wifi\"}";
        statusCode = 200;

        // ESP.reset(); //for esp8266
        ESP.restart(); //for esp32

        //      else {
        //        content = "{\"Error\":\"404 not found\"}";
        //        statusCode = 404;
        //        Serial.println("Sending 404");
        //      }

        server.sendHeader("Access-Control-Allow-Origin", "*");
        server.send(statusCode, "application/json", content);
      }
    });
  }
}

void repeatedCall() {
  static int num = 0;
  unsigned long currentMillis = millis();
  if ((currentMillis - previousMillis) >= interval) {
    // save the last time you blinked the LED
    previousMillis = currentMillis;
    if (FirmwareVersionCheck()) {
      firmwareUpdate();
    }
  }
  if ((currentMillis - previousMillis_2) >= mini_interval) {
    previousMillis_2 = currentMillis;
    Serial.print("idle loop...");
    Serial.print(num++);
    Serial.print(" Active fw version:");
    Serial.println(FirmwareVer);
    if (WiFi.status() == WL_CONNECTED)
    {
      Serial.println("wifi connected");
    }
    else
    {
      connect_wifi();
    }
  }
}

unsigned long getTime() {
  struct tm timeinfo;
  getLocalTime(&timeinfo);
  yr = timeinfo.tm_year + 1900;
  mt = timeinfo.tm_mon + 1;
  dy = timeinfo.tm_mday;
  hr = timeinfo.tm_hour;
  mi = timeinfo.tm_min;
  se = timeinfo.tm_sec;
  rtc.adjust(DateTime(yr, mt, dy, hr, mi, se));
  //disconnect WiFi as it's no longer needed
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
}
