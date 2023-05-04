// Importerer bibliotek
#include <WiFi.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include "Adafruit_BME680.h"
#include "esp32-hal-cpu.h"

// Setter opp MQTT server
const char* ssid = "rpi6";
const char* password = "123456789";
const char* mqtt_server = "192.168.137.7"; // IPen til RPI

// Variabler
const int buzzerChannel = 0;
const int ledPin = 4;
const int buzzerPin = 5;
const int fanPin = 33;
const int pResistor = 34;

float hum_weighting = 0.25; // luftfuktighet står for 25% av luftkvalitet
float gas_weighting = 0.75; // gassmålinger står for 75% av luftkvalitet

float hum_score, gas_score;
float gas_reference = 250000;
float hum_reference = 40;
int   getgasreference_count = 0;

long  lastMsg = 0;
char  msg[50];
int   value = 0;
int   interval = 500;

float temperature = 0;
float gas = 0;
int   pResistorReading = 0;

#define SEALEVELPRESSURE_HPA (1013.25)

// WiFi og PubSub setup
WiFiClient espClientSensorNode;
PubSubClient client(espClientSensorNode);

Adafruit_BME680 bme; // I2C kommunikasjonsprotokoll


void setup() {
  Serial.begin(115200);
  
  while (!Serial);
    Serial.println(F("BME680 async test"));

  Wire.begin();
  
  if (!bme.begin()) {
    Serial.println(F("Could not find a valid BME680 sensor, check wiring!"));
    while (1);
  }

  // Setter opp sampling fra BME 680 og filter
  bme.setTemperatureOversampling(BME680_OS_8X);
  bme.setHumidityOversampling(BME680_OS_2X);
  bme.setPressureOversampling(BME680_OS_4X);
  bme.setIIRFilterSize(BME680_FILTER_SIZE_3);
  bme.setGasHeater(320, 150); // 320*C for 150 ms
  GetGasReference(); // Referanseverdi for VOC nivå kalibreres hver 10. måling

  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);

  // pins
  pinMode(pResistor, INPUT);
  pinMode(ledPin, OUTPUT);
  pinMode(buzzerPin, OUTPUT);
  pinMode(fanPin, OUTPUT);

  // buzzer
  ledcSetup(buzzerChannel, 4000, 10);
  ledcAttachPin(buzzerPin, buzzerChannel);
  ledcWriteTone(buzzerChannel, 5000);
  ledcWrite(buzzerChannel, 4);
  
}


// Kobler seg på clienten
void reconnect() {
  
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    
  // Reconnect
  if (client.connect("espClientSensorNode")) {
      Serial.println("connected");
      
  // Subscribe
  client.subscribe("esp32/lightOutput");
  client.subscribe("esp32/buzzerOutput");
  client.subscribe("esp32/fanOutput");
  client.subscribe("esp32/powerUsage");
  }
       
  else {
    Serial.print("failed, rc=");
    Serial.print(client.state());
    Serial.println(" try again in 5 seconds");
    // Venter 5 sek før den prøver igjen
    delay(5000);
    }
  }
}

// Kobler til WiFi nettverket rpi6 som er delt fra PC, gir beskjed om lokal IP og når man kobles på
void setup_wifi() {
  delay(100);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: "); 
  Serial.println(WiFi.localIP());
}

// callback tar inn en beskjed på en topic, og legger de inn i en string MessageTemp. Blir senere brukt til outputs
void callback(char* topic, byte* message, unsigned int length) {

  Serial.println();
  Serial.print("Message arrived on topic: ");
  Serial.print(topic);
  Serial.print(". Message: ");
  
  String messageTemp;

  for (int i = 0; i < length; i++) {
  Serial.print((char)message[i]); 
  messageTemp += (char)message[i];}
  
  // IF-betingelser sjekker topic og message, og setter definerte outputs på LED, buzzer og vifte. 

  if (String(topic) == "esp32/lightOutput") {
    if(messageTemp == "ON"){
      digitalWrite(ledPin, HIGH);
      }
    else if(messageTemp == "OFF"){
      digitalWrite(ledPin, LOW);
      }
  }
  
  if (String(topic) == "esp32/buzzerOutput") {
    if(messageTemp == "ON"){
      ledcWrite(buzzerChannel, 32);
      ledcWriteTone(buzzerChannel, 32);
      }
    if(messageTemp == "OFF"){
      ledcWrite(buzzerChannel, 0);
      ledcWriteTone(buzzerChannel, 0);
      }
  }
  
  if (String(topic) == "esp32/fanOutput") {
    if(messageTemp == "ON"){
      digitalWrite(fanPin, HIGH);
      } 
    if(messageTemp == "OFF"){
      digitalWrite(fanPin, LOW);
      }
  }

  // Setter ned ytelsen til ESP32 sensornoden til fordel for mindre strømbruk
  if (String(topic) == "esp32/powerUsage"){
    if (messageTemp == "LOW") {
      interval = 3000;
      Serial.end();
      WiFi.setSleep(WIFI_PS_MIN_MODEM); 
      setCpuFrequencyMhz(80);
      }
    if (messageTemp == "HIGH"){
      interval = 500;
      Serial.begin(115200);
      WiFi.setSleep(WIFI_PS_NONE);
      setCpuFrequencyMhz(240);
      }
  } 
  
}
    

void loop() {

  if (!client.connected()) {
    reconnect();
  }
  
  client.loop(); // Holder klienten oppe  

  long now = millis();
  if (now - lastMsg > interval) {
 
  // Regner ut bidrag fra luftfuktighet
  float current_humidity = bme.readHumidity();
  if (current_humidity >= 38 && current_humidity <= 42)
    hum_score = 0.25*100; // Humidity +/-5% around optimum 
  else
  { //sub-optimal
    if (current_humidity < 38) 
      hum_score = 0.25/hum_reference*current_humidity*100;
    else
    {
      hum_score = ((-0.25/(100-hum_reference)*current_humidity)+0.416666)*100;
    }
  }
  
  // Regner ut bidrag fra gasspartikler
  float gas_lower_limit = 5000;   // Bad air quality limit
  float gas_upper_limit = 50000;  // Good air quality limit 
  if (gas_reference > gas_upper_limit) gas_reference = gas_upper_limit; 
  if (gas_reference < gas_lower_limit) gas_reference = gas_lower_limit;
  gas_score = (0.75/(gas_upper_limit-gas_lower_limit)*gas_reference -(gas_lower_limit*(0.75/(gas_upper_limit-gas_lower_limit))))*100;
  
  // Kombinerer resultater, gir en score fra 0-100% der 100% er best
  float air_quality_score = hum_score + gas_score;

  if ((getgasreference_count++)%10==0) GetGasReference();

    // Publisher verdier fra sensorer til Node Red
 
    char IAQString[8]; // IAQ 
    dtostrf(air_quality_score, 1, 2, IAQString);
    Serial.print("Publishing IAQ score in %: ");
    Serial.println(IAQString);
    client.publish("esp32/IAQ", IAQString);

    temperature =  bme.temperature; // Temp
    char tempString[8];
    dtostrf(temperature, 1, 2, tempString);
    Serial.print("Publishing temperature: ");
    Serial.println(tempString);
    client.publish("esp32/temperature", tempString);

    pResistorReading = analogRead(pResistor); // Light
    int lightLevel = map(pResistorReading, 0, 4095, 0, 100);
    char lightString[8];
    dtostrf(lightLevel, 1, 2, lightString);
    Serial.print("Publishing light level: ");
    Serial.println(lightString);  
    client.publish("esp32/lightLevel", lightString);

    Serial.println("-------------------------------------");

    lastMsg = now;
  }
    
  // Gir beskjer om BME680 ikke klarer å lese sensordata
  unsigned long endTime = bme.beginReading();
  if (endTime == 0) {
    Serial.println(F("Failed to begin reading :("));
    return;
  }
  
  if (!bme.endReading()) {
    Serial.println(F("Failed to complete reading :("));
    return;
  }

}

// Tar 10 referansemålinger over 1.5 sekunder, skjer etter hver 10. måling.
void GetGasReference(){
  
  Serial.println("Getting a new gas reference value, 1.5 sec");
  int readings = 10;
  for (int i = 1; i <= readings; i++){ // read gas for 10 x 0.150mS = 1.5secs
    gas_reference += bme.readGas();
  }
  gas_reference = gas_reference / readings;
}
