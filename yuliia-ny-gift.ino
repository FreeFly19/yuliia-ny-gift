#include "FS.h"
#include "SPIFFS.h"


#include <WiFi.h>
#include <WiFiMulti.h>
#include <WiFiAP.h>
#include <WiFiClientSecure.h>
#include <DNSServer.h>


// Should by installed using library manager
#include <OneWire.h>
#include <DallasTemperature.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <AsyncJson.h>
#include <UniversalTelegramBot.h>

#include "utils.h"

WiFiMulti wifiMulti;

OneWire oneWire(4); // DS18B20 pin
DallasTemperature sensors(&oneWire);

IPAddress apIP(192, 168, 0, 1);
AsyncWebServer server(80);

const int deepSleepSec = 30;
bool hasInternetConnection;

DNSServer dnsServer;
DeviceConfig deviceConfig;
 
volatile bool shouldGoToSleep = false;
const int ledPin = 12;


void IRAM_ATTR onTimer() {
  shouldGoToSleep = true;
}

void startWifiAccessPointAndConfigurationWebServer();
void handleNewMessages(UniversalTelegramBot* bot, int numNewMessages);
void bot_setup(UniversalTelegramBot* bot);

void setup() {
  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, LOW);
  sensors.begin();
  Serial.begin(115200);

  if(!SPIFFS.begin(true)){
      Serial.println("SPIFFS Mount Failed");
      return;
  }

  Serial.println("Reading device configurations...");
  deviceConfig = getDeviceConfigFromFile("/config.json");
  wifiMulti.addAP(deviceConfig.ssid.c_str(), deviceConfig.password.c_str());

  Serial.println("Connecting Wifi...");
  
  hasInternetConnection = wifiMulti.run() == WL_CONNECTED;
  
  if(hasInternetConnection) {
    Serial.println("WiFi connected");
    WiFiClientSecure client;
    UniversalTelegramBot bot(deviceConfig.botToken, client);
    bot_setup(&bot);
    int numNewMessages = bot.getUpdates(getBotLastMessageNumber() + 1);
    handleNewMessages(&bot, numNewMessages);
    deepSleep(deepSleepSec);
  } else {
    startWifiAccessPointAndConfigurationWebServer();
    hw_timer_t * timer = timerBegin(0, 80, true);
    timerAttachInterrupt(timer, &onTimer, true);
    timerAlarmWrite(timer, 1800 * 1e6, true);
    timerAlarmEnable(timer);
  }

}

void loop() {
  if (!hasInternetConnection) {
    dnsServer.processNextRequest();
    if (shouldGoToSleep) {
      deepSleep(deepSleepSec);
    }
  }
}


void startWifiAccessPointAndConfigurationWebServer() {  
  Serial.println("WiFi is not connected. Staring AP...");
  WiFi.disconnect();
  WiFi.mode(WIFI_OFF);
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
  WiFi.softAP("SmartHouse");
  dnsServer.start(53, "*", apIP);

  Serial.print("AP IP address: ");
  Serial.println(apIP);
  
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/index.html", "text/html");
  });

  server.on("/api/config", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/config.json", "application/json");
  });

  AsyncCallbackJsonWebHandler* handler = new AsyncCallbackJsonWebHandler("/api/config", [](AsyncWebServerRequest *request, JsonVariant &json) {
    StaticJsonDocument<128> jsonObj = json.as<JsonObject>();
    
    Serial.println((const char*)jsonObj["ssid"]);
    request->send(200, "application/json", "{}");
    saveDeviceConfigToFile(jsonObj, "/config.json");
    ESP.restart();
  });
  
  server.addHandler(handler);

  server.begin();
}


void handleNewMessages(UniversalTelegramBot* bot, int numNewMessages) {
  Serial.print("New Messages: ");
  Serial.println(numNewMessages);

  for (int i=0; i<numNewMessages; i++) {
    // Chat id of the requester
    String chat_id = String(bot->messages[i].chat_id);
    if (chat_id != deviceConfig.botChatId){
      bot->sendMessage(chat_id, "Unauthorized user", "");
      continue;
    }
    
    String text = bot->messages[i].text;
    Serial.println("Received a message:");
    Serial.println(text);

    if (text == "/temperature") {
      sensors.requestTemperatures(); 
      float temperatureC = sensors.getTempCByIndex(0);
      Serial.println(temperatureC);
      bot->sendMessage(deviceConfig.botChatId, String("Temperature: ") + String(temperatureC), "");
    }
    
    if (text == "/light") {
      bot->sendMessage(chat_id, "LED state set to ON for 30 seconds", "");
      digitalWrite(ledPin, HIGH);
      delay(30000);
    }
  }
  saveBotLastMessageNumber(max(bot->last_message_received, getBotLastMessageNumber()));
}

void bot_setup(UniversalTelegramBot* bot) {
  const String commands = F("["
                            "{\"command\":\"temperature\",  \"description\":\"Get current temperature\"},"
                            "{\"command\":\"light\", \"description\":\"Turns on the light for some period of time\"}"
                            "]");
  bot->setMyCommands(commands);
}
