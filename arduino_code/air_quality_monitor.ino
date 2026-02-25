#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include "DHT.h"
#include <math.h>

#define DHTPIN 4
#define DHTTYPE DHT11

#define MQ135 34
#define MQ9   35
#define MQ8   32
#define DUST  33

#define FAN_RELAY   16
#define GREEN_LED 18
#define RED_LED   19
#define BUZZER 25

LiquidCrystal_I2C lcd(0x27, 16, 2);
DHT dht(DHTPIN, DHTTYPE);

float RL = 10.0;
float R0_MQ135 = 0;
float R0_MQ9   = 0;
float R0_MQ8   = 0;

unsigned long lastReadTime = 0;
unsigned long lastDisplayChange = 0;

const unsigned long readInterval = 120000;   // 2 min sensor update
const unsigned long displayInterval = 3000;  // 3 sec LCD rotate

int screen = 0;

float temp, hum;
float ppm135, ppm9, ppm8;
int dustValue;
int AQI;

int getFilteredValue(int pin) {
  long sum = 0;
  for(int i=0; i<20; i++){
    sum += analogRead(pin);
    delay(5);
  }
  return sum / 20;
}

// ===== Auto Calibration =====
float calibrateSensor(int pin, float cleanAirFactor) {

  float voltage = getFilteredValue(pin) * (3.3 / 4095.0);
  float Rs = (3.3 - voltage) / voltage * RL;
  return Rs / cleanAirFactor;
}

// ===== AQI (EPA simplified) =====
int calculateAQI(float ppm) {

  if (ppm <= 50) return map(ppm,0,50,0,50);
  else if (ppm <= 100) return map(ppm,51,100,51,100);
  else if (ppm <= 150) return map(ppm,101,150,101,150);
  else if (ppm <= 200) return map(ppm,151,200,151,200);
  else return map(ppm,201,500,201,500);
}

void setup() {

  Serial.begin(115200);

  pinMode(FAN_RELAY, OUTPUT);
  pinMode(GREEN_LED, OUTPUT);
  pinMode(RED_LED, OUTPUT);
  pinMode(BUZZER, OUTPUT);

  digitalWrite(FAN_RELAY, HIGH);

  dht.begin();
  lcd.init();
  lcd.backlight();

  lcd.print("Calibrating...");
  delay(10000);   // 10 sec clean air

  R0_MQ135 = calibrateSensor(MQ135, 3.6);
  R0_MQ9   = calibrateSensor(MQ9, 9.6);
  R0_MQ8   = calibrateSensor(MQ8, 9.0);

  lcd.clear();
  lcd.print("Calibration OK");
  delay(2000);
}

void loop() {

  if (millis() - lastReadTime >= readInterval) {

    lastReadTime = millis();

    temp = dht.readTemperature();
    hum  = dht.readHumidity();

    // MQ135
    float v135 = getFilteredValue(MQ135)*(3.3/4095.0);
    float Rs135 = (3.3 - v135)/v135*RL;
    float ratio135 = Rs135/R0_MQ135;
    ppm135 = 116.6020682 * pow(ratio135, -2.769034857);

    // MQ9
    float v9 = getFilteredValue(MQ9)*(3.3/4095.0);
    float Rs9 = (3.3 - v9)/v9*RL;
    float ratio9 = Rs9/R0_MQ9;
    ppm9 = 1000 * pow(ratio9, -1.5);

    // MQ8
    float v8 = getFilteredValue(MQ8)*(3.3/4095.0);
    float Rs8 = (3.3 - v8)/v8*RL;
    float ratio8 = Rs8/R0_MQ8;
    ppm8 = 1000 * pow(ratio8, -1.4);

    dustValue = getFilteredValue(DUST);

    AQI = calculateAQI(ppm135);

    // Automation
    if (AQI > 150) {
      digitalWrite(FAN_RELAY, LOW);
      digitalWrite(RED_LED, HIGH);
      digitalWrite(GREEN_LED, LOW);
      digitalWrite(BUZZER, HIGH);
      delay(1000);
      digitalWrite(BUZZER, LOW);
    }
    else {
      digitalWrite(FAN_RELAY, HIGH);
      digitalWrite(GREEN_LED, HIGH);
      digitalWrite(RED_LED, LOW);
    }
  }

  // ===== LCD Rotation =====
  if (millis() - lastDisplayChange >= displayInterval) {

    lastDisplayChange = millis();
    lcd.clear();

    switch(screen) {

      case 0:
        lcd.print("Temp:");
        lcd.print(temp);
        lcd.print("C");
        lcd.setCursor(0,1);
        lcd.print("Hum:");
        lcd.print(hum);
        lcd.print("%");
        break;

      case 1:
        lcd.print("MQ135 ppm:");
        lcd.print(ppm135);
        break;

      case 2:
        lcd.print("MQ9 ppm:");
        lcd.print(ppm9);
        break;

      case 3:
        lcd.print("MQ8 ppm:");
        lcd.print(ppm8);
        break;

      case 4:
        lcd.print("Dust:");
        lcd.print(dustValue);
        break;

      case 5:
        lcd.print("AQI:");
        lcd.print(AQI);
        break;
    }

    screen++;
    if(screen > 5) screen = 0;
  }
}
