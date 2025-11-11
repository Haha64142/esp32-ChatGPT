#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "secrets.h"

HTTPClient http;
JsonDocument messageDoc;

String model = "openai/gpt-4.1-mini";

String getInput(const char* message = "");
// Setup functions with default values

void setup() {
  Serial.begin(115200);
  while (Serial.availableForWrite() < 20) {}
  delay(1000);
  Serial.print("\n");

  String ssid;
  String password;
  bool connected = setupWifi(WIFI_SSID, WIFI_PASSWORD);
  while (!connected) {
    ScanWiFi();

    ssid = getInput("Please enter the WiFi SSID you would like to connect to");
    password = getInput("Please enter the password for the network");

    connected = setupWifi(ssid.c_str(), password.c_str());
  }
  setupMessageDoc();
}

void loop() {
  String input = getInput("\nPlease enter your message");
  if (input.endsWith("\n")) {
    input.remove(input.length() - 1);
  }
  if (input == "") {
    Serial.println("Cannot send an empty string. Use --help for help");
    return;
  }

  switch (input[0]) {
    case '-':
      switch (input[1]) {
        case 'h': // option:  -h | --help    display a help message
          displayHelp();
          return;

        case 'c':   // option:  -c             reset messageDoc
          setupMessageDoc();
          Serial.println("Conversation reset");
          return;
        
        case 'r': { // option:  -r [<count>]   remove the specified number of elements (default 1)
          input = input.substring(input.indexOf(' ') + 1); // Remove the option and space

          // Get the count (uses consecutive digits)
          String countStr = "";
          for (size_t i = 0; i < input.length(); ++i) {
            if (isdigit(input[i])) {
              countStr += input[i];
            } else {
              break;
            }
          }

          // Defaults to 1
          if (countStr == "") {
            countStr = "1";
          }

          const int messagesDeleted = removeMessages(messageDoc, countStr.toInt());
          Serial.print(messagesDeleted);
          Serial.print(" message");
          if (messagesDeleted != 1) Serial.print("s"); // Make plural if needed
          Serial.println(" deleted");
          return;
        }
        
        case 'u':  // option:  -u              send as a user
          input = input.substring(input.indexOf(' ') + 1); // Remove the option and space
          addMessage(messageDoc, "user", input.c_str());
          Serial.println("Message added as user");
          return;

        case 'd':  // option:  -d <message>    send as a developer
          input = input.substring(input.indexOf(' ') + 1); // Remove the option and space
          addMessage(messageDoc, "developer", input.c_str());
          Serial.println("Message added as developer");
          return;

        case 'a':  // option:  -a <message>    send as the ai (assistant)
          input = input.substring(input.indexOf(' ') + 1); // Remove the option and space
          addMessage(messageDoc, "assistant", input.c_str());
          Serial.println("Message added as assistant");
          return;
        
        case 'm': { // option:  -m [<model>]   set the ai model (omit argument to display the current model)
          input.concat(" ");
          input = input.substring(input.indexOf(' ') + 1);
          String newModel = input.substring(0, input.indexOf(' '));

          if (newModel == "") {
            Serial.print("Current model: ");
            Serial.println(model);
            return;
          }

          model = newModel;
          messageDoc["model"] = model;
          Serial.print("Model set: ");
          Serial.println(model);
          return;
        }
        
        case 'p':  // option:  -p              print the conversation as JSON
          serializeJsonPretty(messageDoc, Serial);
          Serial.print("\n");
          return;

        default: {
          input.concat(" ");
          const String option = input.substring(0, input.indexOf(' '));
          if (option == "--help") {
            displayHelp();
            return;
          } else if (option == "--get-models") { // display the github ai models from https://models.github.ai/catalog/models
            fetchModels();
            return;
          }

          Serial.print("Invalid option: ");
          Serial.println(option);
          Serial.println("\nUse --help for help");
          return;
        }
      }
      break;

    case '"': // Use a double quote at the start of the message to send exactly how it's written
      input.remove(0, 1); // Remove the qoute at the start of the string
    default:
      addMessage(messageDoc, "user", input.c_str());
  }
  
  setupHttp();

  String postData;
  serializeJson(messageDoc, postData);

  Serial.println("Sending request");
  int httpCode = http.POST(postData);

  if (httpCode >= 200 && httpCode < 300) {
    String response = http.getString();

    JsonDocument responseDoc;
    DeserializationError error = deserializeJson(responseDoc, response);

    if (error) {
      Serial.print("JSON parse failed: ");
      Serial.println(error.c_str());
      return;
    }

    const char* content = responseDoc["choices"][0]["message"]["content"];

    Serial.print("Response: ");
    Serial.println(httpCode);
    Serial.println(content);

    addMessage(messageDoc, "assistant", content);
  } else if (httpCode > 0) {
    Serial.print("Response Error: ");
    Serial.println(httpCode);
    Serial.println("Data sent: ");
    serializeJsonPretty(messageDoc, Serial);
    Serial.print("\n");
  } else {
    Serial.print("Connection Error: ");
    Serial.println(httpCode);
    Serial.println(http.errorToString(httpCode));
  }

  http.end();
}

bool setupWifi(const char* ssid, const char* password) {
  Serial.println("\nConnecting to WiFi");
  WiFi.disconnect(true);
  WiFi.begin(ssid, password);
  const int beginTime = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - beginTime < 20000) { // 20 second connection timeout
    delay(100);
    Serial.print(".");
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nConnected to WiFi");
    return true;
  }

  Serial.println("\nConnection failed");
  return false;
}

void ScanWiFi() {
  Serial.println("-------------------------------------");
  Serial.println("Scan start");
  WiFi.disconnect(true);
  WiFi.STA.begin();
  // WiFi.scanNetworks will return the number of networks found.
  int n = WiFi.scanNetworks();
  Serial.println("Scan done");
  if (n == 0) {
    Serial.println("no networks found");
  } else {
    Serial.print(n);
    Serial.println(" networks found");
    Serial.println("Nr | SSID                             | RSSI | CH | Encryption");
    for (int i = 0; i < n; ++i) {
      // Print SSID and RSSI for each network found
      Serial.printf("%2d", i + 1);
      Serial.print(" | ");
      Serial.printf("%-32.32s", WiFi.SSID(i).c_str());
      Serial.print(" | ");
      Serial.printf("%4ld", WiFi.RSSI(i));
      Serial.print(" | ");
      Serial.printf("%2ld", WiFi.channel(i));
      Serial.print(" | ");
      switch (WiFi.encryptionType(i)) {
        case WIFI_AUTH_OPEN:            Serial.print("open"); break;
        case WIFI_AUTH_WEP:             Serial.print("WEP"); break;
        case WIFI_AUTH_WPA_PSK:         Serial.print("WPA"); break;
        case WIFI_AUTH_WPA2_PSK:        Serial.print("WPA2"); break;
        case WIFI_AUTH_WPA_WPA2_PSK:    Serial.print("WPA+WPA2"); break;
        case WIFI_AUTH_WPA2_ENTERPRISE: Serial.print("WPA2-EAP"); break;
        case WIFI_AUTH_WPA3_PSK:        Serial.print("WPA3"); break;
        case WIFI_AUTH_WPA2_WPA3_PSK:   Serial.print("WPA2+WPA3"); break;
        case WIFI_AUTH_WAPI_PSK:        Serial.print("WAPI"); break;
        default:                        Serial.print("unknown");
      }
      Serial.println();
      delay(10);
    }
  }

  // Delete the scan result to free memory for code below.
  WiFi.scanDelete();
  Serial.println("-------------------------------------");
}

/**
 * Sets up http with pre-defined headers
 */
void setupHttp() {
  http.begin("https://models.github.ai/inference/chat/completions");
  http.setTimeout(30000);

  http.addHeader("Content-Type", "application/json");
  http.addHeader("Authorization", String("Bearer ") + API_TOKEN);
}

/**
 * @brief Gets input from serial
 * 
 * @param message Sends the specified message before getting input. Sends a nothing if no parameters specified.
 */
String getInput(const char* message) {
  Serial.println(message);

  while (Serial.available() == 0) {}

  return Serial.readString();
}

/**
 * Resets messageDoc for a blank conversation
 */
void setupMessageDoc() {
  messageDoc.clear();
  messageDoc["model"] = model;
  messageDoc["messages"].to<JsonArray>();
}

void addMessage(JsonDocument& doc, const char* role, const char* content) {
  JsonObject newMessage = doc["messages"].add<JsonObject>();
  newMessage["role"] = role;
  newMessage["content"] = content;
}

/**
 * @return The number of messages actually deleted
 */
int removeMessages(JsonDocument& doc, int count) {
  JsonArray messages = doc["messages"];
  int messagesDeleted = 0;

  if (count > messages.size()) {
    count = messages.size();
  }

  for (; count > 0; --count, ++messagesDeleted) {
    messages.remove(messages.size() - 1);
  }

  return messagesDeleted;
}

void fetchModels() {
  http.begin("https://models.github.ai/catalog/models");
  int httpCode = http.GET();

  if (httpCode >= 200 && httpCode < 300) {
    String response = http.getString();

    JsonDocument responseDoc;
    DeserializationError error = deserializeJson(responseDoc, response);

    if (error) {
      Serial.print("JSON parse failed: ");
      Serial.println(error.c_str());
      return;
    }

    JsonArray models = responseDoc.as<JsonArray>();

    Serial.println("Model IDs and rate_limit_tier:");
    for (JsonObject model : models) {
      const char* id = model["id"];
      const char* tier = model["rate_limit_tier"];
      Serial.print("ID: ");
      Serial.print(id);
      Serial.print("  |  Tier: ");
      Serial.println(tier);
    }
  } else if (httpCode > 0) {
    Serial.print("Response Error: ");
    Serial.println(httpCode);
  } else {
    Serial.print("Connection Error: ");
    Serial.println(httpCode);
    Serial.println(http.errorToString(httpCode));
  }

  http.end();
}

void displayHelp() {
  Serial.print(
    "Welcome to esp32-ChatGPT\n"
    "Usage:\n"
    "  (-h | --help)\n"
    "  (-c | -p)\n"
    "  -r [<count>]\n"
    "  -m [<model>]\n"
    "  --get-models\n"
    "  [-u | -d | -a | \"] <message>\n"
    "  <message>\n"
    "\n"
    "Options:\n"
    "  -h | --help     display this help text\n"
    "  -c              reset the conversation\n"
    "  -r [<count>]    remove the specified number of elements (default 1)\n"
    "  -m [<model>]    set the ai model (omit argument to display the current model)\n"
    "  --get-models    display github ai models from https://models.github.ai/catalog/models\n"
    "  -p              print the conversation as JSON\n"
    "  -u              add a message as a user\n"
    "  -d              add a message as a developer\n"
    "  -a              add a message as the ai (assistant)\n"
    "  \"               send a message exactly how it's written\n"
    "\n"
    "Note: If you use -u, -d, or -a, it won't send a request to ChatGPT,\n"
    "      but a quote will still send the request\n"
    "\n"
    "Examples:\n"
    "  -c\n"
    "  -d Only talk using words less than 6 letters\n"
    "  -a Hi! How can I help you\n"
    "  \"-c I don't want this to reset the conversation\n"
  );
}