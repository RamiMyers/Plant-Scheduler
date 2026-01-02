#include <Arduino.h>

// TODO: Verify state machine best practices

#define SENSOR_PIN A0
#define VALID_MAX 470
#define VALID_MIN 0
#define DRY_THRESHOLD 440
#define WET_THRESHOLD 195

enum State { IDLE, CHECK, WATERING, FAULT, RECOVERY }; 

const unsigned long T_sample = 1000000;
const unsigned long T_budget = 5000;
unsigned long now, nextRelease, sampleStart, sampleEnd, sampleTime, maxSampleTime = 0, lateness;
unsigned deadlineMisses = 0, scheduleMissesThisCycle, scheduleMisses;
unsigned moistureValue;
byte state;

void updateStateMachine();

void setup() {
  Serial.begin(9600); 

  nextRelease = micros() + T_sample;
}

void loop() {
  now = micros();
  updateStateMachine();

  Serial.print("Moisture Value: ");
  Serial.println(moistureValue);

  Serial.print("Sample Time: ");
  Serial.print(sampleTime);
  Serial.println(" micros");

  Serial.print("Max Sample Time: ");
  Serial.print(maxSampleTime);
  Serial.println(" micros");

  Serial.print("Deadline Misses: ");
  Serial.println(deadlineMisses);


  Serial.print("Schedule Missed By: ");
  Serial.print(lateness);
  Serial.println(" micros");

  Serial.print("Schedule Misses: ");
  Serial.println(scheduleMisses);
  Serial.println();
}

void updateStateMachine() {
  switch (state) {
    case CHECK:
      if (long(now - nextRelease) >= 0) {
        lateness = now - nextRelease;
        scheduleMissesThisCycle = lateness / T_sample;
        scheduleMisses += scheduleMissesThisCycle;
        nextRelease += (scheduleMissesThisCycle + 1) * T_sample;
        state = WATERING;
      }

      break;
    case WATERING:
      sampleStart = micros();
      moistureValue = analogRead(SENSOR_PIN);
      sampleEnd = micros();
      sampleTime = sampleEnd - sampleStart;

      if (sampleTime > maxSampleTime) maxSampleTime = sampleTime;
      if (sampleTime >= T_budget) deadlineMisses++;

      state = CHECK;

      break;
  }
}