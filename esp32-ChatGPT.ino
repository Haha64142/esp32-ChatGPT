#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "secrets.h"

HTTPClient http;
String input;
JsonDocument inputDoc;
JsonDocument outputDoc;
String postData;

// Setup functions with default values
String getInput(const char* message = "");

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.print("\n");

  setupWifi(WIFI_SSID, WIFI_PASSWORD);
}

void loop() {
  setupHttp();

  input = getInput("\nPlease enter your message");

  createPostData();

  int code = http.POST(postData);

  if (code >= 200 && code < 300) {
    String response = http.getString();

    DeserializationError error = deserializeJson(outputDoc, response);

    if (error) {
      Serial.print("JSON parse failed: ");
      Serial.println(error.c_str());
      return;
    }

    const char* content = outputDoc["choices"][0]["message"]["content"];

    Serial.print("Response: ");
    Serial.println(code);
    Serial.println(content);
  } else if (code > 0) {
    Serial.print("Response Error: ");
    Serial.println(code);
    Serial.println("Data sent: ");
    serializeJsonPretty(inputDoc, Serial);
  } else {
    Serial.print("Connection Error: ");
    Serial.println(code);
    Serial.println(http.errorToString(code));
  }

  http.end();
}

void setupWifi(const char* ssid, const char* password) {
  Serial.println("\nConnecting to WiFi");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(100);
    Serial.print(".");
  }
  Serial.println("\nConnected to WiFi");
}

/**
Sets up http with pre-defined headers
*/
void setupHttp() {
  http.begin("https://models.github.ai/inference/chat/completions");
  http.setTimeout(15000);

  http.addHeader("Content-Type", "application/json");
  http.addHeader("Authorization", String("Bearer ") + API_TOKEN);
}

String getInput(const char* message) {
  Serial.println(message);

  while (Serial.available() == 0) {}

  return Serial.readString();
}

/**
Set the postData variable using ArduinoJson.
Use https://arduinojson.org/v7/assistant to easily serialize the JSON
*/
void createPostData() {
  inputDoc.clear();
  inputDoc["model"] = "openai/gpt-4o-mini";

  JsonArray messages = inputDoc["messages"].to<JsonArray>();

  JsonObject messages_0 = messages.add<JsonObject>();
  messages_0["role"] = "user";
  messages_0["content"] = input;

  // JsonObject messages_0 = messages.add<JsonObject>();
  // messages_0["role"] = "developer";
  // messages_0["content"] = "Do things in the most unhinged way possible";

  // JsonObject messages_1 = messages.add<JsonObject>();
  // messages_1["role"] = "user";
  // messages_1["content"] = "How do you eat a pizza";

  inputDoc.shrinkToFit();
  serializeJson(inputDoc, postData);
}