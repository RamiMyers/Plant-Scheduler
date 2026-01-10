#include <Arduino.h>
#define SENSOR_PIN A0
#define PUMP_PIN_B1_A 9
#define PUMP_PIN_B2_A 10
#define VALID_MAX 500
#define VALID_MIN 100
#define DRY_THRESHOLD 250
#define MOISTURE_DELTA_THRESHOLD 5
#define FAULT_CONFIRM_COUNT 2
#define RECOVERY_CONFIRM_COUNT 5
#define SCHEDULE_MISS_MAX 0
#define BUDGET_MISS_MAX 0

enum State { INIT, IDLE, CHECK, WATERING, FAULT, RECOVERY }; 
enum FaultCode { NONE, SENSOR_INVALID, TIMER_INVALID }; 

// Constants
const unsigned long T_sample = 1000000UL;
const unsigned long T_budget = 500000UL;
const unsigned long T_pump_max = 1000000UL;
const unsigned long T_settle = 5000000UL;
// Cycle Timers
unsigned long now, nextRelease, lateness;
// Sample Timers
unsigned long sampleStart, sampleEnd, sampleTime;
// Pump Timers
unsigned long pumpOn, pumpOff;
// Counters
unsigned scheduleCounter;
unsigned budgetMisses = 0, scheduleMisses = 0;
unsigned faultConfirmCounter = 0, recoverConfirmCounter = 0;
// Sensor Values
unsigned moistureValue, lastMoistureValue, deltaMoisture;
// State
State state = INIT;
// Fault Values
FaultCode lastFaultCode = NONE, pendingFaultCode = NONE, newFaultCode = NONE;
unsigned faultCount = 0;
bool faultFlag = false;

void updateStateMachine();
void logValues();
FaultCode latchFaultCode();

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
        if (faultFlag)
          state = FAULT;
        else
          state = CHECK;
      }
      break;
    case CHECK:
      lateness = now - nextRelease;
      scheduleMisses += lateness / T_sample;
      nextRelease += T_sample;

      sampleStart = micros();
      moistureValue = analogRead(SENSOR_PIN);
      sampleEnd = micros();
      sampleTime = sampleEnd - sampleStart;

      budgetMisses += sampleTime / T_budget;

      if (scheduleMisses > SCHEDULE_MISS_MAX || budgetMisses > BUDGET_MISS_MAX) {
        lastFaultCode = TIMER_INVALID;
        faultCount++;
        faultFlag = true;
        state = FAULT;
      } else if ((VALID_MIN > moistureValue || moistureValue > VALID_MAX) || 
                  scheduleCounter > 0 && abs(int(moistureValue - lastMoistureValue)) > MOISTURE_DELTA_THRESHOLD)
      {
        faultConfirmCounter++;
        if (faultConfirmCounter >= FAULT_CONFIRM_COUNT) {
          lastFaultCode = SENSOR_INVALID;
          faultCount++;
          faultFlag = true;
          state = FAULT;
        } else {
          state = IDLE;
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
        digitalWrite(PUMP_PIN_B2_A, LOW);
      } else {
        digitalWrite(PUMP_PIN_B1_A, LOW);
        digitalWrite(PUMP_PIN_B2_A, LOW);
        nextRelease += T_sample;
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
          Serial.print("Recover Confirm Counter: ");
          Serial.println(recoverConfirmCounter);
          if (moistureValue >= VALID_MIN && moistureValue <= VALID_MAX && 
              abs(int(moistureValue - lastMoistureValue)) <= MOISTURE_DELTA_THRESHOLD) 
          {
            recoverConfirmCounter++;
            if (recoverConfirmCounter > RECOVERY_CONFIRM_COUNT)
              state = RECOVERY;
            else
              state = IDLE;
          } else {
            recoverConfirmCounter = 0;
            state = IDLE;
          }
          lastMoistureValue = moistureValue;
          break;
        case TIMER_INVALID:
          Serial.println("Timer Fault");
          lateness = now - nextRelease;
          scheduleMisses= lateness / T_sample;
          nextRelease += T_sample;

          sampleStart = micros();
          moistureValue = analogRead(SENSOR_PIN);
          sampleEnd = micros();
          sampleTime = sampleEnd - sampleStart;

          budgetMisses= sampleTime / T_budget;

          Serial.print("Schedule Misses This Cycle: ");
          Serial.println(scheduleMisses);
          Serial.print("Budget Misses This Cycle: ");
          Serial.println(budgetMisses);
          Serial.print("Recover Confirm Counter: ");
          Serial.println(recoverConfirmCounter);

          if (scheduleMisses <= SCHEDULE_MISS_MAX && budgetMisses <= BUDGET_MISS_MAX) 
          {
            recoverConfirmCounter++;
            if (recoverConfirmCounter > RECOVERY_CONFIRM_COUNT)
              state = RECOVERY;
            else
              state = IDLE;
          } else {
            recoverConfirmCounter = 0;
            state = IDLE;
          }
          break;
      }
      nextRelease += T_sample;
      break;
    case RECOVERY:
      Serial.println("Recovery Mode");
      nextRelease = micros() + T_sample;
      faultConfirmCounter = 0;
      recoverConfirmCounter = 0;
      scheduleMisses = 0;
      budgetMisses = 0;
      faultFlag = false;
      state = IDLE;
      break;
  }
}

void logValues() {
  Serial.print("State: ");
  Serial.println(state);
  Serial.print("Moisture Value: ");
  Serial.println(moistureValue);

  Serial.print("Sample Time: ");
  Serial.print(sampleTime);
  Serial.println(" micros");

  Serial.print("Budget Misses: ");
  Serial.println(budgetMisses);


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

FaultCode latchFaultCode() {
  if ((VALID_MIN > moistureValue || moistureValue > VALID_MAX) ||
       scheduleCounter > 0 && abs(int(moistureValue - lastMoistureValue)) > MOISTURE_DELTA_THRESHOLD) {
    return SENSOR_INVALID;
  } else if (scheduleMisses > SCHEDULE_MISS_MAX || budgetMisses > BUDGET_MISS_MAX) {
    return TIMER_INVALID;
  } else {
    return NONE;
  }
}