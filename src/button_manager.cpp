#include "button_manager.h"
#include "config.h"

struct DebouncedPin {
  uint8_t pin;
  bool lastReading;
  bool steadyState;
  uint32_t lastDebounceTime;
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

  if (uint32_t(millis() - dp.lastDebounceTime) > DEBOUNCE_DELAY_MS) {
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

ButtonEvents buttonManagerUpdate() {
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

  ButtonEvents events = 0;
  if (modeNextFell != modePrevFell)
    events |= buttonEventBit(modeNextFell ? BTN_EVENT_MODE_NEXT : BTN_EVENT_MODE_PREV);
  if (brightUpFell != brightDownFell)
    events |= buttonEventBit(brightUpFell ? BTN_EVENT_BRIGHT_UP : BTN_EVENT_BRIGHT_DOWN);
  return events;
}

bool getDoorSwitchAggregateOpen() {
  // Active-LOW: Contact to GND (LOW) means closed.
  // HIGH means magnet removed / switch open.
  return (doorLeft.steadyState == HIGH) || (doorRight.steadyState == HIGH);
}
