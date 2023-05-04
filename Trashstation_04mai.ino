// Importerer bibliotek
#include <WiFi.h>
#include <PubSubClient.h>

// Kobler til MQTT server
const char* ssid = "rpi6";
const char* password = "123456789";
const char* mqtt_server = "192.168.137.7";

//Variabler
const int triggerPin = 32; //Trigger for ultrasonisk sensor
const int echoPin = 35; //Registrerer ultrasonisk lyd som kommer tilbake til sensoren
const int greenledPin = 33;
const int yellowledPin = 25;
const int redledPin = 26;

float duration, distance;

bool isDistancePrinted = false;

WiFiClient espClientTrashstation;
PubSubClient client(espClientTrashstation);

void setup() {
  Serial.begin(115200);

  //Setter pinModes på de forskjellige pinnene som brukes
  pinMode(triggerPin, OUTPUT);
  pinMode(greenledPin, OUTPUT);
  pinMode(yellowledPin, OUTPUT);
  pinMode(redledPin, OUTPUT);
  pinMode(echoPin, INPUT);

  // WiFi setup kode
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED){
    delay(1000);
    Serial.println("Connecting to Wifi..");
    }
  
  Serial.println("Connected to Wifi");
  Serial.println("IP adress");
  Serial.println(WiFi.localIP());

  //Kobler til MQTT serveren
  client.setServer(mqtt_server,1883);
  while (!client.connected()) {
    if (client.connect("espClientTrashstation")) {
      Serial.println("Connected to MQTT broker");  
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
}


void loop() {
  //Aktiverer og deaktiverer trigger på den ultrasoniske sensoren, dette sender pulser som kan registrerers
  digitalWrite(triggerPin, LOW);
  delayMicroseconds(2);
  digitalWrite(triggerPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(triggerPin, LOW);

  //Registrerer den ultrasoniske lyden hvis echoPin blir høy
  duration = pulseIn(echoPin, HIGH);
  //Regner ut distansen basert på hvor lang tid det tar å måle pulsen tilbake
  distance = (duration*.0343)/2;
  //Serial.print("Distance: ");
  //Serial.println(distance);

  //Setter et delay når noe kommer mot sensoren, for at det skal registreres riktig verdi når det kommer til ro forran sensoren
  if (distance < 30 && distance > 22){
    delay(2000);
  }


  //If løkke som sender forskjellige meldinger til NodeRED basert på hvor bilen er i forhold til distanse fra sensoren
  //Denne koden setter også et flagg til true, som gjør at denne koden ikke gjør målinger hele tiden, men en gang før den blir resatt
  if (distance < 5 && !isDistancePrinted) { //Sender Close hvis distansen er under 5cm
    digitalWrite(yellowledPin, LOW);        
    digitalWrite(redledPin, LOW);
    digitalWrite(greenledPin, HIGH);
    Serial.print("Publishing placement from sensor: ");
    Serial.println("Close");
    client.publish("Trashstation", "Close");
    isDistancePrinted = true;
  } else if (distance < 10 && !isDistancePrinted) { //Sender Mid hvis distansen er under 10 cm
    digitalWrite(yellowledPin, HIGH);
    digitalWrite(redledPin, LOW);
    digitalWrite(greenledPin, LOW);
    Serial.print("Publishing placement from sensor: ");
    Serial.println("Mid");
    client.publish("Trashstation", "Mid");
    isDistancePrinted = true;
  } else if (distance < 20 && !isDistancePrinted) { //Sender Mid hvis distansen er under 20 cm
    digitalWrite(greenledPin, LOW);
    digitalWrite(yellowledPin, LOW);
    digitalWrite(redledPin, HIGH);
    Serial.print("Publishing placement from sensor: ");
    Serial.println("Far");
    client.publish("Trashstation", "Far");
    isDistancePrinted = true;
  } else if (distance > 21 && isDistancePrinted) { //Resetter opperasjonen når bilen kjører vekk fra og mot sensoren og tillater ny måling
    digitalWrite(greenledPin, LOW);
    digitalWrite(yellowledPin, LOW);
    digitalWrite(redledPin, LOW);
    Serial.println("Not detected");
    client.publish("Trashstation", "Not detected");
    isDistancePrinted = false;
  }
}
