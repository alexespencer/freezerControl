#include <OneWire.h>
#include <DallasTemperature.h>
#include <LiquidCrystal.h>

//********************PIN CONFIG************************
// Data wire is plugged into pin 8 on the Arduino
#define ONE_WIRE_BUS 8 // Pin 2 is used by LCD now
#define RELAY_PIN 9    // Pin controlling the relay      
#define LED_PIN 13     // LED pin
#define DIAL_PIN A1    // Analog pin to read Dial setting
#define SWITCH_3WAY A0 // Analogue PIN for the 3 way switch
//******************************************************

//********************PARAMETERS************************
#define DIAL_LOW 1020
#define DIAL_HIGH 50

#define SET_TEMP_LOW 0      // Min temperature (gets divided by 10)
#define SET_TEMP_HIGH 200   // Max temperature (gets divided by 10)

#define SET_DELTA_LOW 0      // Min temperature (gets divided by 1000)
#define SET_DELTA_HIGH 1000    // Max temperature (gets divided by 1000)
//******************************************************

// Declare Liquid Crystal Display
LiquidCrystal lcd(7, 6, 5, 4, 3, 2); //Using 1602A LCD screen

byte customChar[8] = {
  0b00000,
  0b00100,
  0b01110,
  0b00100,
  0b00000,
  0b01110,
  0b00000,
  0b00000
};

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
  // Initialise LCD display
  lcd.begin(16, 2);
  lcd.createChar(0, customChar); // Register the plus/minus sign
  
  // Set up pins
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);

  forceTurnCompressorOff(); //Ensure the compressor is off

  tempSetPoint = 8.5; // Target temperature (can change this to use a potentiometer)
  tempDelta = 0.25;   // Temperature difference allowed before turning on / off the compressor

  minTemp = 9999;
  maxTemp = -9999;
  beenOn = 0;
  lastCycledTime = 0;
  cycleDelta = 15 * 60 * 1000L;
  //cycleDelta = 5 * 1000L; // For testing

  // start serial port
  Serial.begin(9600);
  Serial.println("Kegerator controller");

  // Start up the library
  sensors.begin();
}

void loop(void)
{
  // Call the control method, to turn on or off the freezer depending upon the setting
  control();

  // Update the LCD display
  lcdUpdate();

  // Debug, print to serial
  //debug();

  delay(100 * 1); // No need to loop quite so fast
}

void debug() {
  Serial.print("Temperature is: ");
  Serial.println(temperature);

  Serial.print("Min temperature: ");
  Serial.print(minTemp);
  Serial.print("   Max temperature: ");
  Serial.println(maxTemp);

  Serial.print("Temperature set point: ");
  Serial.println(tempSetPoint);

  Serial.print("Temperature delta: ");
  Serial.println(tempDelta);
}

void turnCompressorOn() {
  if (beenOn == 0 || millis() - lastCycledTime > cycleDelta) {
    forceTurnCompressorOn();
  }
  else {
    Serial.print("Compressor not turned on due to cycle limit. Millis: ");
    Serial.print(millis());
    Serial.print(" Last Cycled Time: ");
    Serial.println(lastCycledTime);
  }
}

void turnCompressorOff() {
  if (millis() - lastCycledTime > cycleDelta) {
    forceTurnCompressorOff();
  }
  else {
    Serial.print("Compressor not turned off due to cycle limit. Millis: ");
    Serial.print(millis());
    Serial.print(" Last Cycled Time: ");
    Serial.println(lastCycledTime);
  }
}

void forceTurnCompressorOff() {
  lastCycledTime = millis();
  relayState = HIGH;
  digitalWrite(RELAY_PIN, !relayState);
  digitalWrite(LED_PIN, !relayState);
  Serial.println("Compressor turned OFF");
}

void forceTurnCompressorOn() {
  lastCycledTime = millis();
  relayState = LOW;
  beenOn = 1;
  digitalWrite(RELAY_PIN, !relayState);
  digitalWrite(LED_PIN, !relayState);
  Serial.println("Compressor turned ON");
}

int getDialRaw() {
  return analogRead(DIAL_PIN);
}

float getDialTemperature() {
  return map(constrain(getDialRaw(), DIAL_HIGH, DIAL_LOW), DIAL_LOW, DIAL_HIGH, SET_TEMP_LOW, SET_TEMP_HIGH) / 10.0;
}

float getDialDelta() {
  return map(constrain(getDialRaw(), DIAL_HIGH, DIAL_LOW), DIAL_LOW, DIAL_HIGH, SET_DELTA_LOW, SET_DELTA_HIGH) / 1000.0;
}

boolean isCompressorOff() {
  return (relayState == HIGH);
}

boolean isCompressorOn() {
  return !isCompressorOff();
}

void lcdUpdate() {
  lcd.clear(); // Set cursor to first row left and clear screen
  
  int mode = switchSetting();

  if (mode == 2) {          // Freezer mode
    lcd.print("FREEZER mode");
    lcd.setCursor(0, 2); // Set cursor to second row left
    lcd.print("Temp: ");
    lcd.print(temperature);
  }
  else if (mode == 0) {     // Off
    lcd.print("OFF. Temp: ");
    lcd.print(temperature);
    lcd.setCursor(0, 2); // Set cursor to second row left
    lcd.print("Delta: ");
    lcd.print(tempDelta);
  }
  else if (mode == 1) {     // Control mode
    lcd.print("Auto-Temp: ");
    lcd.print(temperature);
    lcd.setCursor(0, 2); // Set cursor to second row left
    lcd.print("Set: ");
    lcd.print(tempSetPoint);
    lcd.write((uint8_t)0); // Send the plus/minus sign to the LCD
    lcd.print(tempDelta);
  }

  lcd.setCursor(0, 2); // Set cursor to second row left
}

int switchSetting() {
  int analogValue = analogRead(SWITCH_3WAY);

  int actualState;
  if (analogValue < 100) actualState = 1;
  else if (analogValue < 900) actualState = 2;
  else actualState = 0;

  return actualState;
}

void updateMinMaxTemp(float temp) {
  if (minTemp > temp) {
    minTemp = temp;
  }

  if (maxTemp < temp) {
    maxTemp = temp;
  }
}

void control() {
  // call sensors.requestTemperatures() to issue a global temperature
  // request to all devices on the bus

  sensors.requestTemperatures(); // Send the command to get temperatures
  temperature = sensors.getTempCByIndex(0); // Store temperature
  updateMinMaxTemp(temperature);

  // Get the current switch setting
  // 0 = Freezer Off (also allows the dial to adjust the delta)
  // 1 = Control mode, aims for the target temperature (dial adjusts target temp)
  // 2 = Freezer mode

  int mode = switchSetting();

  if (mode == 2) {          // Freezer mode
    if (isCompressorOff()) turnCompressorOn();
  }
  else if (mode == 0) {     // Off
    if (isCompressorOn()) turnCompressorOff();

    //Use dial to adjust delta
    tempDelta = getDialDelta();

  }
  else if (mode == 1) {     // Control mode
    //Use dial to adjust target temperature
    tempSetPoint = getDialTemperature();


    if (isCompressorOff()) {
      Serial.print("Compressor is off currently. Max temp before turn on is: ");
      Serial.println(tempSetPoint + tempDelta);

      //Check if the temperature is too warm
      if (temperature > tempSetPoint + (tempDelta) && isCompressorOff()) {
        //Turn the freezer on
        turnCompressorOn();
      }
    }

    if (isCompressorOn()) {
      Serial.print("Compressor is on currently. Min temp before turn off is: ");
      Serial.println(tempSetPoint - tempDelta);

      //Check if the temperature is too cold
      if (temperature < tempSetPoint - tempDelta && isCompressorOn()) {
        //Turn the freezer off
        turnCompressorOff();
      }
    }
  }
}
