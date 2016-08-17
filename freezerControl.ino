#include <OneWire.h>
#include <DallasTemperature.h>

//********************PIN CONFIG************************
// Data wire is plugged into pin 8 on the Arduino
#define ONE_WIRE_BUS 8 // Pin 2 is used by LCD now
#define RELAY_PIN 9    // Pin controlling the relay      
#define LED_PIN 13     // LED pin
#define DIAL_PIN 1     // Analog pin to read Dial setting
//******************************************************

//********************PARAMETERS************************
#define DIAL_LOW 1023
#define DIAL_HIGH 183

#define SET_TEMP_LOW 0      // Min temperature (times by 10)
#define SET_TEMP_HIGH 200   // Max temperature (times by 10)

//******************************************************

// Setup a oneWire instance to communicate with any OneWire devices 
// (not just Maxim/Dallas temperature ICs)
OneWire oneWire(ONE_WIRE_BUS);
 
// Pass our oneWire reference to Dallas Temperature.
DallasTemperature sensors(&oneWire);

unsigned long lastCycledTime; // Stores the last time cycled
unsigned long cycleDelta; // Stores the min allowed cycle time (10 minutes?)

float temperature;

float minTemp; // Tracks min and max temp
float maxTemp;

float tempSetPoint; // Target temperature
float tempDelta; // The delta for the temperature cutoff
 
int relayState;

int beenOn;

void setup(void)
{
  // Set up pins
  //pinMode(relayPin, OUTPUT);
  pinMode(LED_PIN, OUTPUT);

  //forceTurnCompressorOff(); //Ensure the compress is off

  tempSetPoint = 8.5; //Target temperature (can change this to use a potentiometer
  tempDelta = 0.25; //Temperature difference allowed before turning on / off the compressor
  //Current code is set to allow 5x this limit above the limit and one 1x below the limit. This is becuase
  //It is so much faster to cool down than the freezer takes to warm up, and because the walls take a while to release cold
  //Theroretically, the average temperature should be closer to the set point that way

  minTemp = 9999;
  maxTemp = -9999;
  beenOn = 0;
  lastCycledTime = 0;
  cycleDelta = 15 * 60 * 1000L;
  //cycleDelta = 30 * 1000L; //For testing
  
  // start serial port
  Serial.begin(9600);
  Serial.println("Kegerator controller");

  // Start up the library
  sensors.begin();
}
 
void loop(void)
{
  // call sensors.requestTemperatures() to issue a global temperature
  // request to all devices on the bus
  //Serial.print(" Requesting temperatures...");
  //sensors.requestTemperatures(); // Send the command to get temperatures
  //Serial.println("DONE");

  //Serial.print("Temperature is: ");
  //temperature = sensors.getTempCByIndex(0);
  //Serial.println(temperature);

  //Read the Dials setting
  Serial.print("Raw: ");
  Serial.print(getDialRaw());
  Serial.print("  Dial set temp: ");
  Serial.println(getDialTemperature());

//  if (minTemp > temperature) {
//    minTemp = temperature;
//  }
//
//  if (maxTemp < temperature){
//    maxTemp = temperature;
//  }
//
//  if (isCompressorOff()){
//    Serial.print("Compressor is off currently. Max temp before turn on is: ");
//    Serial.println(tempSetPoint + (2.0 * tempDelta));
//
//    //Check if the temperature is too warm
//    if (temperature > tempSetPoint + (2.0 * tempDelta) && isCompressorOff()){
//      //Turn the freezer on
//      turnCompressorOn();
//    }
//  }
//
//  if (isCompressorOn()){
//    Serial.print("Compressor is on currently. Min temp before turn off is: ");
//    Serial.println(tempSetPoint - tempDelta);
//
//    //Check if the temperature is too cold
//    if (temperature < tempSetPoint - tempDelta && isCompressorOn()){
//      //Turn the freezer off
//      turnCompressorOff(); 
//    }
//  }
//
//  Serial.print("Min temperature: ");
//  Serial.print(minTemp);
//  Serial.print("  Max temperature: ");
//  Serial.println(maxTemp);
//  
  delay(250 * 1);
}

void turnCompressorOn(){
  if (beenOn == 0 || millis() - lastCycledTime > cycleDelta){
    lastCycledTime = millis();
    relayState=LOW;
    beenOn = 1;
    digitalWrite(RELAY_PIN,relayState);
    digitalWrite(LED_PIN,!relayState);
    Serial.println("Compressor turned ON");
  }
  else {
    Serial.print("Compressor not turned on due to cycle limit. Millis: ");
    Serial.print(millis());
    Serial.print(" Last Cycled Time: ");
    Serial.println(lastCycledTime);
  }
}
 
void turnCompressorOff(){
  if (millis() - lastCycledTime > cycleDelta){
    lastCycledTime = millis();
    relayState=HIGH;
    digitalWrite(RELAY_PIN,relayState);
    digitalWrite(LED_PIN,!relayState);
    Serial.println("Compressor turned OFF");
  }
  else {
    Serial.print("Compressor not turned off due to cycle limit. Millis: ");
    Serial.print(millis());
    Serial.print(" Last Cycled Time: ");
    Serial.println(lastCycledTime);
  }
}

void forceTurnCompressorOff(){
  lastCycledTime = millis();
  relayState=HIGH;
  digitalWrite(RELAY_PIN,relayState);
  digitalWrite(LED_PIN,!relayState);
  Serial.println("Compressor turned OFF");
}

int getDialRaw(){
  return analogRead(DIAL_PIN);
}

float getDialTemperature(){
  return map(constrain(getDialRaw(), DIAL_HIGH, DIAL_LOW), DIAL_LOW, DIAL_HIGH, SET_TEMP_LOW, SET_TEMP_HIGH) / 10.0;
}
 
boolean isCompressorOff(){
  return (relayState==HIGH); 
}
 
boolean isCompressorOn(){
  return !isCompressorOff();
}
