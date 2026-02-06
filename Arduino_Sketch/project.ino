#include <WiFi.h>
#include <Ethernet.h> // RJ45 සඳහා
#include <PubSubClient.h>

// 1. WiFi සැකසුම්
const char* ssid = "Redmi Note 13";
const char* password = "19961121";

// 2. ඔබේ PC එකේ IP ලිපිනය මෙතනට ලබා දෙන්න (CMD එකේ ipconfig ගසා බලන්න)
const char* mqtt_server = "192.168.43.100"; 

// 3. පින් සැකසුම් (Pins)
#define RELAY_PIN 26        // Smartboard එක පාලනය කරන Relay එක
#define POWER_SENSOR_PIN 34   // පවර් එක එනවාදැයි බලන සෙන්සර් පින් එක

// 4. උපාංගයේ නම (Dashboard එකේ පෙන්වන ID එක)
String deviceID = "BOARD_OFFICE_01"; 

WiFiClient espClient;
EthernetClient ethClient;
PubSubClient client;

// --- WiFi සම්බන්ධ කිරීම ---
void setup_wifi() {
  delay(10);
  Serial.println("\nConnecting to WiFi...");
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi Connected!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
  client.setClient(espClient); // WiFi පාවිච්චි කරන ලෙස දැනුම් දීම
}

// --- Ethernet (RJ45) පරීක්ෂා කිරීම ---
void setup_network() {
  byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0x01 };
  
  // මුලින්ම RJ45 පරීක්ෂා කරයි
  if (Ethernet.begin(mac) != 0) {
    Serial.println("Ethernet Connected via RJ45!");
    client.setClient(ethClient);
  } else {
    // RJ45 නැත්නම් WiFi වලට මාරු වෙයි
    Serial.println("Ethernet not found. Switching to WiFi...");
    setup_wifi();
  }
}

// --- Dashboard එකෙන් ලැබෙන විධාන පරීක්ෂා කිරීම ---
void callback(char* topic, byte* payload, unsigned int length) {
  String message = "";
  for (int i = 0; i < length; i++) message += (char)payload[i];

  Serial.print("Command Received: ");
  Serial.println(message);

  if (message == "ON") {
    digitalWrite(RELAY_PIN, HIGH);
  } else if (message == "OFF") {
    digitalWrite(RELAY_PIN, LOW);
  }
}

// --- MQTT නැවත සම්බන්ධ කිරීම (Reconnect) ---
void reconnect() {
  while (!client.connected()) {
    Serial.print("Connecting to Dashboard (MQTT)...");
    if (client.connect(deviceID.c_str())) {
      Serial.println("Connected!");
      // පාලක විධාන ලබා ගැනීමට Subscribe කිරීම
      client.subscribe(("smartboard/control/" + deviceID).c_str());
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" retrying in 5 seconds...");
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(POWER_SENSOR_PIN, INPUT);
  
  // පද්ධතිය ආරම්භයේදී Relay එක OFF කර තබන්න
  digitalWrite(RELAY_PIN, LOW);

  // Network (Ethernet/WiFi) ආරම්භ කිරීම
  setup_network();

  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
}

unsigned long lastMsg = 0;

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  // සෑම තත්පර 3කට වරක්ම Status එක Dashboard එකට යැවීම
  unsigned long now = millis();
  if (now - lastMsg > 3000) {
    lastMsg = now;

    // පවර් එක තිබේදැයි බලන සැබෑ තත්වය කියවීම
    bool isPowered = digitalRead(POWER_SENSOR_PIN);
    String currentPowerStatus = isPowered ? "ON" : "OFF";

    // JSON ආකාර