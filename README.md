# Plant Watering Controller  
**Deterministic Embedded System with Timing Guarantees and Fault Handling**

## Overview
This project implements a deterministic, microcontroller-based plant watering system with explicit timing constraints, actuator safety limits, and robust fault detection and recovery logic.

The system periodically samples a soil moisture sensor, determines whether watering is required, and drives a pump through a motor driver. The design emphasizes:
- predictable timing behavior
- bounded actuator operation
- fault detection with latching and controlled recovery
- separation of monitoring vs actuation phases
- power-domain isolation between logic and actuation

This project was built to demonstrate **embedded systems engineering practices**, not just functional automation.

---

## System Architecture (High-Level)
- **Sensor**: Resistive soil moisture sensor (analog)
- **Controller**: Arduino (single-threaded, cooperative scheduler)
- **Actuator**: DC pump driven via L9110S motor driver
- **Control Model**: Periodic sampling with non-blocking state machine
- **Power Architecture**:
  - Logic (Arduino + sensor) powered from USB
  - Motor driver + pump powered from an external 5 V supply
  - Common ground shared between logic and motor domains

The system alternates between **monitoring mode** (periodic sensing) and **watering mode** (actuation with enforced safety limits).

---

## System Block Diagram

![System Block Diagram](docs/system_block_diagram.png)

The block diagram illustrates the high-level system architecture and the flow of data, control, and power between subsystems.  
Separate logic and motor power domains are shown with a **shared ground reference**.

---

## Timing Specification

### Sampling Period
| Parameter | Value | Description |
|---------|------|------------|
| `T_sample` | 1,000,000 µs (1 s) | Period between soil moisture checks during monitoring |

### Execution Budget
| Parameter | Value | Description |
|---------|------|------------|
| `T_budget` | 500,000 µs (0.5 s) | Maximum allowed execution time for the sampling job |

If the sampling job exceeds `T_budget`, a **budget miss** is recorded.

### Actuator Timing
| Parameter | Value | Description |
|---------|------|------------|
| `T_pump_max` | 1,000,000 µs (1 s) | Maximum continuous pump ON time |
| `T_settle` | 5,000,000 µs (5 s) | Settling delay after watering before resuming sampling |

During watering and settling, periodic sampling is **intentionally paused** (by design).

---

## Safety Guarantees

### Pump Safety
- The pump **cannot remain ON longer than `T_pump_max`**
- Pump control is non-blocking and time-bounded
- Pump is explicitly shut off on:
  - watering completion
  - fault entry
  - system reset (default LOW outputs)

### Monitoring Safety
- Sampling is suspended during watering and settling
- Schedule and budget violations are only evaluated during monitoring
- Sensor readings are validated before use
- Fault conditions suppress actuation

These guarantees prevent runaway actuation and undefined behavior.

---

## Fault Handling

Faults are **latched**, meaning the system remembers that a fault occurred and does not immediately resume normal operation.

### Fault Confirmation
Faults are only entered after sustained violation of expected behavior to avoid reacting to transient noise.

| Parameter | Value |
|---------|------|
| `FAULT_CONFIRM_COUNT` | N consecutive invalid sensor samples |
| `SCHEDULE_MISS_MAX` | Maximum allowed schedule misses before fault |
| `BUDGET_MISS_MAX` | Maximum allowed budget misses before fault |
| `RECOVERY_CONFIRM_COUNT` | N consecutive valid samples required to recover |

---

## Fault Table

| Fault | Detection Condition | System Response | Recovery Condition |
|-----|--------------------|-----------------|-------------------|
| `SENSOR_INVALID` | Moisture reading outside valid range OR implausible delta across samples | Enter FAULT state, disable pump | N consecutive valid readings with stable delta |
| `TIMER_INVALID` | `scheduleMisses > SCHEDULE_MISS_MAX` OR `budgetMisses > BUDGET_MISS_MAX` | Enter FAULT state, disable pump | Counters reset and N valid monitoring cycles |

---

## Fault Reproduction Guide

### Sensor Invalid Fault
**How to reproduce:**
1. Unplug the moisture sensor
2. Short or float the analog input
3. Introduce large, sudden moisture deltas

**Expected behavior:**
- Fault is latched after `FAULT_CONFIRM_COUNT`
- Pump is disabled
- System remains in FAULT until recovery criteria are met

### Timer Invalid Fault
**How to reproduce:**
1. Artificially introduce execution delays
2. Force repeated schedule or budget violations

**Expected behavior:**
- Fault is latched once miss counters exceed configured limits
- Pump is disabled
- System requires sustained correct timing to recover

---

## Measured Results

### Sensor Ranges (Observed)
| Condition | ADC Range |
|--------|-----------|
| Air | ~450–460 |
| Dry Soil | ~430–440 |
| Wet Soil | ~190–200 |

*(Exact values depend on sensor orientation and soil composition.)*

### Execution Timing
| Metric | Observed |
|------|---------|
| Typical sample time | < few hundred µs |
| Budget misses | Tracked at runtime |
| Schedule misses | Tracked during monitoring only |

---

## Design Decisions (Rationale)

### Why sampling pauses during watering
Sensor readings during watering are unreliable due to:
- electrical noise from the pump
- transient soil saturation
- physical disturbance

Pausing sampling avoids false readings and misleading timing metrics.

### Why faults are latched
Single-sample faults are common with analog sensors and timing jitter. Latching prevents:
- fault flapping
- premature recovery
- unsafe actuator behavior

### Why motor power is isolated
The motor is powered from a separate supply to:
- prevent logic brownouts
- reduce electrical noise on the MCU rail
- ensure deterministic control behavior

Logic and motor domains share a **common ground** for signal reference.

---

## Project Goals
This project was designed to demonstrate:
- real-time reasoning without an RTOS
- explicit timing guarantees
- safe actuator control
- robust fault handling
- power-domain awareness
- clear separation of concerns (monitoring vs actuation)

---

## Future Improvements
- Capacitive moisture sensor for improved stability
- PCB-based implementation with dedicated regulators
- Hardware watchdog integration
- Event-based telemetry for fault analysis

---

## Build & Run
- PlatformIO + Arduino framework
- Target: Arduino Uno (or compatible)