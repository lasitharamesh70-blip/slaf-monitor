#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <SocketIoClient.h> // Instant Sync සඳහා

// --- Wi-Fi විස්තර ---
const char* ssid = "Redmi Note 13";
const char* pass = "19961121";

// --- Ngrok Server විස්තර (http/https රහිතව host එක පමණක්) ---
char host[] = "winter-uniaxial-inflexibly.ngrok-free.dev";
const char* fullUrl = "https://winter-uniaxial-inflexibly.ngrok-free.dev/api/register";

int RELAY_PIN = D5; 
int SENSOR_PIN = A0; 
String myID = "";

SocketIoClient webSocket;
WiFiClientSecure secureClient;

// --- Dashboard එකෙන් Command එකක් ආවම ක්‍රියාත්මක වන කොටස ---
void handleControl(const char * payload, size_t length) {
    StaticJsonDocument<200> doc;
    deserializeJson(doc, payload);
    
    String targetID = doc["id"];
    String cmd = doc["cmd"];

    // මගේ ID එකට ආපු මැසේජ් එකක්ද බලනවා
    if (targetID == myID) {
        if (cmd == "ON") {
            digitalWrite(RELAY_PIN, HIGH);
            Serial.println("INSTANT COMMAND: ON");
        } else {
            digitalWrite(RELAY_PIN, LOW);
            Serial.println("INSTANT COMMAND: OFF");
        }
    }
}

void setup() {
    Serial.begin(115200);
    pinMode(RELAY_PIN, OUTPUT);
    digitalWrite(RELAY_PIN, LOW);

    WiFi.begin(ssid, pass);
    Serial.print("Connecting to WiFi");
    while (WiFi.status() != WL_CONNECTED) { 
        delay(500); 
        Serial.print("."); 
    }
    Serial.println("\nWiFi Connected!");

    myID = WiFi.macAddress();
    myID.replace(":", ""); 
    Serial.println("Device ID: " + myID);

    // Socket.io සබඳතාවය ආරම්භ කිරීම
    secureClient.setInsecure(); // Ngrok සඳහා අත්‍යවශ්‍යයි
    webSocket.on("control", handleControl);
    webSocket.begin(host, 443); // SSL (HTTPS) සඳහා Port 443
}

// --- Current එක මැනීමේ Function එක ---
float readCurrent() {
    float voltagePerAmp = 0.185; 
    int readValue = 0;
    int sampleCount = 100;

    for (int i = 0; i < sampleCount; i++) {
        readValue += analogRead(SENSOR_PIN);
        delay(2);
    }
    float averageValue = (float)readValue / sampleCount;
    float voltage = (averageValue / 1024.0) * 3.3; 
    float current = (voltage - 1.65) / voltagePerAmp; 

    if (current < 0.15) current = 0; 
    return current;
}

void loop() {
    webSocket.loop(); // Instant commands සඳහා මෙය නිතර ක්‍රියාත්මක විය යුතුයි

    static unsigned long lastTime = 0;
    if (millis() - lastTime > 10000) { // තත්පර 10කට වරක් සර්වර් එකට දත්ත යවයි
        updateServer();
        lastTime = millis();
    }
}

void updateServer() {
    if (WiFi.status() == WL_CONNECTED) {
        HTTPClient http;
        http.begin(secureClient, fullUrl);
        http.addHeader("Content-Type", "application/json");
        http.addHeader("ngrok-skip-browser-warning", "true");

        float currentAmps = readCurrent();

        StaticJsonDocument<200> doc;
        doc["id"] = myID;
        doc["current"] = currentAmps;

        String requestBody;
        serializeJson(doc, requestBody);

        int httpResponseCode = http.POST(requestBody);

        if (httpResponseCode > 0) {
            String response = http.getString();
            Serial.println("HB Sync: " + response);
            
            // මුල් වරට සර්වර් එකෙන් එන පවර් ස්ටේටස් එක ලබා ගැනීම
            StaticJsonDocument<200> resDoc;
            deserializeJson(resDoc, response);
            if (resDoc.containsKey("power")) {
                String pwr = resDoc["power"];
                digitalWrite(RELAY_PIN, (pwr == "ON") ? HIGH : LOW);
            }
        }
        http.end();
    }
}