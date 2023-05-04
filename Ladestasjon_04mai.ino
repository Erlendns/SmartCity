// Importerer bibliotek
#include <WiFi.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <math.h>

// Kobler til MQTT server på Rpi
const char* ssid = "rpi6";
const char* password = "123456789";
const char* mqtt_server = "192.168.137.7"; // IPen til RPI

// Variabler
int targetBatteryLevel = 0;
int newBatteryLevel;
float newAccountBalance;

int costFactor = 1;
float costPerCharge = 1; // Blir oppdatert fra API
int chargingInterval = 30; // [ms] per loop

float batteryHealthFactor = 1 ; // Fra 0.01 til 1.01
bool emergencyCharge = true;

WiFiClient espClientCharging;
PubSubClient client(espClientCharging);

unsigned long previousTime = 0;
unsigned long chargingSpeed = 0;


void setup() {
  
Serial.begin(115200);

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
    
    if (client.connect("espClientCharging")) {
      Serial.println("Connected to MQTT broker");

      // Subscribe til topics
      client.subscribe("charging");
      client.subscribe("currentBatteryLevel");
      client.subscribe("currentAccountBalance");
      client.subscribe("costPerCharge");
      client.subscribe("batteryHealth");
      client.subscribe("emergencyCharge");
      
    } else {
      Serial.print("Failed with state ");
      Serial.print(client.state());
      delay(2000);
    }
  }
  
 client.setCallback(callback);

}

// Tar imot meldinger på topics og behandler data
void callback(char* topic, byte* message, unsigned int length) {

  Serial.print("Message arrived on topic: ");
  Serial.print(topic);
  Serial.print(". Message: ");
  String messageTemp;
  
  for (int i = 0; i < length; i++) {
    Serial.print((char)message[i]);
    messageTemp += (char)message[i];
    }
    Serial.println();

  // Henter kostnad per %
  if (String(topic) == "costPerCharge") {
    const char* messageChars = messageTemp.c_str();
    costPerCharge = atof(messageChars);
  }

  // Tar inn info fra bilen om tilgjengelig balanse, batterinivå og -helse
  if (String(topic) == "currentAccountBalance") { // Ladestasjonen finner ut hvor mye du kan kjøpe for
    newAccountBalance = messageTemp.toInt();
  }

  if (String(topic) == "currentBatteryLevel") { // Ladestasjonen finner ut hvor mye du kan kjøpe for
    newBatteryLevel = messageTemp.toInt();
    }

  if (String(topic) == "batteryHealth") { // Lager en faktor som reduserer hvor fort batteri lades 
    batteryHealthFactor = messageTemp.toInt();
    batteryHealthFactor = (batteryHealthFactor+1) / 100; // Faktor mellom 0.01 og 1.01, for å ikke dele på 0
    }     

  // Bestiller lading, setter interval og cost
  if (String(topic) == "charging") { // Sjekker hvilken lading bilen har bestilt og om den har nok penger
  
    if ((messageTemp == "50%")&&(newAccountBalance >= 50)) {
      targetBatteryLevel = 50;
      chargingInterval = 30;
      costFactor = 1;
      }  
    if ((messageTemp == "80%")&&(newAccountBalance >= 80)) {
      targetBatteryLevel = 80;
      chargingInterval = 30;
      costFactor = 1;
      }
    if ((messageTemp == "100%")&&(newAccountBalance >= 100)) {
      targetBatteryLevel = 100;
      chargingInterval = 30;
      costFactor = 1;
      }
    if (messageTemp == "Fast charging") {
      targetBatteryLevel = newAccountBalance / 3 ; // Setter target til 1/3 av tilgjengelig balanse, pga det er dyrere. Blir begrenset til maks 100 senere
      chargingInterval = 10;
      costFactor = 3;
      }
  
  else {
    Serial.println("Insufficient funds");
    }
  }

  // Nødlading
  if ((String(topic) == "emergencyCharge") && (emergencyCharge)){
    targetBatteryLevel = 20;
    costFactor = 0;
    emergencyCharge = false;
    }
  
}

void loop() {
  
  client.loop();

  newBatteryLevel = constrain(newBatteryLevel, 0, 100);
  targetBatteryLevel = constrain(targetBatteryLevel, 0, 100);
  
  unsigned long currentTime = millis();

  // looper med intervallet chargingSpeed
  if (currentTime - previousTime >= chargingSpeed) { 

      // Lader batteriet opp mot valgt prosent
      if (newBatteryLevel < targetBatteryLevel + 1) {
      newBatteryLevel += 1;
      
        if (newBatteryLevel <= targetBatteryLevel){
          newAccountBalance -= costFactor * costPerCharge;
        }
      }

      // Resetter target og intern batterinivå
      if (newBatteryLevel == targetBatteryLevel + 1) { 
        targetBatteryLevel = 0;
        newBatteryLevel = 0;
      }

      
      // Endrer hastigheten på loopen i [ms], høyere newBatteryLevel = saktere
      chargingSpeed = (chargingInterval/batteryHealthFactor) * (log(newBatteryLevel + 1) + 1);
      
      /*Serial.print("Charging speed is: ");
      Serial.println(chargingSpeed);*/
     
    // Publiserer kontinuerlig til target er nådd
    if ((newBatteryLevel < targetBatteryLevel + 1) && (newBatteryLevel != 0)) {
      
      char newAccountBalanceString[8];
      dtostrf(newAccountBalance, 1, 2, newAccountBalanceString);
      Serial.print("New account balance: ");
      Serial.println(newAccountBalanceString);
      client.publish("newAccountBalance", newAccountBalanceString);

      char newBatteryLevelString[8];
      dtostrf(newBatteryLevel, 1, 2, newBatteryLevelString);
      Serial.print("New battery level: ");
      Serial.println(newBatteryLevelString); 
      client.publish("newBatteryLevel", newBatteryLevelString);
    }
    
    previousTime = currentTime;
  }
}
