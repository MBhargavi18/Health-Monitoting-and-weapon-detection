#include <Wire.h>
#include <LiquidCrystal.h>
#include <DHT.h>
#include <MAX30105.h>
#include <SoftwareSerial.h>

SoftwareSerial wifi_serial(2, 3); // RX, TX

// ---------- LCD ----------
LiquidCrystal lcd(13, 12, 11, 10, 9, 8);

// ---------- DHT ----------
#define DHTPIN 6
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

// ---------- Buzzer ----------
#define BUZZER 7

// ---------- MAX30105 ----------
MAX30105 particleSensor;

// ---------- Variables ----------
int pulse = 0;
int spo2 = 0;
float temperature = 0;
bool emergency = false;
unsigned long lastAlert = 0;

// ---------- Weapon Detection ----------
int receivedValue = 0;
bool weaponAlertSent = false;

// Variables for incoming wifi_serial commands
String inputString = "";
char gchr, rcr, rcv;
bool stringComplete = false;
int sti = 0;

void okcheck() {
  while (true) {
    if (wifi_serial.available() > 0) {
      if (wifi_serial.read() == 'K') {
        return;
      }
    }
    delay(100);
  }
}

void sendcommand(const String& command, int delayMs = 500) {
  wifi_serial.write(command.c_str());
  wifi_serial.write("\r\n");
  okcheck();
  delay(delayMs);
}

void send_telnet_alert(const String& message) {
  wifi_serial.write("AT+CIPSEND=0,");
  wifi_serial.write(String(message.length() + 2).c_str());
  wifi_serial.write("\r\n");
  delay(2000);
  wifi_serial.println(message);
  wifi_serial.write("\r\n");
  delay(3000);
}

// ---------- Simulated values ----------
int measurePulse() {
  return random(70, 95);
}

int measureSpO2() {
  return random(90, 100);
}

void wifi_init() {

  sendcommand("AT");
  sendcommand("ATE0");
  sendcommand("AT+CWMODE=2");
  sendcommand("AT+CIPMUX=1");
  sendcommand("AT+CIPSERVER=1,23");

  lcd.clear();
  lcd.print("Waiting For");
  lcd.setCursor(0, 1);
  lcd.print("Connection");

  while (true) {
    rcv = wifi_serial.read();
    if (rcv == 'C') {
      break;
    }
  }

  lcd.clear();
  lcd.print("Connected");
  delay(2000);
  lcd.clear();
}

void title_display() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("welcome to the");
  lcd.setCursor(0, 1);
  lcd.print("   project     ");
  delay(3000);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("health care");
  lcd.setCursor(0, 1);
  lcd.print("    sys     ");
  delay(3000);
  lcd.clear();
  lcd.print("Wifi init....");
}

// ---------- Buzzer ----------
void buzzerAlert() {
  digitalWrite(BUZZER, HIGH);
  delay(2000);
  digitalWrite(BUZZER, LOW);
}

void send_vitals_telnet(int pulse, int spo2, float temp) {
  String msg = "Vitals Alert\r\n";
  msg += "Pulse: " + String(pulse) + " bpm\r\n";
  msg += "SpO2: " + String(spo2) + " %\r\n";
  msg += "Temperature: " + String(temp) + " C\r\n";

  wifi_serial.write("AT+CIPSEND=0,");
  wifi_serial.write(String(msg.length() + 2).c_str());
  wifi_serial.write("\r\n");
  delay(2000);

  wifi_serial.print(msg);
  wifi_serial.write("\r\n");
  delay(3000);
}

// ---------- Setup ----------
void setup() {
  Serial.begin(115200);
  wifi_serial.begin(115200);
  randomSeed(analogRead(0));

  pinMode(BUZZER, OUTPUT);

  lcd.begin(16, 2);
  title_display();
  delay(2000);
  lcd.clear();

  dht.begin();

  if (!particleSensor.begin(Wire, I2C_SPEED_STANDARD)) {
    lcd.print("MAX30105 FAIL");
    while (1);
  }
  particleSensor.setup();

  wifi_init();

  lcd.print("System Ready");
  delay(1500);
  lcd.clear();

  wifi_serial.println("System Started...");
}

// ---------- Loop ----------
void loop() {

  // -------- Weapon Detection --------
  if (Serial.available()) {
    char c = Serial.read();
    if (c == '1') {

      digitalWrite(BUZZER, HIGH);

      Serial.println("⚠️ WEAPON DETECTED");
      send_telnet_alert("Weapon detected....!");

      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("!!! ALERT !!!");
      lcd.setCursor(0, 1);
      lcd.print("Weapon Detected");

      buzzerAlert();
//      weaponAlertSent = true;
    }
    else if (receivedValue == 0) {
      digitalWrite(BUZZER, LOW);
//      weaponAlertSent = false;
    }
  }

  // -------- Health Monitoring --------
  long irValue = particleSensor.getIR();
  temperature = dht.readTemperature();

  if (isnan(temperature)) temperature = 0;

  if (irValue > 70000) {

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Measuring...");

    int pulseSum = 0;
    int spo2Sum = 0;
    int numReadings = 10;

    for (int i = 1; i <= numReadings; i++) {
      int currentPulse = measurePulse();
      int currentSpO2 = measureSpO2();

      pulseSum += currentPulse;
      spo2Sum += currentSpO2;

      lcd.setCursor(0, 1);
      lcd.print("Cnt:");
      lcd.print(i);

      delay(1500);
    }

    pulse = pulseSum / numReadings;
    spo2 = spo2Sum / numReadings;

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Pulse:");
    lcd.print(pulse);

    lcd.setCursor(0, 1);
    lcd.print("SpO2:");
    lcd.print(spo2);

    send_vitals_telnet(pulse, spo2, temperature);

    String vitalsMsg = "Vitals Update\r\n";
    vitalsMsg += "Pulse: " + String(pulse) + " bpm\r\n";
    vitalsMsg += "SpO2: " + String(spo2) + " %\r\n";
    vitalsMsg += "Temperature: " + String(temperature) + " C\r\n";
    send_telnet_alert(vitalsMsg);

    if ((pulse > 88 || temperature > 40) && millis() - lastAlert > 30000) {

      emergency = true;

      String cause = "";

      if (pulse >= 83 && temperature > 40) {
        cause = "Due to High Pulse & Temp";
      } 
      else if (pulse >= 83) {
        cause = "Due to High Pulse";
      } 
      else if (temperature > 40) {
        cause = "Due to High Temperature";
      }

      wifi_serial.println("⚠️ EMERGENCY: " + cause);
      send_telnet_alert("Emergency detected.......!, " + cause);

      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("!!! EMERGENCY !!!");
      lcd.setCursor(0, 1);
      lcd.print(cause);

      buzzerAlert();
      lastAlert = millis();
    }

  } else {
    lcd.setCursor(0, 0);
    lcd.print("Place Finger   ");
    lcd.setCursor(0, 1);
    lcd.print("On Sensor      ");
  }

  delay(1000);
}