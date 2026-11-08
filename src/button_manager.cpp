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

static DebouncedPin btnMode   = { PIN_BTN_MODE, HIGH, HIGH, 0 };
static DebouncedPin btnUp     = { PIN_BTN_UP,   HIGH, HIGH, 0 };
static DebouncedPin btnDown   = { PIN_BTN_DOWN, HIGH, HIGH, 0 };
static DebouncedPin btnTask   = { PIN_BTN_TASK, HIGH, HIGH, 0 };

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

  pinMode(PIN_BTN_MODE,   INPUT_PULLUP);
  pinMode(PIN_BTN_UP,     INPUT_PULLUP);
  pinMode(PIN_BTN_DOWN,   INPUT_PULLUP);
  pinMode(PIN_BTN_TASK,   INPUT_PULLUP);
}

ButtonEvent buttonManagerUpdate() {
  bool dummyDoorFell = false;
  bool modeFell = false;
  bool upFell   = false;
  bool downFell = false;
  bool taskFell = false;

  // Update all pins every tick so debounce timers stay accurate
  updatePin(doorLeft,  dummyDoorFell);
  updatePin(doorRight, dummyDoorFell);
  updatePin(btnMode,   modeFell);
  updatePin(btnUp,     upFell);
  updatePin(btnDown,   downFell);
  updatePin(btnTask,   taskFell);

  if (taskFell) return BTN_EVENT_TASK_TAP;
  if (modeFell) return BTN_EVENT_MODE_TAP;
  if (upFell)   return BTN_EVENT_UP_TAP;
  if (downFell) return BTN_EVENT_DOWN_TAP;

  return BTN_EVENT_NONE;
}

bool getDoorSwitchAggregateOpen() {
  // Active-LOW: Contact to GND (LOW) means closed.
  // HIGH means magnet removed / switch open.
  return (doorLeft.steadyState == HIGH) || (doorRight.steadyState == HIGH);
}
