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
  bool fell = false;

  updatePin(doorLeft, fell);
  updatePin(doorRight, fell);

  updatePin(btnMode, fell);
  if (fell) return BTN_EVENT_MODE_TAP;

  updatePin(btnUp, fell);
  if (fell) return BTN_EVENT_UP_TAP;

  updatePin(btnDown, fell);
  if (fell) return BTN_EVENT_DOWN_TAP;

  updatePin(btnTask, fell);
  if (fell) return BTN_EVENT_TASK_TAP;

  return BTN_EVENT_NONE;
}

bool getDoorSwitchAggregateOpen() {
  // Active-LOW: Contact to GND (LOW) means closed.
  // HIGH means magnet removed / switch open.
  return (doorLeft.steadyState == HIGH) || (doorRight.steadyState == HIGH);
}