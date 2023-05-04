
#include <Wire.h>
#include <Zumo32U4.h>

// maks hastighet på bilen som vi setter til 300, kan være opptil 400. 
const uint16_t maxSpeed = 200;

Zumo32U4Buzzer buzzer;
Zumo32U4LineSensors lineSensors;
Zumo32U4Motors motors;
Zumo32U4ButtonC buttonC;

String incomming_string;

//int stop;

int16_t lastError = 0;

#define NUM_SENSORS 5
unsigned int lineSensorValues[NUM_SENSORS];

void calibrateSensors()
{

  // venter 1 sekund før den starter Kalibrering. 
  // roterer 360 grader først den ene veien så 720 grader tilbake den andre veien, samtidig leser den linjene på underlaget for å danne seg en grunnlag. 
  delay(1000);
  for(uint16_t i = 0; i < 120; i++)
  {
    if (i > 30 && i <= 90)
    {
      motors.setSpeeds(-200, 200);
    }
    else
    {
      motors.setSpeeds(200, -200);
    }

    lineSensors.calibrate();
  }
  motors.setSpeeds(0, 0);
}


void setup()
{
  Serial.begin(115200);
  // Setter opp Serial1, som er to ledninger mellom ESP32 og Zumo32U4
  Serial1.begin(115200);


  lineSensors.initFiveSensors();

  

  // spiller en en lyd 
  buzzer.play(">g32>>c32");

  buttonC.waitForButton();

  calibrateSensors();

  //showReadings();

  // spiller en lyd før den begynner å kjøre. 
  buttonC.waitForButton();
  buzzer.play("L16 cdegreg4");
  while(buzzer.isPlaying());
}

void dataESP32(){
  // funksjon for å hente data fra ESP32
  if (Serial1.available()) {
    // Leser ut alle bytes, frem til '\n'
     incomming_string = Serial1.readStringUntil('\n');
 
    // Skriver ut resultatet til seriemonitoren
    Serial.println(incomming_string);

    if (incomming_string == 'S') { // hvis mottatt karakter er 'S', stopp roboten
        motors.setSpeeds(0, 0);
        //motors.setSpeeds(0, 0);
        while (Serial1.available() > 0) { // tøm bufferen for å unngå å lese inn samme karakter flere ganger
          Serial.read();
        } 
      }
  }
}

void LineFollower(){
  // linje følger programmet.
   
  int16_t position = lineSensors.readLine(lineSensorValues);
  
  int16_t error = position - 2000;
  //

  // Get motor speed difference using proportional and derivative
  // PID terms (the integral term is generally not very useful
  // for line following).  Here we are using a proportional
  // constant of 1/4 and a derivative constant of 6, which should
  // work decently for many Zumo motor choices.  You probably
  // want to use trial and error to tune these constants for your
  // particular Zumo and line course.
  int16_t speedDifference = error / 4 + 6 * (error - lastError);

  lastError = error;

  
  int16_t leftSpeed = (int16_t)maxSpeed + speedDifference;
  int16_t rightSpeed = (int16_t)maxSpeed - speedDifference;
  // får individuelle hastighet for hver motor når de skal svinge. 

  
  //leftSpeed = constrain(leftSpeed, 0, (int16_t)maxSpeed);
  //rightSpeed = constrain(rightSpeed, 0, (int16_t)maxSpeed);
  // hvis man vil at bilen ikke skal spinne. 

  motors.setSpeeds(leftSpeed, rightSpeed);  
}



void loop()
{
  

  dataESP32();
  // kaller på funksjonen for henting av data fra ESP32 

  LineFollower(); 
  // kaller på linjefølger programmet. 
  

  
}
