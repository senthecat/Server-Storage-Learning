#include <Arduino.h>
#include <WiFiMulti.h>
#include <WebSocketsClient.h>

#include <ArduinoJson.h>

const char* ssid = "F9BEE3-gateway";
const char* password = "Blue7901";

#define WS_HOST "y3txhbyqo6.execute-api.us-west-1.amazonaws.com"
#define WS_PORT 443
#define WS_URL  "/dev"

#define MSG_SIZE  256

WiFiMulti wifiMulti;
WebSocketsClient wsClient;

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
    if (strcmp(doc["body"]["type"],  "digitalWrite") == 0) {
      digitalWrite(doc["body"]["pin"], (int)doc["body"]["value"]);
      sendOkMessage();
      return;
    }
    if (strcmp(doc["body"]["type"],  "digitalRead") == 0) {
      auto value = digitalRead(doc["body"]["pin"]);

      // handle reading value section 
      char msg[MSG_SIZE];
      sprintf(msg, "{\"action\":\"msg\",\"type\":\"output\",\"body\":%d}", value);
      wsClient.sendTXT(msg);

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

void setup() {
  Serial.begin(115200);
  Serial.print("Establishing server to: ");
  Serial.println(ssid);

  pinMode(LED_BUILTIN, OUTPUT);

  wifiMulti.addAP(ssid, password);

  while (wifiMulti.run() != WL_CONNECTED) {
    delay(500);
    Serial.println("...");
  }

  Serial.println("WiFi connected");

  wsClient.beginSSL(WS_HOST, WS_PORT, WS_URL, "", "wss");
  wsClient.onEvent(onWSEventHandler);
}

void loop() {
  digitalWrite(LED_BUILTIN, WiFi.status() == WL_CONNECTED);
  wsClient.loop();
}
