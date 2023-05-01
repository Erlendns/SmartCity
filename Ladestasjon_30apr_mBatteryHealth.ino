#include <WiFi.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <math.h>

const char* ssid = "rpi6";
const char* password = "123456789";
const char* mqtt_server = "192.168.137.7"; // IPen til RPI

int targetBatteryLevel = 0;

int newBatteryLevel; // Nytt nivå etter endt lading
int newAccountBalance; // Oppdatert kontoinformasjon
int cost; // Hvor mye det koster per % lading
int chargingInterval = 30; // Faktor i ms på hvor ofte hver loop med lading skjer
float batteryHealthFactor = 1 ; // Fra 0.01 til 1.01

WiFiClient espClientCharging;
PubSubClient client(espClientCharging);

unsigned long previousTime = 0;
unsigned long chargingSpeed = 0; // Setter variabelen, blir endret i loop()


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

      // Subscribe to topics
      client.subscribe("charging");
      client.subscribe("currentBatteryLevel");
      client.subscribe("currentAccountBalance");
      client.subscribe("batteryHealth");
      
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
  
  for (int i = 0; i < length; i++) {
    Serial.print((char)message[i]);
    messageTemp += (char)message[i];
  }
    Serial.println();

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

  
  
  if (String(topic) == "charging") { // Sjekker hvilken lading bilen har bestilt og om den har nok penger
  
    if ((messageTemp == "50%")&&(newAccountBalance >= 50)) {
      targetBatteryLevel = 50;
      chargingInterval = 30;
      cost = 1;
      }  
    if ((messageTemp == "80%")&&(newAccountBalance >= 80)) {
      targetBatteryLevel = 80;
      chargingInterval = 30;
      cost = 1;
      }
    if ((messageTemp == "100%")&&(newAccountBalance >= 100)) {
      targetBatteryLevel = 100;
      chargingInterval = 30;
      cost = 1;
      }
    if (messageTemp == "Fast charging") {
      targetBatteryLevel = newAccountBalance / 3 ; // Setter target til 1/3 av tilgjengelig balanse, pga det er dyrere. Blir begrenset til maks 100 senere
      chargingInterval = 10;
      cost = 3;
      }
  
  else {
    Serial.println("Insufficient funds");
    }
  }
  
}

void loop() {
  
  client.loop();

  newBatteryLevel = constrain(newBatteryLevel, 0, 100); // holder batteriet mellom 0 og 100
  targetBatteryLevel = constrain(targetBatteryLevel, 0, 100); // holder target mellom 0 og 100
  
  unsigned long currentTime = millis();
  
  if (currentTime - previousTime >= chargingSpeed) { // millis() som looper med forskj. hastighet utifra batterinivå
      
      if (newBatteryLevel < targetBatteryLevel + 1) { // Lader batteriet opp mot valgt prosent
      newBatteryLevel += 1;
      
        if (newBatteryLevel <= targetBatteryLevel){
          newAccountBalance -= cost;
        }
      }

      if (newBatteryLevel == targetBatteryLevel + 1) { // Resetter targetBatteryLevel etter opplading ferdig
        targetBatteryLevel = 0;
        newBatteryLevel = 0;
      }

      
      // Endrer hastigheten på loopen i millisekund, høyere newBatteryLevel = saktere. chargingInterval bestemmes av om det er hurtiglading eller ikke
      // Vanligvis 30/1.01 * (ln(x+1)+1)
      chargingSpeed = (chargingInterval/batteryHealthFactor) * (log(newBatteryLevel + 1) + 1);
      Serial.print("Charging speed is: ");
      Serial.println(chargingSpeed);
      
     

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
