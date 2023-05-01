#include <WiFi.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include "Adafruit_BME680.h"

const char* ssid = "rpi6";
const char* password = "123456789";
const char* mqtt_server = "192.168.137.7"; // IPen til RPI

// Variabler
const int ledPin = 4;
const int buzzerChannel = 0;
const int buzzerPin = 5;
const int pResistor = 34;

float hum_weighting = 0.25; // so hum effect is 25% of the total air quality score
float gas_weighting = 0.75; // so gas effect is 75% of the total air quality score

float hum_score, gas_score;
float gas_reference = 250000;
float hum_reference = 40;
int   getgasreference_count = 0;

long  lastMsg = 0;
char  msg[50];
int   value = 0;

#define SEALEVELPRESSURE_HPA (1013.25)

// WiFi og PubSub setup
WiFiClient espClient;
PubSubClient client(espClient);

Adafruit_BME680 bme; // I2C
float temperature = 0;
float gas = 0;
int pResistorReading = 0;

void setup() {
  Serial.begin(115200);
  
  while (!Serial);
  Serial.println(F("BME680 async test"));

  Wire.begin();
  if (!bme.begin()) {
    Serial.println(F("Could not find a valid BME680 sensor, check wiring!"));
    while (1);
  }

  // Set up oversampling and filter initialization
  bme.setTemperatureOversampling(BME680_OS_8X);
  bme.setHumidityOversampling(BME680_OS_2X);
  bme.setPressureOversampling(BME680_OS_4X);
  bme.setIIRFilterSize(BME680_FILTER_SIZE_3);
  bme.setGasHeater(320, 150); // 320*C for 150 ms
  GetGasReference(); // Referanseverdi for VOC nivå, kalibrering

  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);

  pinMode(ledPin, OUTPUT);
  pinMode(pResistor, INPUT);
  pinMode(buzzerPin, OUTPUT);

  ledcSetup(buzzerChannel, 4000, 10);
  ledcAttachPin(buzzerPin, buzzerChannel);
  ledcWriteTone(buzzerChannel, 5000);
  ledcWrite(buzzerChannel, 4);
  
}



void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    
    // Attempt to connect
    if (client.connect("ESP8266Client")) {
      Serial.println("connected");
      
      // Subscribe
  client.subscribe("esp32/lightOutput");
  client.subscribe("esp32/buzzerOutput");
  client.subscribe("esp32/fanOutput");
    }
       
    else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void setup_wifi() {
  delay(10);
  // We start by connecting to a WiFi network
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

void callback(char* topic, byte* message, unsigned int length) {

  Serial.println();
  Serial.print("Message arrived on topic: ");
  Serial.print(topic);
  Serial.print(". Message: ");
  
  String messageTemp;

  for (int i = 0; i < length; i++) {
  Serial.print((char)message[i]); 
  messageTemp += (char)message[i];}
  
  // If a message is received on the topic esp32/output, you check if the message is either "on" or "off". 
  // Changes the output state according to the message

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
      ledcWrite(buzzerChannel, 64);
      ledcWriteTone(buzzerChannel, 64);
      }
      
    if(messageTemp == "OFF"){
      ledcWrite(buzzerChannel, 0);
      ledcWriteTone(buzzerChannel, 0);
      }
  }
  
  if (String(topic) == "esp32/fanOutput") {
    
    if(messageTemp == "ON"){
      // Fan ON
      }
      
    if(messageTemp == "OFF"){
      // Fan OFF
      }
  }
}
    


void loop() {

  if (!client.connected()) {
    reconnect();
  }
  
  client.loop();  

  long now = millis();
  if (now - lastMsg > 2000) {
    lastMsg = now;


  Serial.print("Temperature = ");

  Serial.print(bme.readTemperature());
  Serial.println("°C");

  Serial.print("   Pressure = ");

  Serial.print(bme.readPressure() / 100.0F);
  Serial.println(" hPa");

  Serial.print("   Humidity = ");
  Serial.print(bme.readHumidity());
  Serial.println("%");

  Serial.print("        Gas = ");
  Serial.print(bme.readGas());
  Serial.println("R\n");
 
  //Calculate humidity contribution to IAQ index
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
  
  //Calculate gas contribution to IAQ index
  float gas_lower_limit = 5000;   // Bad air quality limit
  float gas_upper_limit = 50000;  // Good air quality limit 
  if (gas_reference > gas_upper_limit) gas_reference = gas_upper_limit; 
  if (gas_reference < gas_lower_limit) gas_reference = gas_lower_limit;
  gas_score = (0.75/(gas_upper_limit-gas_lower_limit)*gas_reference -(gas_lower_limit*(0.75/(gas_upper_limit-gas_lower_limit))))*100;
  
  //Combine results for the final IAQ index value (0-100% where 100% is good quality air)
  float air_quality_score = hum_score + gas_score;

  Serial.println("Air Quality = "+String(air_quality_score,1)+"% derived from 25% of Humidity reading and 75% of Gas reading - 100% is good quality air");
  Serial.println("Humidity element was : "+String(hum_score/100)+" of 0.25");
  Serial.println("     Gas element was : "+String(gas_score/100)+" of 0.75");
  if (bme.readGas() < 120000) Serial.println("***** Poor air quality *****");
  Serial.println();
  if ((getgasreference_count++)%10==0) GetGasReference(); 
  Serial.println(CalculateIAQ(air_quality_score));
  Serial.println("------------------------------------------------");


      // IAQ  
    
    char IAQString[8];
    dtostrf(air_quality_score, 1, 2, IAQString);
    Serial.print("Publishing IAQ score in %: ");
    Serial.println(IAQString);
    client.publish("esp32/IAQ", IAQString);

    // Temp
    temperature =  bme.temperature;
    
    char tempString[8];
    dtostrf(temperature, 1, 2, tempString);
    Serial.print("Publishing temperature: ");
    Serial.println(tempString);
    client.publish("esp32/temperature", tempString);

    // Light
    pResistorReading = analogRead(pResistor);
    int lightLevel = map(pResistorReading, 0, 4095, 0, 100);
    
    char lightString[8];
    dtostrf(lightLevel, 1, 2, lightString);
    Serial.print("Publishing light level: ");
    Serial.println(lightString);
    client.publish("esp32/lightLevel", lightString);

  }
    
  // Tell BME680 to begin measurement.
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

void GetGasReference(){
  // Now run the sensor for a burn-in period, then use combination of relative humidity and gas resistance to estimate indoor air quality as a percentage.
  Serial.println("Getting a new gas reference value");
  int readings = 10;
  for (int i = 1; i <= readings; i++){ // read gas for 10 x 0.150mS = 1.5secs
    gas_reference += bme.readGas();
  }
  gas_reference = gas_reference / readings;
}

String CalculateIAQ(float score){
  String IAQ_text = "Air quality is ";
  score = (100-score)*5;
  if      (score >= 301)                  IAQ_text += "Hazardous";
  else if (score >= 201 && score <= 300 ) IAQ_text += "Very Unhealthy";
  else if (score >= 176 && score <= 200 ) IAQ_text += "Unhealthy";
  else if (score >= 151 && score <= 175 ) IAQ_text += "Unhealthy for Sensitive Groups";
  else if (score >=  51 && score <= 150 ) IAQ_text += "Moderate";
  else if (score >=  00 && score <=  50 ) IAQ_text += "Good";
  return IAQ_text;
}
