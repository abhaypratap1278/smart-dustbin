/*
  Smart Dustbin (NodeMCU ESP8266 + Blynk IoT)

  - Ultrasonic sensor (HC-SR04) detects a person near the dustbin
  - Servo motor opens the lid, and closes it a few seconds after the person leaves
  - IR sensor inside the dustbin detects when the garbage reaches the top
  - When the dustbin is full, a notification is sent to the phone through Blynk

  Board in Arduino IDE: "NodeMCU 1.0 (ESP-12E Module)"
  Board package: "esp8266" by ESP8266 Community (Boards Manager)
  Library: "Blynk" by Volodymyr Shymanskyy (Library Manager). The Servo library comes with the ESP8266 package.

  ================= WIRING (power OFF while connecting; common GND everywhere) =================
  HC-SR04 ultrasonic sensor:
    VCC  -> NodeMCU VIN (5V)
    GND  -> NodeMCU GND
    TRIG -> NodeMCU D5
    ECHO -> NodeMCU D6 through a voltage divider (ECHO -> 1k ohm -> D6, and D6 -> 2k ohm -> GND)
            (ECHO gives 5V, the NodeMCU pins are 3.3V, so the divider protects the board)

  Servo motor (lid):
    Signal (orange/yellow) -> NodeMCU D7
    VCC (red)              -> 5V (NodeMCU VIN or a separate 5V supply)
    GND (brown/black)      -> GND (common with NodeMCU)

  IR obstacle sensor (mounted inside the dustbin, near the top, pointing at the garbage level):
    OUT -> NodeMCU D2
    VCC -> NodeMCU 3V3
    GND -> NodeMCU GND

  Power: 5V supply -> NodeMCU (micro-USB or VIN + GND)
  ===============================================================================================
  Full details with tables: see README.md
*/

// ---------- Blynk IoT settings (copy these from your Blynk Cloud console) ----------
#define BLYNK_TEMPLATE_ID   "YOUR_TEMPLATE_ID"
#define BLYNK_TEMPLATE_NAME "Smart Dustbin"
#define BLYNK_AUTH_TOKEN    "YOUR_AUTH_TOKEN"

#define BLYNK_PRINT Serial

#include <ESP8266WiFi.h>
#include <BlynkSimpleEsp8266.h>
#include <Servo.h>

// ---------- WiFi settings (change these) ----------
char ssid[] = "YOUR_WIFI_NAME";
char pass[] = "YOUR_WIFI_PASSWORD";

// ---------- Pins ----------
#define TRIG_PIN   D5   // GPIO14
#define ECHO_PIN   D6   // GPIO12
#define SERVO_PIN  D7   // GPIO13
#define IR_PIN     D2   // GPIO4

// ---------- Settings (tune these for your dustbin) ----------
const int  OPEN_DISTANCE_CM   = 20;     // open the lid when a person is closer than this
const int  LID_CLOSED_ANGLE   = 0;      // servo angle when the lid is closed
const int  LID_OPEN_ANGLE     = 90;     // servo angle when the lid is open
const unsigned long CLOSE_DELAY_MS  = 3000;   // keep the lid open this long after the person leaves
const unsigned long FULL_CONFIRM_MS = 3000;   // garbage must stay in front of the IR sensor this long = full
const bool IR_ACTIVE_LOW = true;        // Most IR sensor modules give LOW when they detect something.
                                        // If yours gives HIGH, set this to false.

Servo lidServo;
BlynkTimer timer;

bool lidOpen = false;
unsigned long lastNearTime = 0;
unsigned long fullSince = 0;
bool fullNotified = false;

// ---------- Ultrasonic distance in cm (returns 0 if no echo) ----------
long readDistanceCm() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  long duration = pulseIn(ECHO_PIN, HIGH, 25000);   // 25 ms timeout
  if (duration == 0) return 0;
  return duration * 0.034 / 2;
}

void openLid() {
  lidServo.write(LID_OPEN_ANGLE);
  lidOpen = true;
}

void closeLid() {
  lidServo.write(LID_CLOSED_ANGLE);
  lidOpen = false;
}

// ---------- Runs every 100 ms: open / close the lid ----------
void checkLid() {
  long d = readDistanceCm();
  bool personNear = (d > 0 && d < OPEN_DISTANCE_CM);

  if (personNear) {
    lastNearTime = millis();
    if (!lidOpen) openLid();
  } else if (lidOpen && (millis() - lastNearTime > CLOSE_DELAY_MS)) {
    closeLid();
  }
}

// ---------- Runs every 500 ms: is the dustbin full? ----------
void checkFull() {
  // Only check while the lid is closed, so a hand or garbage being dropped in doesn't trigger a false alarm
  if (lidOpen) {
    fullSince = 0;
    return;
  }

  bool detected = (digitalRead(IR_PIN) == (IR_ACTIVE_LOW ? LOW : HIGH));

  if (detected) {
    if (fullSince == 0) fullSince = millis();
    if (!fullNotified && (millis() - fullSince > FULL_CONFIRM_MS)) {
      // Event code "bin_full" must be created in the Blynk template (see README)
      Blynk.logEvent("bin_full", "Dustbin is full. Please empty it.");
      fullNotified = true;
    }
  } else {
    // Dustbin emptied: allow a new notification next time it fills up
    fullSince = 0;
    fullNotified = false;
  }
}

void setup() {
  Serial.begin(115200);

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(IR_PIN, INPUT);

  lidServo.attach(SERVO_PIN);
  closeLid();

  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);

  timer.setInterval(100L, checkLid);
  timer.setInterval(500L, checkFull);
}

void loop() {
  Blynk.run();
  timer.run();
}
