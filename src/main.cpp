#include <Arduino.h>  // MANDATORY: PlatformIO requires this at the absolute top
#include <WiFi.h>

enum ContextState {
  STATE_IDLE,
  STATE_CROWD,
  STATE_BURNOUT,
  STATE_COMMUTE,
  STATE_EXAM
};

// Use unsigned long instead of uint32_t for millis tracking to prevent compiler mismatch warnings
const unsigned long SCAN_INTERVAL_MS = 15000;
const unsigned long HAPTIC_INTERVAL_MS = 500;
const unsigned long LOG_INTERVAL_MS = 5000;

const int THRESHOLD_CROWD = 15;
const int THRESHOLD_COMMUTE = 8;
const int THRESHOLD_EXAM = 3;
const int THRESHOLD_BURNOUT = 1;

ContextState currentState = STATE_IDLE;
ContextState previousState = STATE_IDLE;

unsigned long lastScanMs = 0;
unsigned long lastHapticMs = 0;
unsigned long lastLogMs = 0;

bool scanInProgress = false;
int networkTotal = 0;

// ADDED DEBUG LOGS: These empty placeholders now print text to your screen so you can verify the route works!
void triggerCrowdAlert() {
  // Aromal will later plug in LRA haptic sine waves here
  static unsigned long lastPrint = 0;
  if (millis() - lastPrint > 2000) {
    Serial.println("[HAPTIC] Running: Silent 60 BPM Stealth Pacing...");
    lastPrint = millis();
  }
}

void triggerStealthPacing() {
  static unsigned long lastPrint = 0;
  if (millis() - lastPrint > 2000) {
    Serial.println("[HAPTIC] Running: Sharp Asymmetric Loops...");
    lastPrint = millis();
  }
}

void triggerCommuteNudge() {
  static unsigned long lastPrint = 0;
  if (millis() - lastPrint > 2000) {
    Serial.println("[HAPTIC] Running: Heavy Continuous Somatic Waves...");
    lastPrint = millis();
  }
}

void triggerExamFocus() {
  static unsigned long lastPrint = 0;
  if (millis() - lastPrint > 2000) {
    Serial.println("[HAPTIC] Running: 4-Second Tactile Box-Breathing...");
    lastPrint = millis();
  }
}

void triggerIdleHaptic() {
  // Idle state remains silent
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

void startScan() {
  // Setup client-side transient background scanning parameters cleanly
  WiFi.scanNetworks(true, false, false, 150); 
  scanInProgress = true;
  lastScanMs = millis();
}

void pollScan() {
  if (!scanInProgress) {
    return;
  }

  int result = WiFi.scanComplete();

  // Handle the specific variable parameters returned by the Espressif core
  if (result == WIFI_SCAN_RUNNING || result == -1) {
    return;
  }

  scanInProgress = false;

  if (result >= 0) {
    networkTotal = result;
    previousState = currentState;
    currentState = classifyEnvironment(networkTotal);

    Serial.print("[SCAN] Networks Found: ");
    Serial.println(networkTotal);

    if (currentState != previousState) {
      Serial.print("[ROUTE] State Altered: ");
      Serial.print(stateToString(previousState));
      Serial.print(" -> ");
      Serial.println(stateToString(currentState));
    }
  } else {
    Serial.println("[SCAN] Internal Core Timed Out");
  }

  // Clear network cache frames out of heap storage memory to protect system assets
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

void setup() {
  Serial.begin(115200);
  delay(1000); // Give serial hub buffer space to spin up safely
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  Serial.println("[BOOT] Smartwatch Context Router Online.");
  startScan();
}

void loop() {
  unsigned long now = millis(); // Kept unified layout types matching unsigned long variables

  pollScan();

  if (!scanInProgress && (now - lastScanMs >= SCAN_INTERVAL_MS)) {
    startScan();
  }

  if (now - lastHapticMs >= HAPTIC_INTERVAL_MS) {
    lastHapticMs = now;
    routeState();
  }

  if (now - lastLogMs >= LOG_INTERVAL_MS) {
    lastLogMs = now;
    Serial.print("[LOG] Total Sniffed: ");
    Serial.print(networkTotal);
    Serial.print(" | Active Path: ");
    Serial.println(stateToString(currentState));
  }
}
