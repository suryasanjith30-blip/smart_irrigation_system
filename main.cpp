/*
  ============================================================
             SMART IRRIGATION SYSTEM — ESP32
             WOKWI / PLATFORMIO VERSION
  ============================================================

  PIN MAP
  ------------------------------------------------------------
  GPIO34  Soil Moisture A       Analog
  GPIO35  Soil Moisture B       Analog
  GPIO32  LDR                   Analog
  GPIO33  Tank Level            Analog

  GPIO21  LCD SDA
  GPIO22  LCD SCL

  GPIO25  Relay A / Pump A
  GPIO26  Relay B / Pump B

  GPIO27  Button A
  GPIO16  Button B
  GPIO17  Buzzer Silence / Reset Button

  GPIO14  Rain Sensor DO
  GPIO13  Buzzer
  GPIO4   DHT22 in Wokwi
  GPIO18  Servo Valve

  IMPORTANT:
  In physical hardware, if you use DHT11,
  change DHT_TYPE from DHT22 to DHT11.
  ============================================================
*/

#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <DHT.h>
#include <ESP32Servo.h>


// ============================================================
// PIN DEFINITIONS
// ============================================================

#define SOIL_A_PIN       34
#define SOIL_B_PIN       35
#define LDR_PIN          32
#define TANK_PIN         33

#define LCD_SDA          21
#define LCD_SCL          22

#define RELAY_A_PIN      25
#define RELAY_B_PIN      26

#define BUTTON_A_PIN     27
#define BUTTON_B_PIN     16
#define BUZZER_BTN_PIN   17

#define RAIN_PIN         14
#define BUZZER_PIN       13

#define DHT_PIN          4
#define SERVO_PIN        18


// ============================================================
// OBJECTS
// ============================================================

LiquidCrystal_I2C lcd(0x27, 16, 2);

// Wokwi uses DHT22
#define DHT_TYPE DHT22
DHT dht(DHT_PIN, DHT_TYPE);

Servo tankServo;


// ============================================================
// RELAY LOGIC
// ============================================================

// Relay modules are ACTIVE LOW
#define RELAY_ON  LOW
#define RELAY_OFF HIGH


// ============================================================
// SERVO POSITIONS
// ============================================================

const int VALVE_CLOSED = 0;
const int VALVE_OPEN   = 90;


// ============================================================
// SOIL SETTINGS
// ============================================================

const int SOIL_DRY_LIMIT = 2500;


// ============================================================
// TANK SETTINGS
// ============================================================

const int TANK_LOW_LIMIT  = 1000;
const int TANK_LOW_CLEAR  = 1200;

const int TANK_FULL_LIMIT = 3000;
const int TANK_FULL_CLEAR = 2800;


// ============================================================
// TIMING
// ============================================================

const unsigned long FILL_TIMEOUT     = 60000UL;
const unsigned long DHT_INTERVAL     = 2000UL;
const unsigned long LCD_INTERVAL     = 500UL;
const unsigned long DEBOUNCE_TIME    = 50UL;
const unsigned long LONG_PRESS_TIME  = 2000UL;
const unsigned long BUZZER_INTERVAL  = 500UL;


// ============================================================
// LCD PAGE SETTINGS
// ============================================================

// LCD alternates between normal information
// and temperature/humidity information.

const unsigned long LCD_PAGE_INTERVAL = 2500UL;

bool lcdPage = false;
unsigned long lastLcdPageChange = 0;


// ============================================================
// TANK STATE
// ============================================================

enum TankState
{
  TANK_STATE_LOW,
  TANK_STATE_NORMAL,
  TANK_STATE_FULL
};

TankState tankState = TANK_STATE_NORMAL;
TankState previousTankState = TANK_STATE_NORMAL;


// ============================================================
// SENSOR VARIABLES
// ============================================================

int soilA = 0;
int soilB = 0;
int ldrValue = 0;
int tankLevel = 0;

float temperature = 0;
float humidity = 0;

bool raining = false;

bool dhtOK = true;
bool previousDhtFault = false;


// ============================================================
// PUMP VARIABLES
// ============================================================

bool pumpA = false;
bool pumpB = false;

bool manualA = false;
bool manualB = false;


// ============================================================
// VALVE / FILL VARIABLES
// ============================================================

bool valveOpen = false;
bool fillFault = false;

unsigned long valveOpenTime = 0;


// ============================================================
// BUZZER VARIABLES
// ============================================================

bool buzzerSilenced = false;
bool buzzerState = false;

unsigned long lastBuzzerChange = 0;


// ============================================================
// SENSOR / LCD TIMERS
// ============================================================

unsigned long lastDhtRead = 0;
unsigned long lastLcdUpdate = 0;


// ============================================================
// BUTTON VARIABLES
// ============================================================

bool lastButtonA = HIGH;
bool lastButtonB = HIGH;
bool lastBuzzerButton = HIGH;

unsigned long buttonAPressTime = 0;
unsigned long buttonBPressTime = 0;
unsigned long buzzerButtonPressTime = 0;

bool buttonAHandled = false;
bool buttonBHandled = false;


// ============================================================
// FUNCTION DECLARATIONS
// ============================================================

void readSensors();
void readDHT();

void determineTankState();
void handleTank();

void openTankValve();
void closeTankValve();

void handlePumps();
void setPumpA(bool state);
void setPumpB(bool state);

void handleButtons();
void handleBuzzerButton();

void handleBuzzer();

void updateLCD();


// ============================================================
// SETUP
// ============================================================

void setup()
{
  Serial.begin(115200);


  // ==========================================================
  // INPUTS
  // ==========================================================

  pinMode(SOIL_A_PIN, INPUT);
  pinMode(SOIL_B_PIN, INPUT);

  pinMode(LDR_PIN, INPUT);
  pinMode(TANK_PIN, INPUT);

  /*
    IMPORTANT FIX:

    Rain sensor is connected between GPIO14 and GND.

    INPUT_PULLUP gives:

    Button released -> HIGH -> NO RAIN
    Button pressed  -> LOW  -> RAIN
  */
  pinMode(RAIN_PIN, INPUT_PULLUP);

  // Manual buttons
  pinMode(BUTTON_A_PIN, INPUT_PULLUP);
  pinMode(BUTTON_B_PIN, INPUT_PULLUP);

  // Buzzer silence/reset button
  pinMode(BUZZER_BTN_PIN, INPUT_PULLUP);


  // ==========================================================
  // OUTPUTS
  // ==========================================================

  pinMode(RELAY_A_PIN, OUTPUT);
  pinMode(RELAY_B_PIN, OUTPUT);

  pinMode(BUZZER_PIN, OUTPUT);


  // ==========================================================
  // SAFETY STARTUP
  // ==========================================================

  digitalWrite(RELAY_A_PIN, RELAY_OFF);
  digitalWrite(RELAY_B_PIN, RELAY_OFF);

  digitalWrite(BUZZER_PIN, LOW);


  // ==========================================================
  // LCD
  // ==========================================================

  Wire.begin(LCD_SDA, LCD_SCL);

  lcd.init();
  lcd.backlight();

  lcd.clear();

  lcd.setCursor(0, 0);
  lcd.print("SMART IRRIGATION");

  lcd.setCursor(0, 1);
  lcd.print("Starting...");

  delay(1500);


  // ==========================================================
  // DHT
  // ==========================================================

  dht.begin();


  // ==========================================================
  // SERVO
  // ==========================================================

  tankServo.attach(SERVO_PIN);

  // Valve must start CLOSED
  tankServo.write(VALVE_CLOSED);

  valveOpen = false;

  delay(500);

  lcd.clear();


  // ==========================================================
  // SERIAL START MESSAGE
  // ==========================================================

  Serial.println();
  Serial.println("=================================");
  Serial.println(" SMART IRRIGATION SYSTEM");
  Serial.println(" ESP32 STARTED");
  Serial.println("=================================");
}


// ============================================================
// MAIN LOOP
// ============================================================

void loop()
{
  // 1. Read all sensors
  readSensors();

  // 2. Determine tank condition
  determineTankState();

  // 3. Handle tank and valve
  handleTank();

  // 4. Handle buttons
  handleButtons();

  // 5. Control pumps
  handlePumps();

  // 6. Handle warning buzzer
  handleBuzzer();

  // 7. Update LCD
  updateLCD();

  delay(20);
}


// ============================================================
// READ SENSORS
// ============================================================

void readSensors()
{
  // Analog sensors
  soilA = analogRead(SOIL_A_PIN);
  soilB = analogRead(SOIL_B_PIN);

  ldrValue = analogRead(LDR_PIN);
  tankLevel = analogRead(TANK_PIN);


  // ==========================================================
  // RAIN SENSOR
  // ==========================================================

  /*
    Because INPUT_PULLUP is used:

    LOW  = Rain detected
    HIGH = No rain
  */

  raining = (digitalRead(RAIN_PIN) == LOW);


  // Read temperature and humidity
  readDHT();


  // ==========================================================
  // SERIAL MONITOR
  // ==========================================================

  Serial.print("Temperature: ");

  if (dhtOK)
    Serial.print(temperature);
  else
    Serial.print("ERROR");

  Serial.print(" C | Humidity: ");

  if (dhtOK)
    Serial.print(humidity);
  else
    Serial.print("ERROR");

  Serial.print(" %");

  Serial.print(" | Soil A: ");
  Serial.print(soilA);

  Serial.print(" | Soil B: ");
  Serial.print(soilB);

  Serial.print(" | Tank: ");
  Serial.print(tankLevel);

  Serial.print(" | LDR: ");
  Serial.print(ldrValue);

  Serial.print(" | Rain: ");
  Serial.print(raining ? "YES" : "NO");

  Serial.print(" | Pump A: ");
  Serial.print(pumpA ? "ON" : "OFF");

  Serial.print(" | Pump B: ");
  Serial.print(pumpB ? "ON" : "OFF");

  Serial.println();
}


// ============================================================
// READ DHT
// ============================================================

void readDHT()
{
  unsigned long now = millis();

  // DHT should not be read continuously
  if (now - lastDhtRead < DHT_INTERVAL)
    return;

  lastDhtRead = now;


  float newHumidity = dht.readHumidity();
  float newTemperature = dht.readTemperature();


  // Check for invalid sensor readings
  if (isnan(newHumidity) || isnan(newTemperature))
  {
    dhtOK = false;

    Serial.println("DHT SENSOR ERROR");

    return;
  }


  // Save valid readings
  humidity = newHumidity;
  temperature = newTemperature;

  dhtOK = true;
}


// ============================================================
// DETERMINE TANK STATE
// ============================================================

void determineTankState()
{
  previousTankState = tankState;


  switch (tankState)
  {
    // ========================================================
    // TANK LOW
    // ========================================================

    case TANK_STATE_LOW:

      if (tankLevel >= TANK_FULL_LIMIT)
      {
        tankState = TANK_STATE_FULL;
      }
      else if (tankLevel >= TANK_LOW_CLEAR)
      {
        tankState = TANK_STATE_NORMAL;
      }

      break;


    // ========================================================
    // TANK NORMAL
    // ========================================================

    case TANK_STATE_NORMAL:

      if (tankLevel < TANK_LOW_LIMIT)
      {
        tankState = TANK_STATE_LOW;
      }
      else if (tankLevel >= TANK_FULL_LIMIT)
      {
        tankState = TANK_STATE_FULL;
      }

      break;


    // ========================================================
    // TANK FULL
    // ========================================================

    case TANK_STATE_FULL:

      if (tankLevel < TANK_LOW_LIMIT)
      {
        tankState = TANK_STATE_LOW;
      }
      else if (tankLevel < TANK_FULL_CLEAR)
      {
        tankState = TANK_STATE_NORMAL;
      }

      break;
  }
}


// ============================================================
// TANK CONTROL
// ============================================================

void handleTank()
{
  // ==========================================================
  // NEW LOW TANK EVENT
  // ==========================================================

  if (
    tankState == TANK_STATE_LOW &&
    previousTankState != TANK_STATE_LOW
  )
  {
    // Start warning
    buzzerSilenced = false;

    // Allow a new filling attempt
    fillFault = false;

    openTankValve();

    Serial.println("TANK LOW -> VALVE OPEN");
  }


  // ==========================================================
  // TANK LOW
  // ==========================================================

  if (tankState == TANK_STATE_LOW)
  {
    // Pumps must remain OFF
    pumpA = false;
    pumpB = false;

    if (!valveOpen && !fillFault)
    {
      openTankValve();
    }
  }


  // ==========================================================
  // TANK FULL
  // ==========================================================

  if (tankState == TANK_STATE_FULL)
  {
    closeTankValve();

    // Filling successful
    fillFault = false;

    buzzerSilenced = false;

    Serial.println("TANK FULL -> VALVE CLOSED");
  }


  // ==========================================================
  // FILL TIMEOUT
  // ==========================================================

  if (
    valveOpen &&
    tankState != TANK_STATE_FULL &&
    !fillFault
  )
  {
    unsigned long now = millis();

    if (now - valveOpenTime >= FILL_TIMEOUT)
    {
      closeTankValve();

      fillFault = true;

      pumpA = false;
      pumpB = false;

      buzzerSilenced = false;

      Serial.println("FILL TIMEOUT!");
    }
  }
}


// ============================================================
// OPEN TANK VALVE
// ============================================================

void openTankValve()
{
  if (valveOpen)
    return;

  tankServo.write(VALVE_OPEN);

  valveOpen = true;

  valveOpenTime = millis();

  Serial.println("Valve OPEN");
}


// ============================================================
// CLOSE TANK VALVE
// ============================================================

void closeTankValve()
{
  if (!valveOpen)
    return;

  tankServo.write(VALVE_CLOSED);

  valveOpen = false;

  Serial.println("Valve CLOSED");
}


// ============================================================
// PUMP CONTROL
// ============================================================

void handlePumps()
{
  // ==========================================================
  // TANK LOW = HIGHEST PRIORITY
  // ==========================================================

  if (tankState == TANK_STATE_LOW)
  {
    setPumpA(false);
    setPumpB(false);

    return;
  }


  // ==========================================================
  // FILL FAULT
  // ==========================================================

  if (fillFault)
  {
    setPumpA(false);
    setPumpB(false);

    return;
  }


  // ==========================================================
  // PUMP A
  // ==========================================================

  if (manualA)
  {
    // Manual mode
    setPumpA(true);
  }
  else
  {
    // Automatic mode

    bool soilADry = (soilA >= SOIL_DRY_LIMIT);

    /*
      Pump A turns ON only when:

      1. Soil A is dry
      2. It is NOT raining
    */

    bool automaticA =
      !raining &&
      soilADry;

    setPumpA(automaticA);
  }


  // ==========================================================
  // PUMP B
  // ==========================================================

  if (manualB)
  {
    // Manual mode
    setPumpB(true);
  }
  else
  {
    // Automatic mode

    bool soilBDry = (soilB >= SOIL_DRY_LIMIT);

    /*
      Pump B turns ON only when:

      1. Soil B is dry
      2. It is NOT raining
    */

    bool automaticB =
      !raining &&
      soilBDry;

    setPumpB(automaticB);
  }
}


// ============================================================
// SET PUMP A
// ============================================================

void setPumpA(bool state)
{
  pumpA = state;

  digitalWrite(
    RELAY_A_PIN,
    state ? RELAY_ON : RELAY_OFF
  );
}


// ============================================================
// SET PUMP B
// ============================================================

void setPumpB(bool state)
{
  pumpB = state;

  digitalWrite(
    RELAY_B_PIN,
    state ? RELAY_ON : RELAY_OFF
  );
}


// ============================================================
// BUTTON HANDLING
// ============================================================

void handleButtons()
{
  unsigned long now = millis();


  // ==========================================================
  // BUTTON A
  // ==========================================================

  bool currentA = digitalRead(BUTTON_A_PIN);


  // Button pressed
  if (
    lastButtonA == HIGH &&
    currentA == LOW
  )
  {
    buttonAPressTime = now;
    buttonAHandled = false;
  }


  // Debounce
  if (
    currentA == LOW &&
    !buttonAHandled &&
    now - buttonAPressTime >= DEBOUNCE_TIME
  )
  {
    manualA = !manualA;

    buttonAHandled = true;

    Serial.print("Manual A: ");
    Serial.println(manualA ? "ON" : "OFF");
  }


  lastButtonA = currentA;


  // ==========================================================
  // BUTTON B
  // ==========================================================

  bool currentB = digitalRead(BUTTON_B_PIN);


  // Button pressed
  if (
    lastButtonB == HIGH &&
    currentB == LOW
  )
  {
    buttonBPressTime = now;
    buttonBHandled = false;
  }


  // Debounce
  if (
    currentB == LOW &&
    !buttonBHandled &&
    now - buttonBPressTime >= DEBOUNCE_TIME
  )
  {
    manualB = !manualB;

    buttonBHandled = true;

    Serial.print("Manual B: ");
    Serial.println(manualB ? "ON" : "OFF");
  }


  lastButtonB = currentB;


  // ==========================================================
  // BUZZER BUTTON
  // ==========================================================

  handleBuzzerButton();
}


// ============================================================
// BUZZER BUTTON
//
// SHORT PRESS:
// Silence warning
//
// LONG PRESS >= 2 SEC:
// Reset fill fault
// ============================================================

void handleBuzzerButton()
{
  unsigned long now = millis();

  bool current = digitalRead(BUZZER_BTN_PIN);


  // ==========================================================
  // BUTTON PRESSED
  // ==========================================================

  if (
    lastBuzzerButton == HIGH &&
    current == LOW
  )
  {
    buzzerButtonPressTime = now;
  }


  // ==========================================================
  // BUTTON RELEASED
  // ==========================================================

  if (
    lastBuzzerButton == LOW &&
    current == HIGH
  )
  {
    unsigned long pressDuration =
      now - buzzerButtonPressTime;


    // ========================================================
    // LONG PRESS
    // ========================================================

    if (pressDuration >= LONG_PRESS_TIME)
    {
      if (fillFault)
      {
        fillFault = false;

        buzzerSilenced = false;

        Serial.println("FILL FAULT RESET");


        if (tankState == TANK_STATE_LOW)
        {
          openTankValve();

          Serial.println("FILL RETRY");
        }
      }
      else if (
        tankState == TANK_STATE_LOW ||
        !dhtOK
      )
      {
        buzzerSilenced = false;
      }
    }


    // ========================================================
    // SHORT PRESS
    // ========================================================

    else if (pressDuration >= DEBOUNCE_TIME)
    {
      if (
        fillFault ||
        tankState == TANK_STATE_LOW ||
        !dhtOK
      )
      {
        buzzerSilenced = true;

        Serial.println("BUZZER SILENCED");
      }
    }
  }


  lastBuzzerButton = current;
}


// ============================================================
// BUZZER CONTROL
// ============================================================

void handleBuzzer()
{
  bool warningActive =
    fillFault ||
    tankState == TANK_STATE_LOW ||
    !dhtOK;


  // ==========================================================
  // NO WARNING
  // ==========================================================

  if (!warningActive)
  {
    digitalWrite(BUZZER_PIN, LOW);

    buzzerState = false;

    buzzerSilenced = false;

    return;
  }


  // ==========================================================
  // NEW DHT ERROR
  // ==========================================================

  if (
    !dhtOK &&
    !previousDhtFault
  )
  {
    buzzerSilenced = false;

    lastBuzzerChange = millis();
  }

  previousDhtFault = !dhtOK;


  // ==========================================================
  // SILENCED
  // ==========================================================

  if (buzzerSilenced)
  {
    digitalWrite(BUZZER_PIN, LOW);

    buzzerState = false;

    return;
  }


  // ==========================================================
  // BEEP EVERY 500ms
  // ==========================================================

  unsigned long now = millis();


  if (now - lastBuzzerChange >= BUZZER_INTERVAL)
  {
    lastBuzzerChange = now;

    buzzerState = !buzzerState;

    digitalWrite(
      BUZZER_PIN,
      buzzerState ? HIGH : LOW
    );
  }
}


// ============================================================
// LCD
// ============================================================

void updateLCD()
{
  unsigned long now = millis();


  // Update LCD every 500 ms
  if (now - lastLcdUpdate < LCD_INTERVAL)
    return;

  lastLcdUpdate = now;


  // ==========================================================
  // CHANGE NORMAL LCD PAGE
  // ==========================================================

  if (now - lastLcdPageChange >= LCD_PAGE_INTERVAL)
  {
    lastLcdPageChange = now;

    lcdPage = !lcdPage;
  }


  lcd.clear();


  // ==========================================================
  // FILL FAULT
  // ==========================================================

  if (fillFault)
  {
    lcd.setCursor(0, 0);
    lcd.print("FILL TIMEOUT");

    lcd.setCursor(0, 1);
    lcd.print("LONG PRESS=RST");

    return;
  }


  // ==========================================================
  // TANK LOW
  // ==========================================================

  if (tankState == TANK_STATE_LOW)
  {
    lcd.setCursor(0, 0);
    lcd.print("TANK LOW!");

    lcd.setCursor(0, 1);

    if (valveOpen)
      lcd.print("PUMPS OFF FILL");
    else
      lcd.print("PUMPS OFF");

    return;
  }


  // ==========================================================
  // TANK FULL
  // ==========================================================

  if (tankState == TANK_STATE_FULL)
  {
    lcd.setCursor(0, 0);
    lcd.print("TANK FULL");

    lcd.setCursor(0, 1);
    lcd.print("VALVE CLOSED");

    return;
  }


  // ==========================================================
  // DHT ERROR
  // ==========================================================

  if (!dhtOK)
  {
    lcd.setCursor(0, 0);
    lcd.print("DHT SENSOR ERR");

    lcd.setCursor(0, 1);

    lcd.print("A:");
    lcd.print(pumpA ? "ON " : "OFF");

    lcd.print(" B:");
    lcd.print(pumpB ? "ON" : "OFF");

    return;
  }


  // ==========================================================
  // RAIN DETECTED
  // ==========================================================

  if (raining)
  {
    lcd.setCursor(0, 0);

    if (manualA && manualB)
      lcd.print("RAIN A+B MAN");

    else if (manualA)
      lcd.print("RAIN + A MAN");

    else if (manualB)
      lcd.print("RAIN + B MAN");

    else
      lcd.print("RAIN DETECTED");


    lcd.setCursor(0, 1);

    lcd.print("A:");
    lcd.print(pumpA ? "ON " : "OFF");

    lcd.print(" B:");
    lcd.print(pumpB ? "ON" : "OFF");

    return;
  }


  // ==========================================================
  // NORMAL PAGE 1
  // PUMP + SOIL
  // ==========================================================

  if (!lcdPage)
  {
    lcd.setCursor(0, 0);

    lcd.print("A:");
    lcd.print(pumpA ? "ON " : "OFF");

    lcd.print(" B:");
    lcd.print(pumpB ? "ON" : "OFF");


    lcd.setCursor(0, 1);

    lcd.print("S");
    lcd.print(soilA);

    lcd.print(" ");
    lcd.print(soilB);

    return;
  }


  // ==========================================================
  // NORMAL PAGE 2
  // TEMPERATURE + HUMIDITY
  // ==========================================================

  lcd.setCursor(0, 0);

  lcd.print("T:");
  lcd.print(temperature, 1);
  lcd.print("C H:");
  lcd.print(humidity, 1);
  lcd.print("%");


  lcd.setCursor(0, 1);

  lcd.print("TEMP & HUMIDITY");
}