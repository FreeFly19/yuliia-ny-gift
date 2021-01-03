struct DeviceConfig {
  String ssid;
  String password;
  String botToken;
  String botChatId;
};

DeviceConfig getDeviceConfigFromFile(String fileName) {
  File file = SPIFFS.open(fileName, "r");
 
  if (!file) {
      Serial.println("Failed to open " + fileName + " file for reading");
  }

  DynamicJsonDocument doc(192);
  deserializeJson(doc, file);

  DeviceConfig deviceConfig;
  deviceConfig.ssid = doc["ssid"].as<String>();
  deviceConfig.password = doc["password"].as<String>();
  deviceConfig.botToken = doc["botToken"].as<String>();
  deviceConfig.botChatId = doc["botChatId"].as<String>();
  file.close();
  return deviceConfig;
}

void saveDeviceConfigToFile(StaticJsonDocument<128> creds, String fileName) {
  File file = SPIFFS.open(fileName, "w");
 
  if (!file) {
      Serial.println("Failed to open " + fileName + " file for writing");
  }

  serializeJson(creds, file);
  file.close();
}

void saveBotLastMessageNumber(long lastMessageNumber) {
  File file = SPIFFS.open("/bot_message_offset.txt", "w");
  file.print(String(lastMessageNumber) + '\n');
  Serial.println("saveBotLastMessageNumber: " + String(lastMessageNumber));
  file.close();
}

long getBotLastMessageNumber() {
  File file = SPIFFS.open("/bot_message_offset.txt", "r");
  if (!file) {
    return 0;
  }
  
  String s = file.readStringUntil('\n');
  
  file.close();
  long res = atol(s.c_str());
  Serial.println("getBotLastMessageNumber: " + String(res));

  return res;
}

void deepSleep(int deepSleepSec) {
    esp_sleep_enable_timer_wakeup(deepSleepSec * 1e6);
    Serial.println("Going to sleep for " + String(deepSleepSec) + " seconds...");
    Serial.flush(); 
    esp_deep_sleep_start();
}
