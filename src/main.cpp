#include <WiFi.h>
#include <Wire.h>
#include "MAX30105.h"

enum ContextState {
  STATE_IDLE,
  STATE_CROWD,
  STATE_BURNOUT,
  STATE_COMMUTE,
  STATE_EXAM
};

const uint32_t SCAN_INTERVAL_MS = 15000;
const uint32_t HAPTIC_INTERVAL_MS = 500;
const uint32_t LOG_INTERVAL_MS = 5000;
const uint32_t SENSOR_INTERVAL_MS = 50;

const int THRESHOLD_CROWD = 15;
const int THRESHOLD_COMMUTE = 8;
const int THRESHOLD_EXAM = 3;
const int THRESHOLD_BURNOUT = 1;

const int BPM_STRESS_LIMIT = 100;
const long FINGER_IR_MIN = 50000;
const float PEAK_MIN_AMPLITUDE = 150.0f;
const uint32_t BEAT_MIN_MS = 300;
const uint32_t BEAT_MAX_MS = 2000;
const int BPM_AVG_SIZE = 4;

const uint8_t PIN_SDA = 21;
const uint8_t PIN_SCL = 22;

MAX30105 particleSensor;

ContextState currentState = STATE_IDLE;
ContextState previousState = STATE_IDLE;

uint32_t lastScanMs = 0;
uint32_t lastHapticMs = 0;
uint32_t lastLogMs = 0;
uint32_t lastSensorMs = 0;

bool scanInProgress = false;
bool sensorReady = false;
int networkTotal = 0;

float baselineIR = 0.0f;
float sigPrev = 0.0f;
float sigPrevPrev = 0.0f;
uint32_t lastBeatMs = 0;
int bpmHistory[BPM_AVG_SIZE] = {0};
int bpmIndex = 0;
int bpmCount = 0;
int currentBPM = 0;
long lastIR = 0;

void triggerCrowdAlert() {
}

void triggerStealthPacing() {
}

void triggerCommuteNudge() {
}

void triggerExamFocus() {
}

void triggerIdleHaptic() {
}

bool isStationary() {
  return true;
}

const char* stateToString(ContextState state) {
  switch (state) {
    case STATE_CROWD:   return "STATE_CROWD";
    case STATE_BURNOUT: return "STATE_BURNOUT";
    case STATE_COMMUTE: return "STATE_COMMUTE";
    case STATE_EXAM:    return "STATE_EXAM";
    default:            return "STATE_IDLE";
  }
}

ContextState classifyEnvironment(int total) {
  if (currentBPM > BPM_STRESS_LIMIT && isStationary()) {
    if (total > THRESHOLD_CROWD) {
      return STATE_CROWD;
    }
    return STATE_EXAM;
  }
  if (total > THRESHOLD_CROWD) {
    return STATE_CROWD;
  }
  if (total > THRESHOLD_COMMUTE) {
    return STATE_COMMUTE;
  }
  if (total > THRESHOLD_EXAM) {
    return STATE_EXAM;
  }
  if (total >= THRESHOLD_BURNOUT) {
    return STATE_BURNOUT;
  }
  return STATE_IDLE;
}

void updateState() {
  previousState = currentState;
  currentState = classifyEnvironment(networkTotal);
  if (currentState != previousState) {
    Serial.print("[ROUTE] ");
    Serial.print(stateToString(previousState));
    Serial.print(" -> ");
    Serial.println(stateToString(currentState));
  }
}

void resetPulseTracking() {
  baselineIR = 0.0f;
  sigPrev = 0.0f;
  sigPrevPrev = 0.0f;
  lastBeatMs = 0;
  bpmIndex = 0;
  bpmCount = 0;
  currentBPM = 0;
  for (int i = 0; i < BPM_AVG_SIZE; i++) {
    bpmHistory[i] = 0;
  }
}

void registerBeat(uint32_t nowMs) {
  if (lastBeatMs != 0) {
    uint32_t interval = nowMs - lastBeatMs;
    if (interval >= BEAT_MIN_MS && interval <= BEAT_MAX_MS) {
      int instantBPM = (int)(60000UL / interval);
      bpmHistory[bpmIndex] = instantBPM;
      bpmIndex = (bpmIndex + 1) % BPM_AVG_SIZE;
      if (bpmCount < BPM_AVG_SIZE) {
        bpmCount++;
      }
      long sum = 0;
      for (int i = 0; i < bpmCount; i++) {
        sum += bpmHistory[i];
      }
      currentBPM = (int)(sum / bpmCount);
    } else if (interval > BEAT_MAX_MS) {
      bpmCount = 0;
      bpmIndex = 0;
      currentBPM = 0;
    } else {
      return;
    }
  }
  lastBeatMs = nowMs;
}

void processSample(long irValue, uint32_t nowMs) {
  lastIR = irValue;

  if (irValue < FINGER_IR_MIN) {
    resetPulseTracking();
    return;
  }

  if (baselineIR == 0.0f) {
    baselineIR = (float)irValue;
  }
  baselineIR = (baselineIR * 0.95f) + ((float)irValue * 0.05f);

  float signal = (float)irValue - baselineIR;

  if (sigPrev > sigPrevPrev && sigPrev >= signal && sigPrev > PEAK_MIN_AMPLITUDE) {
    registerBeat(nowMs);
  }

  sigPrevPrev = sigPrev;
  sigPrev = signal;
}

void pollSensor(uint32_t nowMs) {
  if (!sensorReady) {
    return;
  }

  particleSensor.check();

  while (particleSensor.available()) {
    long irValue = (long)particleSensor.getFIFOIR();
    processSample(irValue, nowMs);
    particleSensor.nextSample();
  }

  updateState();
}

void startScan() {
  WiFi.scanNetworks(true);
  scanInProgress = true;
  lastScanMs = millis();
}

void pollScan() {
  if (!scanInProgress) {
    return;
  }

  int result = WiFi.scanComplete();

  if (result == WIFI_SCAN_RUNNING) {
    return;
  }

  scanInProgress = false;

  if (result >= 0) {
    networkTotal = result;
    updateState();

    Serial.print("[SCAN] Networks: ");
    Serial.print(networkTotal);
    Serial.print(" | BPM: ");
    Serial.println(currentBPM);
  } else {
    Serial.println("[SCAN] Failed");
  }

  WiFi.scanDelete();
}

void routeState() {
  switch (currentState) {
    case STATE_CROWD:
      triggerCrowdAlert();
      break;
    case STATE_BURNOUT:
      triggerStealthPacing();
      break;
    case STATE_COMMUTE:
      triggerCommuteNudge();
      break;
    case STATE_EXAM:
      triggerExamFocus();
      break;
    case STATE_IDLE:
    default:
      triggerIdleHaptic();
      break;
  }
}

// Change these global variables from long to uint32_t at the top of your code:
// const uint32_t FINGER_IR_MIN = 50000;
// uint32_t lastIR = 0;

void setup() {
  Serial.begin(115200);
  Wire.begin(PIN_SDA, PIN_SCL);

  // Use 400000Hz explicitly for fast I2C transfer rate
  if (particleSensor.begin(Wire, 400000)) { 
    byte ledBrightness = 0x1F;
    byte sampleAverage = 4;
    byte ledMode = 2;
    int sampleRate = 100;
    int pulseWidth = 411;
    int adcRange = 4096;

    particleSensor.setup(ledBrightness, sampleAverage, ledMode, sampleRate, pulseWidth, adcRange);
    sensorReady = true;
    Serial.println("[BOOT] MAX30105/2 online");
  } else {
    sensorReady = false;
    Serial.println("[BOOT] Sensor not found");
  }

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  Serial.println("[BOOT] Context router online");
  startScan();
}


void loop() {
  uint32_t now = millis();

  pollScan();

  if (!scanInProgress && (now - lastScanMs >= SCAN_INTERVAL_MS)) {
    startScan();
  }

  if (now - lastSensorMs >= SENSOR_INTERVAL_MS) {
    lastSensorMs = now;
    pollSensor(now);
  }

  if (now - lastHapticMs >= HAPTIC_INTERVAL_MS) {
    lastHapticMs = now;
    routeState();
  }

  if (now - lastLogMs >= LOG_INTERVAL_MS) {
    lastLogMs = now;
    Serial.print("[LOG] Total: ");
    Serial.print(networkTotal);
    Serial.print(" | BPM: ");
    Serial.print(currentBPM);
    Serial.print(" | IR: ");
    Serial.print(lastIR);
    Serial.print(" | Path: ");
    Serial.println(stateToString(currentState));
  }
}