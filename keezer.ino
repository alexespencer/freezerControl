// -----------------------------------
// Keezer Code - Control Freezer
// Author: Alex Spencer 
// Date: Nov 2017
// Version: 1.0
// -----------------------------------

/* Features:
    - Cycle on/off protection. This code will not cycle on or off the freezer too quickly to protect the compressor
        It will leave the freezer on (or off) for at least 15 minutes (configurable)
    - Will not create excessive events about the cycle on/off protection - only once every 5 minutes
    - Target temperature can be controlled via the Photon API / web-portal
    - Current temperature and set point can be read via the Photon API / web-portal
    - Min/max temperature can be read via API, and can also be reset. These persist in memory
    - Currently only supports ONE temperature probe - cannot guarantee which sensor will be used otherwise (to change this, it would be possible
        to obtain the sensors address and only use that)
    - Saves temperature set point into EEPROM (be careful, if you have data here already it will be deleted - update the EEPROM_ADDR below to change address used)
*/

// ---------------------------------- 
// Library declaration
// -----------------------------------

#include <OneWire.h>      // Needed to read DS18B20 temperature probe (Temperature can be read from: DS18S20, DS18B20, DS1822, DS2438)
#include "DS18.h"

// -----------------------------------
// PIN Configuration
// -----------------------------------

// Declare PINS
#define LED_PIN D7        // LED pin, useful to debug the state, as it matches the relay state
#define RELAY_PIN D2      // What pin is used to control the relay?
#define ONEWIRE_SIGNAL D4 // The pin used for the OneWire signal. A pull-up resistor is required on the signal line. The spec calls for 4.7K

// -----------------------------------
// Logging Configuration
// -----------------------------------

const int LOG_CYCLE = 0;         // How often the cycle time limit is displayed (waiting for status change)
const int LOG_TEMP = 1;          // How often the temperature is logged as an event
const int LOG_SENSOR = 2;        // How often the sensor error is logged if it occures

// -----------------------------------
// Variable declaration
// -----------------------------------

DS18 sensors(ONEWIRE_SIGNAL);                      // DS18 based sensor object

unsigned long lastCycledTime = 0;     // Stores the last time cycled
unsigned long cycleDelta = 15 * 60 * 1000L; // Stores the min allowed cycle time (10 minutes?) Default: 15 * 60 * 1000L

unsigned long lastLogTime[3] = {0, 0, 0};     // Stores the last time a log was created
unsigned long logDelta[3] = {5 * 60 * 1000L, 30 * 60 * 1000L, 10 * 60 * 1000L} ; // Stores the min allowed logging interval. See Logging Configuration for descriptions

double temperature = 7.5;            // Storage for the current temperature from the sensor

double minTemp = 9999;                // Tracks min and max temp
double maxTemp = -9999;

double tempSetPoint = 7.5;            // Target temperature
double tempDelta = 0.2;               // The delta for the temperature cutoff

int relayState;                       // Should the relay (freezer) current on or off?

int beenOn = 0;                       // Has the freezer been on

const int EEPROM_ADDR = 10;           // Where is the historic set temperature stored? This is used to remember the historic temperature 
                                      // after a power down situation.

// Storage for historic data
struct MyHistoricData {
      uint8_t version;
      double setTemp;
      char check[10];
      double minTemp;
      double maxTemp;
    };

// -----------------------------------
// Function declaration
// -----------------------------------

// Function to turn freezer on
void turnFreezerOn() {
    if (beenOn == 0 || millis() - lastCycledTime > cycleDelta) {
        forceTurnFreezerOn();
    }
    else {
        // Publish event with reason
        // Only log event if not logged within the last 5 minutes
        logEvent("Freezer not turned on: cycle limit", String(millis()), LOG_CYCLE);
    }
}

// Function to turn freezer off
void turnFreezerOff() {
    if (millis() - lastCycledTime > cycleDelta) {
        forceTurnFreezerOff();
    }
    else {
        // Publish event with reason
        // Only log event if not logged within the last 5 minutes
        logEvent("Freezer not turned off: cycle limit", String(millis()), LOG_CYCLE);
    }
}

// Function to force turn off freezer
void forceTurnFreezerOff() {
    lastCycledTime = millis();
    relayState = HIGH;
    digitalWrite(RELAY_PIN, !relayState);
    digitalWrite(LED_PIN, !relayState);
    
    Particle.publish("freezer-OFF", String(temperature), 60, PRIVATE); // Log an event
}

// Function to force turn on freezer
void forceTurnFreezerOn() {
    lastCycledTime = millis();
    relayState = LOW;
    beenOn = 1;
    digitalWrite(RELAY_PIN, !relayState);
    digitalWrite(LED_PIN, !relayState);
    
    Particle.publish("freezer-ON", String(temperature), 60, PRIVATE); // Log an event
}

// Function to log event, but within logging interval
void logEvent(String eventText, String misc, int logType) {
    if (millis() - lastLogTime[logType] > logDelta[logType]) {
        lastLogTime[logType] = millis();
        
        Particle.publish(eventText, misc, 60, PRIVATE); // Publish event as requested
    }
}

// Functions to return the state of the relay (freezer)
boolean isFreezerOff() {
    return (relayState == HIGH);
}

boolean isFreezerOn() {
    return !isFreezerOff();
}

// Update the min/max temperatures
void updateMinMaxTemp(double temp) {
    bool changed = false;
    if (minTemp > temp) {
        minTemp = temp;
        changed = true;
    }
    
    if (maxTemp < temp) {
        maxTemp = temp;
        changed = true;
    }
    
    if (changed) {
        writeEEPROM();      // Update the EEPROM data
    }
    
    // Log temperature as event
    logEvent("current-temperature", String(temperature), LOG_TEMP);
}

// Function to write to EEPROM and set temperature
void writeEEPROM() {
    // Save the temperature, with a check value for the variable name/version (in case people have data in the EEPROM already)
    MyHistoricData myData = { 1, tempSetPoint, "keezer", minTemp, maxTemp };
    EEPROM.put(EEPROM_ADDR, myData);
    
    Particle.publish("EEPROM-writeData", String(tempSetPoint), 60, PRIVATE); // Log an event
}

// Function to read from EEPROM and update temperature set point
void readEEPROM() {
    // Read the historic data
    MyHistoricData myData;
    EEPROM.get(EEPROM_ADDR, myData);
    if (myData.version == 1 && String(myData.check) == "keezer") {
      // Update temperature set point
      tempSetPoint = myData.setTemp;
      minTemp = myData.minTemp;
      maxTemp = myData.maxTemp;
      
      Particle.publish("EEPROM-readData", String(tempSetPoint), 60, PRIVATE); // Log an event
    }
    else {
        Particle.publish("EEPROM-errorVersion", String(myData.version), 60, PRIVATE); // Log an event
    }
}

// -----------------------------------
// Setup board
// -----------------------------------

void setup()
{
   // Declare pins
   pinMode(LED_PIN, OUTPUT);
   pinMode(RELAY_PIN, OUTPUT);

   // Create API function call backs
   //Particle.function("probeTemp", debugProbeTemp); // Uncomment this line to enable the ability to set the "temperature" yourself
   Particle.function("targetTemp", setTargetTemp);
   Particle.function("resetMinMax", resetMinMaxTemp);
   
   // Register the set beer temperature, current termperature and Min/Max temperature so they appear in the API
   Particle.variable("currentTemp", temperature);
   Particle.variable("targetTemp", tempSetPoint);
   Particle.variable("minTemp", minTemp);
   Particle.variable("maxTemp", maxTemp);
   Particle.variable("freezerOff", relayState);
   
   // Force turn the freezer off
   forceTurnFreezerOff(); //Ensure the compressor is off
   
   // Declare that we powered on
   Particle.publish("keezer-power-on", NULL, 60, PRIVATE);
   
   // Check memory for a historic temperature set point etc
   readEEPROM();
}

// -----------------------------------
// Main loop code
// -----------------------------------

void loop()
{
   // Read the temperature from probe - in debug mode, this is updated via the API
   control();
   
   // No need to loop quite so fast
   delay(1000 * 5); 
}

// -----------------------------------
// Main control code (read temperature and take action as needed)
// -----------------------------------

void control() {
    // Read the next available 1-Wire temperature sensor
    if (sensors.read()) {
        temperature = sensors.celsius();  // Store temperature (use the first temperature probe found by default)
        updateMinMaxTemp(temperature);   // Update the min/max temperatures
        
        //Check if the temperature is too warm
        if (isFreezerOff() && temperature > tempSetPoint + tempDelta) {
            //Turn the freezer on
            turnFreezerOn();
        }
        
        //Check if the temperature is too cold
        if (isFreezerOn() && temperature < tempSetPoint - tempDelta) {
            //Turn the freezer off
            turnFreezerOff();
        }
    }
    else {
        // Unable to read sensor - log if not logged in last 1 minute
        logEvent("sensor-read-error", String(NULL), LOG_SENSOR);
    }
}

// -----------------------------------
// API functions
// -----------------------------------

// This is a debug function, when a probe isn't connected and the code is commented out
// you can update the probe temperature reading whenever you want via the API
int debugProbeTemp(String command) {
    // Update the probe temperature

    if (command.toInt() == 0) {
        return -1;
    }
    else {
        temperature = command.toFloat() * 1.0;
        return 1;
    }
}

// This function is called via the API with the temperature you would like the beer to
// be if you do not want the default/saved value
int setTargetTemp(String command) {
    // Update the target temperature

    if (command.toInt() == 0) {
        return -1;
    }
    else {
        // Update the temperature set point now
        tempSetPoint = command.toFloat() * 1.0;
        Particle.publish("set-target-temperature", String(tempSetPoint), 60, PRIVATE); // Log event
        
        // Update the EEPROM for when we have a power cycle in the future
        writeEEPROM();
        
        return 1;
    }
}

// This function is called via the API and resets the min/max temperature values
int resetMinMaxTemp(String command) {
    // Update the min/max temperatures
    
    minTemp = 9999;                // Tracks min and max temp
    maxTemp = -9999;

    Particle.publish("reset-minmax-temperature", NULL, 60, PRIVATE); // Log event
    return 1;
}
