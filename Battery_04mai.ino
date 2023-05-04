// Importerer bibliotek
#include <WiFi.h>
#include <PubSubClient.h>

// Kobler til MQTT server
const char* ssid = "rpi6";
const char* password = "123456789";
const char* mqtt_server = "192.168.137.7";

// Variabler
const int buttonPin = 2;
volatile bool buttonPressed = false;

unsigned long previousTime = 0;
unsigned long lastDebounceTime = 0;
const long interval = 1000;

// Startverdi for batteri prosent, kontobalanse og batterihelse
int currentBatteryLevel = 100;
int currentAccountBalance = 10000;
float batteryHealth = 20;
float batteryHealthFactor = 1;

WiFiClient espClientBattery;
PubSubClient client(espClientBattery);


void setup() {
  
  Serial.begin(115200);

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
      client.subscribe("trashPay");
      
    } else {
      Serial.print("Failed with state ");
      Serial.print(client.state());
      delay(2000);
    }
  }
  
client.setCallback(callback);

}


// Tar inn topics og messages og behandler dem i IF-betingelser
void callback(char* topic, byte* message, unsigned int length) {
  
  Serial.print("Message arrived on topic: ");
  Serial.print(topic);
  Serial.print(". Message: ");
  String messageTemp;
  
  for (int i = 0; i < length; i++) {
    Serial.print((char)message[i]);
    messageTemp += (char)message[i];
  }

  // Henter ny verdi fra ladestasjon, oppdaterer currentAccountBalance, currentBatteryLevel og batteryHealth
 
  if (String(topic) == "newAccountBalance") {
    currentAccountBalance = messageTemp.toInt();
  }

  if (String(topic) == "newBatteryLevel") { 
    currentBatteryLevel = messageTemp.toInt();
    batteryHealth -= 0.1; // Batteriet blir 0.1% dårligere med hver prosent lading det tar
  }

  if (String(topic) == "changeBattery") { 
    batteryHealth = 100;
    currentAccountBalance -= messageTemp.toInt(); // Trekker fra 2000 fra konto
  }
  
  if (String(topic) == "trashPay") {
      if (messageTemp == "Close"){
        currentAccountBalance += 500;
      } 
  }  
  
}


// Kalles på når knappen trykkes inn (Bilen har komt til ladestasjonen)
void buttonInterrupt() {

  if ((millis() - lastDebounceTime) > 50) {
    buttonPressed = true; // Flagg for å publisere til NodeRED
    }
  
  lastDebounceTime = millis();
}


void loop() {

  client.loop();
  
  if (buttonPressed) { 

      // Sender ut batteri, kontobalanse og batterihelse til ladestasjon
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
  
 
  if (millis() - previousTime > interval*batteryHealthFactor) { // Loopen som tømmer batteriet, blir fortere jo laver batterihelsen er
  
    previousTime = millis();

    currentBatteryLevel = constrain(currentBatteryLevel, 0, 100); // Holder batteriet mellom 0 og 100
    batteryHealthFactor = (batteryHealth + 1) / 100; // Faktor fra 0.01 til 1.01
    
      if (currentBatteryLevel > 0) { // Tømmer batteriet hvert sekund, sjekker ideelt om bilen kjører
         currentBatteryLevel -= 1;
         }
         
      Serial.print("Battery level is: ");
      Serial.println(currentBatteryLevel);
      Serial.print("Battery health is: ");
      Serial.println(batteryHealthFactor);
      Serial.print("Account balance is: ");
      Serial.println(currentAccountBalance);
    }
  
}
