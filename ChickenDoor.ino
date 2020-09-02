#include <Wire.h>

const int MOTOR_DOWN_PIN = 13;
const int MOTOR_UP_PIN = 12;
const int BUZZER_PIN = 8;
const int LOWER_LIMIT_PIN = 7;
const int UPPER_LIMIT_PIN = 4;
const int BUTTON_PIN = 2;
const int DOOR_TIMEOUT = 60;
const unsigned long BUTTON_DEBOUNCE_DELAY = 50;

enum class MessageLevel {
  RESPONSE,
  INFO,
  WARNING,
  ERROR
};

enum class LimitStatus {
  BOTTOM,
  TOP,
  UNKNOWN,
  ERROR
};

enum class DoorStatus {
  DOWN,
  UP,
  STOPPED
};

bool doorEnabled = false;
DoorStatus doorCommand = DoorStatus::STOPPED;
DoorStatus oldDoorCommand = DoorStatus::STOPPED;
unsigned long doorTime;
DoorStatus doorMoving = DoorStatus::STOPPED;
LimitStatus limitStatus = LimitStatus::UNKNOWN;


int oldButtonState = LOW;
unsigned long lastDebounceTime;
int buttonState;

bool sendMessage(MessageLevel level, String message) {
  String levelstr;

  switch (level) {
    case MessageLevel::RESPONSE:
      levelstr = "RESPONSE";
      break;
    case MessageLevel::INFO:
      levelstr = "INFO";
      break;
    case MessageLevel::WARNING:
      levelstr = "WARNING";
      break;
    case MessageLevel::ERROR:
      levelstr = "INFO";
      break;
  }

  String formatted_message = levelstr + ": " + message;
  Wire.beginTransmission(2);
  Wire.write(formatted_message.c_str());
  Wire.endTransmission();
  Serial.println(formatted_message);
  return true;
}

bool motorUp() {
  digitalWrite(MOTOR_UP_PIN, HIGH);
  digitalWrite(MOTOR_DOWN_PIN, LOW);
  doorMoving = DoorStatus::UP;
  return true;
}

bool motorDown() {
  digitalWrite(MOTOR_DOWN_PIN, HIGH);
  digitalWrite(MOTOR_UP_PIN, LOW);
  doorMoving = DoorStatus::DOWN;
  return true;
}

bool motorStop() {
  digitalWrite(MOTOR_UP_PIN, LOW);
  digitalWrite(MOTOR_DOWN_PIN, LOW);
  doorMoving = DoorStatus::STOPPED;
  return true;
}

bool checkLowerLimit() {
  return digitalRead(LOWER_LIMIT_PIN);
}

bool checkUpperLimit() {
  return digitalRead(UPPER_LIMIT_PIN);
}

LimitStatus checkLimits() {
  if (checkLowerLimit() && checkUpperLimit()) {
    //sendMessage(MessageLevel::ERROR, "Limit Error");
    limitStatus = LimitStatus::ERROR;
    disableDoor();
  }
  else if (checkLowerLimit()) {
    //sendMessage(MessageLevel::INFO, "Limit Chk: Bottom");
    limitStatus = LimitStatus::BOTTOM;
  }
  else if (checkUpperLimit()) {
    //sendMessage(MessageLevel::INFO, "Limit Chk: Top");
    limitStatus = LimitStatus::TOP;
  }
  else {
    //sendMessage(MessageLevel::WARNING, "Limit Chk: Unknown");
    limitStatus = LimitStatus::UNKNOWN;
  }

  return limitStatus;
}

bool disableDoor() {
  doorEnabled = false;
  doorCommand = DoorStatus::STOPPED;
  return true;
}

bool enableDoor() {
  if (!doorEnabled) {
    //digitalWrite(BUZZER_PIN, HIGH);
    delay(1000);
    digitalWrite(BUZZER_PIN, LOW);
    doorEnabled = true;
    doorCommand = DoorStatus::STOPPED;
  }
  return true;
}

unsigned long getElapsedDoorTime() {
  if (doorMoving != DoorStatus::STOPPED) {
    return (millis() - doorTime);
  }
  return 0.0;
}

bool moveDoorCheck() {
  if (doorCommand != oldDoorCommand) {
    sendMessage(MessageLevel::WARNING, "Door Chk: Door cmd changed!");
    doorTime = millis();
    oldDoorCommand = doorCommand;
  }
  
  if (limitStatus == LimitStatus::ERROR || !doorEnabled) {
    sendMessage(MessageLevel::ERROR, "Limit error or disabled.");
    stopDoor();
    disableDoor();
    return false;
  }

  bool result = false;
  switch (doorCommand) {
    case DoorStatus::STOPPED:
      //Serial.println("Door Chk: Stop");
      result = motorStop();
      break;
    case DoorStatus::UP:
      //sendMessage(MessageLevel::INFO, "Door Chk: Moving up");
      if (limitStatus == LimitStatus::TOP) {
        sendMessage(MessageLevel::INFO, "Door Chk: Reached top");
        result = stopDoor();
      }
      else {
        if ((millis() - doorTime) > 60000) {
          sendMessage(MessageLevel::ERROR, "Door Chk: Door timeout");
          stopDoor();
          result = false;
        }
        else {
          result = motorUp();
        }
      }
      break;
    case DoorStatus::DOWN:
      //sendMessage(MessageLevel::INFO, "Door Chk: Moving down");
      if (limitStatus == LimitStatus::BOTTOM) {
        sendMessage(MessageLevel::INFO, "Door Chk: Reached bottom");
        result = stopDoor();
      }
      else {
        if ((millis() - doorTime) > 60000) {
          sendMessage(MessageLevel::ERROR, "Door Chk: Door timeout");
          stopDoor();
          result = false;
        }
        else {
          result = motorDown();
          result = true;
        }
      }
      break;
    default:
      sendMessage(MessageLevel::ERROR, "Door Chk: Bad door command!");
      stopDoor();
      result = false;
      break;
  }
  
  return result;
}

bool raiseDoor() {
  if (limitStatus == LimitStatus::BOTTOM) {
    doorCommand = DoorStatus::UP;
    return true;
  }

  return false;
}

bool lowerDoor(bool force=false) {
  if (limitStatus == LimitStatus::TOP || force) {
    doorCommand = DoorStatus::DOWN;
    return true;
  }

  return false;
}

bool stopDoor() {
  doorCommand = DoorStatus::STOPPED;
  return true;
}

bool buttonHandler() {
  sendMessage(MessageLevel::INFO, "Btn Handler: Button pressed!");
  digitalWrite(BUZZER_PIN, HIGH);
  delay(500);
  digitalWrite(BUZZER_PIN, LOW);
  delay (500);

  if (!doorEnabled) {
    enableDoor();
    return true;
  }

  if (doorMoving != DoorStatus::STOPPED) {
    sendMessage(MessageLevel::WARNING, "Btn Handler: Stopping door...");
    stopDoor();
    return true;
  }

  bool result = false;
  switch (limitStatus) {
    case LimitStatus::BOTTOM:
      sendMessage(MessageLevel::INFO, "Btn Handler: Limit bottom, raising...");
      result = raiseDoor();
      break;
    case LimitStatus::TOP:
      sendMessage(MessageLevel::INFO, "Btn Handler: Limit top, lowering...");
      result = lowerDoor();
      break;
    case LimitStatus::UNKNOWN:
      sendMessage(MessageLevel::INFO, "Btn Handler: Limit unknown, lowering...");
      result = lowerDoor(true);
      break;
    default:
      sendMessage(MessageLevel::INFO, "Btn Handler: Bad limit status!");
      result = false;
      break;
  }

  return result;
}

void setup() {
  Serial.begin(9600);
  Wire.begin(0x08);
  sendMessage(MessageLevel::INFO, "Starting setup...");

  pinMode(MOTOR_DOWN_PIN, OUTPUT);
  pinMode(MOTOR_UP_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(LOWER_LIMIT_PIN, INPUT);
  pinMode(UPPER_LIMIT_PIN, INPUT);
  pinMode(BUTTON_PIN, INPUT);

  digitalWrite(MOTOR_DOWN_PIN, LOW);
  digitalWrite(MOTOR_UP_PIN, LOW);
  digitalWrite(BUZZER_PIN, LOW);

  enableDoor();
  sendMessage(MessageLevel::INFO, "Setup Complete");
}

void loop() {
  int buttonReading = digitalRead(BUTTON_PIN);
  
  checkLimits();
  
  moveDoorCheck();

  if (buttonReading != oldButtonState) {
    lastDebounceTime = millis();
  }

  if ((millis() - lastDebounceTime) > BUTTON_DEBOUNCE_DELAY) {
    if (buttonReading != buttonState) {
      buttonState = buttonReading;

      if (buttonState == HIGH) {
       buttonHandler();
      }
    }
  }

  oldButtonState = buttonReading;

  if (Serial.available() > 0) {
    String command = Serial.readStringUntil('\n');
    bool result = false;

    if (command == "up") {
      result = raiseDoor();
    }
    if (command == "down") {
      result = lowerDoor();
    }
    if (command == "force down") {
      result = lowerDoor(true);
    }
    if (command == "stop") {
      result = stopDoor();
    }
    if (command == "enable") {
      result = enableDoor();
    }
    if (command == "disable") {
      result = disableDoor();
    }
    sendMessage(MessageLevel::RESPONSE, command + ' ' + result);
    if (command == "time") {
      sendMessage(MessageLevel::RESPONSE, String(getElapsedDoorTime()));
    }
  }
}
