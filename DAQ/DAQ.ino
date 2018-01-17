#include <stdio.h>

// Pins
#define PIN_IN1 3
#define PIN_IN2 4
#define PIN_IN3 5
#define PIN_LED 13

// Timing
unsigned long startTime;

// String buffer
char strBuffer[128];

// Stored values
int stored_val1;
int stored_val2;

/* Initialization */
void setup() {
  // Set up serial
  Serial.begin(256000);
  Serial.print("Time (us), val1 (new), val2 (new)\n");

  // Set up pins
  pinMode(PIN_IN1, INPUT_PULLUP);
  pinMode(PIN_IN2, INPUT_PULLUP);
  pinMode(PIN_LED, OUTPUT);

  // Store initial values
  stored_val1 = digitalRead(PIN_IN1) == HIGH;
  stored_val2 = digitalRead(PIN_IN2) == HIGH;

  // Start timing
  startTime = micros();
  dumpSerial(micros() - startTime, stored_val1, stored_val2);
}

/* Main program loop */
void loop() {
  // Read pin values
  int val1 = digitalRead(PIN_IN1) == HIGH;
  int val2 = digitalRead(PIN_IN2) == HIGH;
  // Check if values have changed
  if (val1 != stored_val1 || val2 != stored_val2) {
    // Update stored values
    stored_val1 = val1;
    stored_val2 = val2;
    // Get current time
    unsigned long nowTime = micros();
    // Output to serial dump
    dumpSerial(nowTime - startTime, val1, val2);
  }
}

void dumpSerial(unsigned long _time, int _val1, int _val2) {
  sprintf(strBuffer, "%11lu, %1d, %1d\n", _time, _val1, _val2);
  Serial.print(strBuffer);
}

