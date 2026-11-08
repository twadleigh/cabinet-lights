#include "button_manager.h"
#include "config.h"

struct DebouncedPin {
  uint8_t pin;
  bool lastReading;
  bool steadyState;
  unsigned long lastDebounceTime;
};

// Initialize steadyState and lastReading to LOW (Closed/Grounded)
static DebouncedPin doorLeft  = { PIN_DOOR_LEFT,  LOW, LOW, 0 };
static DebouncedPin doorRight = { PIN_DOOR_RIGHT, LOW, LOW, 0 };

static DebouncedPin btnModeNext   = { PIN_BTN_MODE_NEXT,   HIGH, HIGH, 0 };
static DebouncedPin btnModePrev   = { PIN_BTN_MODE_PREV,   HIGH, HIGH, 0 };
static DebouncedPin btnBrightUp   = { PIN_BTN_BRIGHT_UP,   HIGH, HIGH, 0 };
static DebouncedPin btnBrightDown = { PIN_BTN_BRIGHT_DOWN, HIGH, HIGH, 0 };

constexpr unsigned long DEBOUNCE_DELAY_MS = 35;

static void updatePin(DebouncedPin& dp, bool& fellEdge) {
  fellEdge = false;
  bool reading = digitalRead(dp.pin);

  if (reading != dp.lastReading) {
    dp.lastDebounceTime = millis();
    dp.lastReading = reading;
  }

  if ((millis() - dp.lastDebounceTime) > DEBOUNCE_DELAY_MS) {
    if (reading != dp.steadyState) {
      dp.steadyState = reading;
      // Falling edge: transitioned from unpressed (HIGH) to pressed (LOW)
      if (dp.steadyState == LOW) {
        fellEdge = true;
      }
    }
  }
}

void buttonManagerInit() {
  pinMode(PIN_DOOR_LEFT,  INPUT_PULLUP);
  pinMode(PIN_DOOR_RIGHT, INPUT_PULLUP);

  pinMode(PIN_BTN_MODE_NEXT,   INPUT_PULLUP);
  pinMode(PIN_BTN_MODE_PREV,   INPUT_PULLUP);
  pinMode(PIN_BTN_BRIGHT_UP,   INPUT_PULLUP);
  pinMode(PIN_BTN_BRIGHT_DOWN, INPUT_PULLUP);
}

ButtonEvent buttonManagerUpdate() {
  bool dummyDoorFell   = false;
  bool modeNextFell    = false;
  bool modePrevFell    = false;
  bool brightUpFell    = false;
  bool brightDownFell  = false;

  // Update all pins every tick so debounce timers stay accurate
  updatePin(doorLeft,      dummyDoorFell);
  updatePin(doorRight,     dummyDoorFell);
  updatePin(btnModeNext,   modeNextFell);
  updatePin(btnModePrev,   modePrevFell);
  updatePin(btnBrightUp,   brightUpFell);
  updatePin(btnBrightDown, brightDownFell);

  if (modeNextFell)   return BTN_EVENT_MODE_NEXT;
  if (modePrevFell)   return BTN_EVENT_MODE_PREV;
  if (brightUpFell)   return BTN_EVENT_BRIGHT_UP;
  if (brightDownFell) return BTN_EVENT_BRIGHT_DOWN;

  return BTN_EVENT_NONE;
}

bool getDoorSwitchAggregateOpen() {
  // Active-LOW: Contact to GND (LOW) means closed.
  // HIGH means magnet removed / switch open.
  return (doorLeft.steadyState == HIGH) || (doorRight.steadyState == HIGH);
}
