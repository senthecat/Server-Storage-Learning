#include <Arduino.h>
#include <WiFiMulti.h>
#include <WebSocketsClient.h>

#include <ArduinoJson.h>

#include <LiquidCrystal.h>

/* Wifi Socket Constants */
const char* ssid = "F9BEE3-gateway";
const char* password = "Blue7901";

#define WS_HOST "y3txhbyqo6.execute-api.us-west-1.amazonaws.com"
#define WS_PORT 443
#define WS_URL  "/dev"

#define MSG_SIZE    256
#define LCD_SIZE    16
#define LCD_ROWS    2
#define MAX_SPACES  5

/* LCD Constant Variables Constants */
#define RS  21
#define EN  23 
#define D4  18
#define D5  19 
#define D6  25
#define D7  26

/* Physical Integraiton */
#define NEXT_BUTTON 27

/* Main Variables */
WiFiMulti wifiMulti;
WebSocketsClient wsClient;

LiquidCrystal lcd(RS, EN, D4, D5, D6, D7);

void sendErrorMessage(const char* error) {
  char msg[MSG_SIZE];
  sprintf(msg, "{\"action\":\"msg\",\"type\":\"error\",\"body\":\"%s\"}", error);
  wsClient.sendTXT(msg);
}

void sendOkMessage() {
  char msg[MSG_SIZE];
  sprintf(msg, "{\"action\":\"msg\",\"type\":\"status\",\"body\":\"ok\"}");
  wsClient.sendTXT(msg);
}

uint8_t toMode(const char* val) {
  if (strcmp(val, "output") == 0) {
    return OUTPUT;
  } 
  if (strcmp(val, "input") == 0) {
    return INPUT_PULLUP;
  } 

  return INPUT;
}

void lcdPublishSentence(const char* name, const char* text) {
  lcd.clear();

  uint8_t maxSpaces = MAX_SPACES;

  char topRow[LCD_SIZE+1]; 
  sprintf(topRow, "%s says", name ? name : "ur mom");
  topRow[LCD_SIZE] = '\0';

  // Initial print topRow
  lcd.home();
  lcd.print(topRow);

  if (!text) text = "";
  size_t len = strlen(text);

  lcd.setCursor(0,1);
  if (len <= LCD_SIZE) {
    lcd.print(text);
    for (size_t i = len ; i < LCD_SIZE; i ++) lcd.print(' ');
    return;
  }

  const size_t gap = MAX_SPACES;
  const size_t cycle = len + gap;

  size_t offset = 0;

  while (digitalRead(NEXT_BUTTON) == LOW) {
    lcd.setCursor(0, 1);

    for (size_t i = 0; i < LCD_SIZE; i++) {
      size_t idx = (offset + i) % cycle;
      char c = (idx < len) ? text[idx] : ' ';
      lcd.print(c);
    }

    // For first print give a small delay
    if (offset == 0) {
      delay(1000);
    }

    offset = (offset + 1) % cycle;
    delay(250);
  }
}

// payload handler
void handlePayload(uint8_t* payload, size_t length) {
  JsonDocument doc;

  DeserializationError error = deserializeJson(doc, payload, length);

  // incorrect json
  if (error) {
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.f_str());
    sendErrorMessage(error.c_str());
    return;
  }

  if (!doc["type"].is<const char *>()) {
    sendErrorMessage("invalid message type format");
    return;
  }

  // cmd parsing
  if (strcmp(doc["type"], "cmd") ==  0) {
    if (!doc["body"].is<JsonObject>()) {
      sendErrorMessage("Invalid cmd body");
      return;
    }

    // pinMode Selection
    if (strcmp(doc["body"]["type"],  "pinMode") == 0) {
      pinMode(doc["body"]["pin"], toMode(doc["body"]["mode"]));
      sendOkMessage();
      return;
    }
    // write to pin
    if (strcmp(doc["body"]["type"],  "digitalWrite") == 0) {
      digitalWrite(doc["body"]["pin"], (int)doc["body"]["value"]);
      sendOkMessage();
      return;
    }
    // read pin
    if (strcmp(doc["body"]["type"],  "digitalRead") == 0) {
      auto value = digitalRead(doc["body"]["pin"]);

      // handle reading value section 
      char msg[MSG_SIZE];
      sprintf(msg, "{\"action\":\"msg\",\"type\":\"output\",\"body\":%d}", value);
      wsClient.sendTXT(msg);
      return;
    }

    // display text
    if (strcmp(doc["body"]["type"], "lcdWrite") == 0) {
      if (!doc["body"]["value"].is<const char*>()) {
        sendErrorMessage("lcdWrite value must be a string");
        return;
      }
      lcdPublishSentence(doc["body"]["name"], doc["body"]["value"]);
      sendOkMessage();
      return;
    }

    sendErrorMessage("unsupported command type");
  }

  sendErrorMessage("unsupported message type");
}

// Event Handler for websocket
void onWSEventHandler(WStype_t type, uint8_t * payload, size_t length) {
  switch (type) {
    case WStype_CONNECTED:
      Serial.println("WS Connected");
      break;
    case WStype_DISCONNECTED:
      Serial.println("WS Disconnected");
      break;
    case WStype_TEXT:
      Serial.printf("WS Message: %s\n", payload);
      
      handlePayload(payload, length);

      break;
  }
}

static inline void initWS() {
  Serial.print("Establishing server to: ");
  Serial.println(ssid);

  wifiMulti.addAP(ssid, password);

  while (wifiMulti.run() != WL_CONNECTED) {
    delay(500);
    Serial.println("Retrying ...");
  }

  Serial.println("WiFi Connected");
  wsClient.beginSSL(WS_HOST, WS_PORT, WS_URL, "", "wss");
  wsClient.onEvent(onWSEventHandler);
}

static inline void initLCD() {
  lcd.begin(16,2);
  lcd.clear();
  lcd.print("Ready...");
}

void setup() {
  Serial.begin(115200);
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(NEXT_BUTTON, INPUT_PULLUP);
  initWS();
  initLCD();
}

void loop() {
   // Visual Confirmation that connection has been established
  digitalWrite(LED_BUILTIN, WiFi.status() == WL_CONNECTED);
  wsClient.loop();
}
