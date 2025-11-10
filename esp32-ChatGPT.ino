#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "secrets.h"

HTTPClient http;
JsonDocument inputDoc;

// Setup functions with default values
String getInput(const char* message = "");

void setup() {
  Serial.begin(115200);
  while (Serial.availableForWrite() < 20) {}
  delay(1000);
  Serial.print("\n");

  setupWifi(WIFI_SSID, WIFI_PASSWORD);
  setupInputDoc();
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

        case 'c': // option:  -c             reset inputDoc
          setupInputDoc();
          Serial.println("Conversation reset");
          return;
        
        case 'r': { // option: -r [<count>]    remove the specified number of elements (default 1)
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

          const int messagesDeleted = removeMessages(inputDoc, countStr.toInt());
          Serial.print(messagesDeleted);
          Serial.print(" message");
          if (messagesDeleted != 1) Serial.print("s"); // Make plural if needed
          Serial.println(" deleted");
          return;
        }
        
        case 'u': // option  -u              send as a user
          input = input.substring(input.indexOf(' ') + 1); // Remove the option and space
          addMessage(inputDoc, "user", input.c_str());
          Serial.println("Message added as user");
          return;

        case 'd': // option:  -d <message>    send as a developer
          input = input.substring(input.indexOf(' ') + 1); // Remove the option and space
          addMessage(inputDoc, "developer", input.c_str());
          Serial.println("Message added as developer");
          return;

        case 'a': // option:  -a <message>    send as the ai (assistant)
          input = input.substring(input.indexOf(' ') + 1); // Remove the option and space
          addMessage(inputDoc, "assistant", input.c_str());
          Serial.println("Message added as assistant");
          return;
        
        case 'p': // option:  -p              print the conversation as JSON
          serializeJsonPretty(inputDoc, Serial);
          Serial.print("\n");
          return;

        default:
          if (input.substring(0, 6) == "--help") {
            displayHelp();
            return;
          }

          Serial.print("Invalid option: ");
          Serial.println(input);
          Serial.println("\nUse --help for help");
          return;
      }
      break;

    case '"': // Use a double quote at the start of the message to send exactly how it's written
      input.remove(0, 1); // Remove the qoute at the start of the string
    default:
      addMessage(inputDoc, "user", input.c_str());
  }
  
  setupHttp();
  Serial.println("Sending request");

  String postData;
  serializeJson(inputDoc, postData);
  int code = http.POST(postData);

  if (code >= 200 && code < 300) {
    String response = http.getString();

    JsonDocument outputDoc;
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

    addMessage(inputDoc, "assistant", content);
  } else if (code > 0) {
    Serial.print("Response Error: ");
    Serial.println(code);
    Serial.println("Data sent: ");
    serializeJsonPretty(inputDoc, Serial);
    Serial.print("\n");
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
 * Sets up http with pre-defined headers
 */
void setupHttp() {
  http.begin("https://models.github.ai/inference/chat/completions");
  http.setTimeout(30000);

  http.addHeader("Content-Type", "application/json");
  http.addHeader("Authorization", String("Bearer ") + API_TOKEN);
}

String getInput(const char* message) {
  Serial.println(message);

  while (Serial.available() == 0) {}

  return Serial.readString();
}

void setupInputDoc() {
  inputDoc.clear();
  inputDoc["model"] = "openai/gpt-4o-mini";
  inputDoc["messages"].to<JsonArray>();
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

void displayHelp() {
  Serial.print(
    "Welcome to esp32-ChatGPT\n"
    "Usage:\n"
    "  (-h | --help)\n"
    "  (-c | -p)\n"
    "  -r [<count>]\n"
    "  [-u | -d | -a | \"] <message>\n"
    "  <message>\n"
    "\n"
    "Options:\n"
    "  -h | --help     display this help text\n"
    "  -c              reset the conversation\n"
    "  -r [<count>]    remove the specified number of elements (default 1)\n"
    "  -p              print the conversation as JSON\n"
    "  -u              add a message as a user\n"
    "  -d              add a message as a developer\n"
    "  -a              add a message as the ai (assistant)\n"
    "  \"               send a message exactly how it's written\n"
    "\n"
    "Note: If you use -u, -d, or -u, it won't send a request to ChatGPT,\n"
    "      but a quote will still send the request\n"
    "\n"
    "Examples:\n"
    "  -c\n"
    "  -d Only talk using words less than 6 letters\n"
    "  -a Hi! How can I help you\n"
    "  \"-c I don't want this to reset the conversation\n"
  );
}