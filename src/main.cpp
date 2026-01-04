#include <Arduino.h>
#define SENSOR_PIN A0
#define PUMP_PIN_B1_A 9
#define PUMP_PIN_B2_A 10
#define VALID_MAX 470
#define VALID_MIN 100
#define DRY_THRESHOLD 440
#define WET_THRESHOLD 195
#define MOISTURE_DELTA_THRESHOLD 5
#define FAULT_CONFIRM_COUNT 3
#define RECOVERY_CONFIRM_COUNT 3

// TODO: Calculate moisture delta, go into FAULT if larger than MOISTURE_DELTA_THRESHOLD

enum State { INIT, IDLE, CHECK, WATERING, FAULT, RECOVERY }; 
enum FaultCode { NONE, SENSOR_INVALID }; 

// Constants
const unsigned long T_sample = 1000000;
const unsigned long T_budget = 500000;
const unsigned long T_pump_max = 1000000;
// Cycle Timers
unsigned long now, nextRelease, lateness;
// Sample Timers
unsigned long sampleStart, sampleEnd, sampleTime, maxSampleTime = 0;
// Pump Timers;
unsigned long pumpOn, pumpOff;
// Counters
unsigned deadlineMisses = 0, scheduleCounter = 0, scheduleMissesThisCycle, scheduleMisses;
unsigned faultConfirmCounter = 0, recoverConfirmCounter = 0;
// Sensor Values
unsigned moistureValue, lastMoistureValue, deltaMoisture;
// State
State state = INIT;
// Fault Values
FaultCode lastFaultCode = NONE;
unsigned faultCount = 0;

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
      pinMode(PUMP_PIN_B1_A, OUTPUT);
      pinMode(PUMP_PIN_B2_A, OUTPUT);

      digitalWrite(PUMP_PIN_B1_A, LOW);
      digitalWrite(PUMP_PIN_B2_A, LOW);

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

      if ((VALID_MIN > moistureValue || moistureValue > VALID_MAX) ||
          scheduleCounter > 0 && abs(moistureValue - lastMoistureValue) > MOISTURE_DELTA_THRESHOLD)
      {
        faultConfirmCounter++;
        if (faultConfirmCounter >= FAULT_CONFIRM_COUNT) {
          lastFaultCode = SENSOR_INVALID;
          faultCount++;
          state = FAULT;
        }
      } else if (moistureValue >= DRY_THRESHOLD) {
        faultConfirmCounter = 0;
        pumpOn = micros();
        state = WATERING;
      } else {
        faultConfirmCounter = 0;
        state = IDLE;
      }

      lastMoistureValue = moistureValue;
      scheduleCounter++;
      
      logValues();

      break;
    case WATERING:
      if (now - pumpOn < T_pump_max) {
        digitalWrite(PUMP_PIN_B1_A, HIGH);
      } else {
        digitalWrite(PUMP_PIN_B1_A, LOW);
        nextRelease = micros() + T_sample;
        state = IDLE;
      }
      break;
    case FAULT:
      switch (lastFaultCode) {
        case SENSOR_INVALID:
          moistureValue = analogRead(SENSOR_PIN);
          Serial.println("Sensor Fault");
          Serial.print("Moisture Value: ");
          Serial.println(moistureValue);
          if (moistureValue >= VALID_MIN && moistureValue <= VALID_MAX && 
              abs(moistureValue - lastMoistureValue) <= MOISTURE_DELTA_THRESHOLD) 
          {
            recoverConfirmCounter++;
            if (recoverConfirmCounter > RECOVERY_CONFIRM_COUNT)
              state = RECOVERY;
          } else {
            recoverConfirmCounter = 0;
          }
          lastMoistureValue = moistureValue;
          break;
      }
      break;
    case RECOVERY:
      Serial.println("Recovery Mode");
      nextRelease = micros() + T_sample;
      faultConfirmCounter = 0;
      recoverConfirmCounter = 0;
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

  Serial.print("Last Fault Code: ");
  Serial.println(lastFaultCode);
  Serial.print("Fault Count: ");
  Serial.println(faultCount);
  Serial.println();
}