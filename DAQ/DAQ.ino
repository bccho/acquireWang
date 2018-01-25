#include <stdio.h>

// Pins
#define NUM_INPUTS 3
const int PIN_INPUTS[] = {3, 4, 5};
#define PIN_TRIGGER 8
#define PIN_LED 13

const char TRIGGER_CHAR = 'T';

// Timing
unsigned long startTime;
unsigned long lastTriggerTime;
unsigned long triggerInterval;
bool started = false;

// String buffer
#define BUFFER_SIZE 128
char strBuffer[BUFFER_SIZE];

// Stored values
int stored_vals[NUM_INPUTS];

/* Print timestamp and input pin values to serial */
void printValues(unsigned long _time, int _vals[]) {
  sprintf(strBuffer, "%9lu", _time);
  Serial.print(strBuffer);
  for (int i = 0; i < NUM_INPUTS; i++) {
    sprintf(strBuffer, ", %10d", _vals[i]);
    Serial.print(strBuffer);
  }
  Serial.print("\n");
}

/*** Send trigger if requested ***/
void serialEvent() {
  if (Serial.available() > 0) {
    char received = Serial.read();
    // Send trigger if we received the trigger character from serial.
    if (received == TRIGGER_CHAR) { sendTrigger(); }
    // Clear input buffer
    while (Serial.available() > 0) { received = Serial.read(); }
  }
}

void sendTrigger() {
  long triggerTime = micros();
  digitalWrite(PIN_TRIGGER, HIGH);
  sprintf(strBuffer, "%9lu, Trigger!\n", triggerTime - startTime);
  Serial.print(strBuffer);
  digitalWrite(PIN_TRIGGER, LOW);
  lastTriggerTime = triggerTime;
  started = true;
}

/* Initialization */
void setup() {
  // Set up serial
  Serial.begin(256000);
  triggerInterval = (unsigned long)Serial.parseInt();
  Serial.print("Trigger interval: ");
  Serial.print(triggerInterval, DEC);
  Serial.print(" us\n");

  // Print CSV header
  Serial.print("Time (us)");
  for (int i = 0; i < NUM_INPUTS; i++) {
    sprintf(strBuffer, ", val%d (new)", i + 1);
    Serial.print(strBuffer);
  }
  Serial.print("\n");

  // Set up pins
  for (int i = 0; i < NUM_INPUTS; i++) {
    pinMode(PIN_INPUTS[i], INPUT_PULLUP);
  }
  pinMode(PIN_TRIGGER, OUTPUT);
  pinMode(PIN_LED, OUTPUT);
  
  digitalWrite(PIN_TRIGGER, LOW);

  // Store initial values
  for (int i = 0; i < NUM_INPUTS; i++) {
    stored_vals[i] = digitalRead(PIN_INPUTS[i]) == HIGH;
  }

  // Start timing
  startTime = micros();
  printValues(micros() - startTime, stored_vals);
}

/* Main program loop */
void loop() {
  /*** Record timestamps when values change ***/
  int vals[NUM_INPUTS];
  bool changed = false;
  long recordTime = micros();
  for (int i = 0; i < NUM_INPUTS; i++) {
    // Read pin values
    vals[i] = digitalRead(PIN_INPUTS[i]) == HIGH;
    // Check if values have changed
    if (vals[i] != stored_vals[i]) {
      // Update stored values
      stored_vals[i] = vals[i];
      changed = true;
    }
  }
  
  if (changed) {
    // Output to serial dump
    printValues(recordTime - startTime, vals);
  }

  /*** If it's time to trigger, trigger. ***/
  if (started && triggerInterval > 0 && recordTime - lastTriggerTime > triggerInterval) {
    sendTrigger();
  }
}
