#include <Arduino.h>

#define SENSOR_PIN A0
#define VALID_MAX 470
#define VALID_MIN 0
#define DRY_THRESHOLD 440
#define WET_THRESHOLD 195

enum State { INIT, IDLE, CHECK, WATERING, FAULT, RECOVERY }; 
enum FaultCode { SENSOR_INVALID }; 

const unsigned long T_sample = 1000000;
const unsigned long T_budget = 5000;
unsigned long now, nextRelease, sampleStart, sampleEnd, sampleTime, maxSampleTime = 0, lateness;
unsigned deadlineMisses = 0, scheduleMissesThisCycle, scheduleMisses;
unsigned moistureValue;
State state = INIT;

void updateStateMachine();
void logValues();

void setup() {
  Serial.begin(9600); 
  updateStateMachine();
}

void loop() {
  now = micros();
  updateStateMachine();
}

void updateStateMachine() {
  switch (state) {
    case INIT:
      nextRelease = micros() + T_sample;
      state = IDLE;
      break;
    case IDLE:
      if (long(now - nextRelease) >= 0) {
        state = CHECK;
      }
      break;
    case CHECK:
      lateness = now - nextRelease;
      scheduleMissesThisCycle = lateness / T_sample;
      scheduleMisses += scheduleMissesThisCycle;
      nextRelease += (scheduleMissesThisCycle + 1) * T_sample;

      sampleStart = micros();
      moistureValue = analogRead(SENSOR_PIN);
      sampleEnd = micros();
      sampleTime = sampleEnd - sampleStart;

      if (sampleTime > maxSampleTime) maxSampleTime = sampleTime;
      if (sampleTime >= T_budget) deadlineMisses++;

      if (VALID_MIN > moistureValue || moistureValue > VALID_MAX)
        state = FAULT;
      else if (moistureValue >= DRY_THRESHOLD)
        state = WATERING;
      else
        state = IDLE;
      
      logValues();

      break;
    case WATERING:
      Serial.println("Watering");
      state = IDLE;
      break;
    case FAULT:
      Serial.println("Sensor Fault");
      state = IDLE;
      break;
  }
}

void logValues() {
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