#include <WiFi.h>
#include <PubSubClient.h>
#include <Wire.h>

const char* ssid = "rpi6";
const char* password = "123456789";
const char* mqtt_server = "192.168.137.7"; // IPen til RPI

// Variabler
const int buttonPin = 2;
volatile bool buttonPressed = false; // interrupt kan forårsake problemer i client.loop()

int currentBatteryLevel = 100; // Startverdi for batteri prosent
int currentAccountBalance = 1000; // Kontoinformasjon
float batteryHealth = 100; //Battery health

unsigned long previousTime = 0;
unsigned long lastDebounceTime = 0;
const long interval = 1000; //intervallet av ms vi vil ha i loopen

WiFiClient espClientBattery; // Viktig å skille mellom navnene på de forskjellige Clientene
PubSubClient client(espClientBattery);

void setup() {
  
  Serial.begin(115200);

  // Sensor til ladestasjonen, foreløpig en knapp
  pinMode(buttonPin, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(buttonPin), buttonInterrupt, FALLING);

  // WiFi setup kode
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED){
    delay(1000);
    Serial.println("Connecting to Wifi..");
    }
  
  Serial.println("Connected to Wifi");
  Serial.println("IP adress");
  Serial.println(WiFi.localIP());

  client.setServer(mqtt_server,1883);
  
  while (!client.connected()) {
    
    if (client.connect("espClientBattery")) {
      Serial.println("Connected to MQTT broker");
      
      // Subscribe to topics
      client.subscribe("newAccountBalance");
      client.subscribe("newBatteryLevel");
      client.subscribe("changeBattery");
      
    } else {
      Serial.print("Failed with state ");
      Serial.print(client.state());
      delay(2000);
    }
  }
  
client.setCallback(callback);

}

void callback(char* topic, byte* message, unsigned int length) {
  
  Serial.print("Message arrived on topic: ");
  Serial.print(topic);
  Serial.print(". Message: ");
  String messageTemp;
  
  // Hver gang callback kjører legges innholdet inn i messageTemp
  for (int i = 0; i < length; i++) {
    Serial.print((char)message[i]);
    messageTemp += (char)message[i];
  }

  // Henter ny verdi fra ladestasjon, og legger i currentAccountBalance og currentBatteryLevel.
 
  if (String(topic) == "newAccountBalance") {
    currentAccountBalance = messageTemp.toInt();
  }

  if (String(topic) == "newBatteryLevel") { 
    currentBatteryLevel = messageTemp.toInt(); // 
    batteryHealth -= 0.1; // Batteriet blir dårligere med hver prosent lading det tar
  }

  if (String(topic) == "changeBattery") { // Bytter batteriet
  batteryHealth = 100;
  currentAccountBalance -= messageTemp.toInt(); // Trekker fra 100 fra konto
  }
  
}

void buttonInterrupt() { // Kalles på når knappen trykkes inn (Bilen har komt til ladestasjonen)

  if ((millis() - lastDebounceTime) > 50) {
    buttonPressed = true; // Flagg, når denne er true publisher ESPen til NodeRED
    }
  
  lastDebounceTime = millis();
}


void loop() {

  client.loop();
  
  if (buttonPressed) { 

      // Publishes topics
      char currentBatteryLevelString[8];
      dtostrf(currentBatteryLevel, 1, 2, currentBatteryLevelString);
      Serial.print("Publishing battery level: ");
      Serial.println(currentBatteryLevelString);
      client.publish("currentBatteryLevel", currentBatteryLevelString);

      char currentAccountBalanceString[8];
      dtostrf(currentAccountBalance, 1, 2, currentAccountBalanceString);
      Serial.print("Publishing account balance: ");
      Serial.println(currentAccountBalanceString);
      client.publish("currentAccountBalance", currentAccountBalanceString);

      char batteryHealthString[8];
      dtostrf(batteryHealth, 1, 2, batteryHealthString);
      Serial.print("Publishing battery health: ");
      Serial.println(batteryHealthString);
      client.publish("batteryHealth", batteryHealthString);
      
      buttonPressed = false; // Resetter flagget
  }
  
 
  if (millis() - previousTime > interval) { // Looper hver 1 sekund
  
    previousTime = millis(); 

    currentBatteryLevel = constrain(currentBatteryLevel, 0, 100); // Holder batteriet mellom 0 og 100
    
      if (currentBatteryLevel > 0) { // Tømmer batteriet hvert sekund, burde sjekke om bilen er i gang
         currentBatteryLevel -= 1;
         }
         
      Serial.print("Battery level is: ");
      Serial.println(currentBatteryLevel);
      Serial.print("Battery health is: ");
      Serial.println(batteryHealth);
         
    }
  
}
