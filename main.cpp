#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <SPI.h>
#include <MFRC522.h>
#include <map>


#define RST_PIN 17
#define SS_PIN 5

const char* BROKER_ADDRESS = "mqtt.myselfgram.com";
const int PORT = 2329;
const int RELAY_PORT = 16;
const String log_topic = "esp32/microLab/mani/log";

const String ssid = "Axe";
const String password = "ramin1234";

WiFiClient espClient;
PubSubClient client(espClient);

void connect_to_internet() {
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        Serial.print(".");
        delay(50);
    }

    Serial.println("Connected to " + ssid);
    Serial.println("IP Address: " + WiFi.localIP().toString());
}

void try_connect_to_mqtt_broker() {
    static unsigned long previous_time;
    unsigned long current_time = millis();

    if (current_time - previous_time > 1000) {
        previous_time = current_time;
        Serial.print("Attempting to connect to MQTT broker.");
        if (client.connect("ManiClient")) {
            Serial.println("Connected");
            client.subscribe("esp32/microLab/mani/open-door");
        }

        else {
            Serial.print("Failed, rc=");
            Serial.print(client.state());
            Serial.println("Retrying in 1 second.");
        }
    }
}

bool door_opened = false;
void open_door() {
    digitalWrite(RELAY_PORT, HIGH);
    door_opened = true;
}

void callback(char* topic, byte* payload, int length) {
    String topic_str = String(topic);
    payload[length] = 0;
    String message = String((char*)payload);

    if (topic_str.equals("esp32/microLab/mani/open-door")) {
        if (message.equals("OPEN")) {
            open_door();
        }

        else if (message.equals("CLOSE")) {
            digitalWrite(RELAY_PORT, LOW);
        }
    }
}

void publish_message(String topic, String payload) {
    char topic_chr[60];
    char payload_chr[120];

    topic.toCharArray(topic_chr, 60);
    payload.toCharArray(payload_chr, 120);

    client.publish(topic_chr, payload_chr);
}

// void loop() {
//     static int counter = 0;
//     unsigned long time = millis();

//     if (time - previous_time > 1000) {

//         publish_message("esp32/microLab/mani/log",
//         String(counter));

//         previous_time = time;
//         counter += 10;

//         if (counter > 100) {
//             counter = 0;
//         }
//     }

//     if (!client.connected()) {
//         try_connect_to_mqtt_broker();
//     }

//     else {
//         client.loop();
//     }

// }      

MFRC522 mfrc522(SS_PIN, RST_PIN);  

String getCardUID(MFRC522::Uid uid){
  String result = "";
  byte* uidCode = uid.uidByte;
  uidCode[uid.size] = 0;
  for(int i = 0; i < uid.size; i++){
    char hexChars[3];
    sprintf(hexChars, "%02X", uidCode[i]);
	result += String(hexChars);
  }
  return result;
}

void setup() {
	Serial.begin(115200);

    pinMode(RELAY_PORT, OUTPUT);
    connect_to_internet();
    client.setServer(BROKER_ADDRESS, PORT);
    client.setCallback(callback);

	SPI.begin();			
	mfrc522.PCD_Init();		
	delay(4);	
	mfrc522.PCD_DumpVersionToSerial();	

	Serial.println("Scan PICC to see UID, SAK, type, and data blocks...");
}


std::map<String, String> uid_to_name = {
    {String("49377F4A"), "Ali Etemadfard"},
    {String("29E68F4A"), "Mani Alimadadi"}
};

unsigned long previous_time = 0;
void loop() {
    unsigned long time = millis();

    if (time - previous_time > 2000) {
        if (door_opened) {
            digitalWrite(RELAY_PORT, LOW);
            door_opened = false;
        }

        previous_time = time;
        
    }


    if (!client.connected()) {
        try_connect_to_mqtt_broker();
    }

    else {
        client.loop();
    }


    Serial.println("before mfrc522");

    if (!mfrc522.PICC_IsNewCardPresent()) return;
    if (!mfrc522.PICC_ReadCardSerial()) return;

    Serial.println("mfrc522 ok");

	// Dump debug info about the card; PICC_HaltA() is automatically called
	mfrc522.PICC_DumpToSerial(&(mfrc522.uid));
    String uid = getCardUID(mfrc522.uid);

    String username;
    // user is in map
    if (uid_to_name.find(uid) != uid_to_name.end()) {
        username = uid_to_name[uid];
        open_door();
        Serial.println("Door opened");
    }

    else {
        username = String("Unknown");
    }

    publish_message(log_topic, String("New card entered: ") + username);
	Serial.println(uid);
}