#include "ping.h"
#include "proxy.h"
#include "flame.h"
#include "legs.h"
#include "pump.h"
#include "activation.h"
#include "lcd.h"
#include "line.h"

bool state_isInversed = false; //true = SLWR, false = SRWR
bool state_isInitialized = false;
unsigned int state_startTime = 0;
unsigned int state_lastSWR = 0;

int CurrentState = 0;
int CounterRead = 0;
int CounterFire = 0;

bool avoidWall(bool inverse = false);
bool flameDetection();
bool avoid3Ladder(bool inverse = false);
bool getCloser2SRWR(bool inverse = false);
void traceRoute();
void traceRouteInversed();

void setup () {
  ping::setup();
  proxy::setup();
  flame::setup();
  legs::setup();
  pump::setup();
  activation::setup();
  lcd::setup();
  line::setup();
}

void loop () {
  activation::update();

  if (activation::isON) {
    if (activation::isLowMove) {
      lcd::justPrint("Path on front     ", "Moving forward + ");
      legs::forward(true);
      return;
    }

    if (!state_isInitialized) {
      state_startTime = millis();
      state_isInversed = ping::checkShouldFollow(); 
      state_isInitialized = true;
    }

    ping::update();
    proxy::update();
    flame::update();
    line::update();

    //
    //    Serial.println(state_lastSWR);
    //
    //    if ((millis() - state_lastSWR) > 17000) {
    //      Serial.println("called");
    //      state_isInversed = !state_isInversed;
    //    }

    if (state_isInversed) {
      if (ping::isOnSLWR) {
        state_lastSWR = millis();
      }
      if (detectLine()) return;
      if (!avoidWall(true)) return;
      if (flameDetection()) return;
      if (!avoid3Ladder(true)) return;
      if (!getCloser2SRWR(true)) return;
      traceRouteInverse();
    } else {
      if (ping::isOnSRWR) {
        state_lastSWR = millis();
      }
      if (detectLine()) return;
      if (!avoidWall()) return;
      if (flameDetection()) return;
      if (!avoid3Ladder()) return;
      if (!getCloser2SRWR()) return;
      traceRoute();
    }
  } else {
    if (state_isInitialized) state_isInitialized = false;

    if (activation::isMenu) {
      if (activation::isMenuChanged) lcd::clean();
      switch (activation::activeMenu) {
        case 0:
          lcd::justPrint(ping::debug(), ping::debug1());
          break;
        case 1:
          lcd::justPrint(flame::debug(), flame::debug1());
          break;
        case 2:
          lcd::justPrint(proxy::debug(), activation::debugSoundActivation());
          break;
        case 3:
          lcd::justPrint(line::debug(), line::debug1());
          break;
        case 4:
          lcd::justPrint("Press start to", "extinguish");
          pump::activate(activation::isStartPushed);
          break;
        default:
          lcd::justPrint("== DEBUG MODE ==", "Press stop again");
      }
    } else {
      standBy();
    }
  }
}

void standBy () {
  if (legs::isStandby) {
    lcd::message(0, lcd::STANDBY);
    lcd::message(1, lcd::BLANK);
    return;
  }

  if (legs::isNormalized) {
    legs::standby();
    return;
  }

  lcd::message(0, lcd::NORMALIZING);
  lcd::message(1, lcd::BLANK);
  legs::normalize();
}

bool avoid3Ladder (bool inverse = false) {
  if (proxy::isDetectingSomething && CurrentState==0 ) {
    lcd::message(0, lcd::THERE_IS_OBSTACLE);
    CurrentState++;
    if (inverse) {
      unsigned int startCounter = millis();
      unsigned int currentCounter = millis();
      while ((currentCounter - startCounter) <= (7300 + 5 * 800)) {
        legs::forwardHigher();
        currentCounter = millis();
      }
    }
    pingupdate();
    state_isInversed = false;
    return true;
  }
  return true;
}

bool avoidWall (bool inverse = false) {
  short int minPos = 0;
  short int maxPos = 8;
  bool minPosFound = false;
  bool maxPosFound = false;

  if (!minPosFound && ping::near_a) {
    minPos = 0;
    minPosFound = true;
  }
  if (!minPosFound && ping::near_b) {
    minPos = 2;
    minPosFound = true;
  }
  if (!minPosFound && ping::near_c) {
    minPos = 4;
    minPosFound = true;
  }
  if (!minPosFound && ping::near_d) {
    minPos = 6;
    minPosFound = true;
  }
  if (!minPosFound && ping::near_e) {
    minPos = 8;
    minPosFound = true;
  }

  if (!maxPosFound && ping::near_e) {
    maxPos = 8;
    maxPosFound = true;
  }
  if (!maxPosFound && ping::near_d) {
    maxPos = 6;
    maxPosFound = true;
  }
  if (!maxPosFound && ping::near_c) {
    maxPos = 4;
    maxPosFound = true;
  }
  if (!maxPosFound && ping::near_b) {
    maxPos = 2;
    maxPosFound = true;
  }
  if (!maxPosFound && ping::near_a) {
    maxPos = 0;
    maxPosFound = true;
  }

  if (!minPosFound || !maxPosFound) return true; // this means wall is successfully avoided, if it's not then continue below

  short int avg = (maxPos + minPos) / 2;

  if (avg < 1) {
    lcd::message(0, lcd::WALL_ON_RIGHT);
    lcd::message(1, lcd::SHIFTING_LEFT);
    legs::shiftLeft();
  } else if (1 <= avg && avg <= 3) {
    lcd::message(0, lcd::WALL_ON_RIGHT);
    lcd::message(1, lcd::ROTATING_CCW);
    legs::rotateCCW();
  } else if (3 < avg && avg < 5) {
    lcd::message(0, lcd::WALL_ON_FRONT);

    if ((maxPos - minPos) == 0) {
      lcd::message(1, lcd::MOVING_BACKWARD);
      legs::backward(); // wall surface is flat
    } else {
      lcd::message(1, lcd::ROTATING_CW);

      if (inverse) {
        legs::rotateCCW(); // wall surface detected is not flat
      } else {
        legs::rotateCW(); // wall surface detected is not flat
      }
    }
  } else if (5 <= avg && avg <= 7) {
    lcd::message(0, lcd::WALL_ON_LEFT);
    lcd::message(1, lcd::ROTATING_CW);
    legs::rotateCW();
  } else if (avg > 7) {
    lcd::message(0, lcd::WALL_ON_LEFT);
    lcd::message(1, lcd::SHIFTING_RIGHT);
    legs::shiftRight();
  }

  return false;
}

bool detectLine () {
  if (line::isDetected && CounterRead == 0){
    CounterRead += 1;
    lcd::message(0, lcd::LINE_DETECTED);
    unsigned int startCounter = millis();
    unsigned int currentCounter = millis();
    while ((currentCounter - startCounter) < 1600) {
      lcd::message(1, lcd::MOVING_FORWARD);
      currentCounter = millis();
      legs::forward();
    }
    return true;
  }
  if (line::isDetected && CounterRead == 2){
    CounterRead += 1;
    lcd::message(0, lcd::LINE_DETECTED);
    unsigned int startCounter = millis();
    unsigned int currentCounter = millis();
    while ((currentCounter - startCounter) < 1600) {
      lcd::message(1, lcd::MOVING_FORWARD);
      currentCounter = millis();
      legs::forward();
    }
    pingupdate();
    return true;
  }
  if (line::isDetected && CounterRead == 4){ // masuk R1 pepet kiri 
    CounterRead += 1;
    lcd::message(0, lcd::LINE_DETECTED);
    unsigned int startCounter = millis();
    unsigned int currentCounter = millis();
    while ((currentCounter - startCounter) < 500) {
      lcd::message(1, lcd::MOVING_FORWARD);
      currentCounter = millis();
      legs::forward();
    }
    pingupdate();
    state_isInversed = true; // pepet kiri
    return true;
  }
  if (line::isDetected && (CounterRead==1 || CounterRead==3)){
    CounterRead += 1;
    lcd::message(0, lcd::LINE_DETECTED);
    unsigned int startCounter = millis();
    unsigned int currentCounter = millis();
    while ((currentCounter - startCounter) < 1600) {
      lcd::message(1, lcd::MOVING_FORWARD);
      currentCounter = millis();
      legs::forward();
    }

    state_isInversed = false;
    return true;
  }
  return false;
}
bool flameDetection () {
  if (flame::is_right && (ping::near_b || ping::near_a)) {
    lcd::message(0, lcd::FIRE_ON_RIGHT);
    lcd::message(1, lcd::SHIFTING_LEFT);
    legs::shiftLeft();
    return true;
  }

  if (flame::is_left && (ping::near_d || ping::near_e)) {
    lcd::message(0, lcd::FIRE_ON_LEFT);
    lcd::message(1, lcd::SHIFTING_RIGHT);
    legs::shiftRight();
    return true;
  }

  if (flame::is_right && !flame::is_left && !flame::is_center) {
    lcd::message(0, lcd::FIRE_ON_RIGHT);
    lcd::message(1, lcd::ROTATING_CW);
    legs::rotateCW();
    return true;
  }

  if (flame::is_left && !flame::is_right && !flame::is_center) {
    lcd::message(0, lcd::FIRE_ON_LEFT);
    lcd::message(1, lcd::ROTATING_CCW);
    legs::rotateCCW();
    return true;
  }

  if (flame::is_right && !flame::is_left && flame::is_center) {
    lcd::message(0, lcd::FIRE_ON_RIGHT);
    lcd::message(1, lcd::ROTATING_CW);
    legs::rotateCWLess();
    return true;
  }

  if (flame::is_left && !flame::is_right && flame::is_center) {
    lcd::message(0, lcd::FIRE_ON_LEFT);
    lcd::message(1, lcd::ROTATING_CCW);
    legs::rotateCCWLess();
    return true;
  }

  if (flame::is_center && CounterFire == 0) {
    lcd::message(0, lcd::FIRE_ON_CENTER);
      if (proxy::isDetectingSomething) {
        lcd::message(1, lcd::EXTINGUISHING);
        pump::extinguish(1000);

        if (flame::state_isIndicatorOn){
          digitalWrite(PIN_FLAME_INDICATOR, HIGH);
        }

        unsigned int startCounter = millis();
        unsigned int currentCounter = millis();

        while ((currentCounter - startCounter) < 1650) {
          currentCounter = millis();
          legs::backward();
        }
        while ((currentCounter - startCounter) < 7650){
          currentCounter = millis();
          legs::rotateCCW();
        }
        CounterFire += 1;
      }
      else {
        lcd::message(1, lcd::MOVING_FORWARD);
        legs::forward();
      }
    return true;
  }
  
  if (flame::is_center && CounterFire == 1) {
    lcd::message(0, lcd::FIRE_ON_CENTER);
      if (proxy::isDetectingSomething) {
        lcd::message(1, lcd::EXTINGUISHING);
        pump::extinguish(1000);

        unsigned int startCounter = millis();
        unsigned int currentCounter = millis();

        while ((currentCounter - startCounter) < 3000) {
          currentCounter = millis();
          legs::backward();
        }
        while ((currentCounter - startCounter) < 9000){
          currentCounter = millis();
          legs::rotateCCW();
        }
        while ((currentCounter - startCounter) < 11000){
          currentCounter = millis();
          legs::shiftRight();
        }
        CounterFire += 1;
        pingupdate();
        state_isInversed = false;
  }else {
        lcd::message(1, lcd::MOVING_FORWARD);
        legs::forward();
      }
    return true;
  }
  
  if (flame::is_center && CounterFire == 2) {
    lcd::message(0, lcd::FIRE_ON_CENTER);
      if (proxy::isDetectingSomething) {
        lcd::message(1, lcd::EXTINGUISHING);
        pump::extinguish(1000);

        unsigned int startCounter = millis();
        unsigned int currentCounter = millis();

        while ((currentCounter - startCounter) < 1650) {
          currentCounter = millis();
          legs::backward();
        }
        while ((currentCounter - startCounter) < 8150){
          currentCounter = millis();
          legs::rotateCCW();
        }
        pingupdate();
        state_isInversed = false;
      }
       
      else {
        lcd::message(1, lcd::MOVING_FORWARD);
        legs::forward();
      }
    return true;
  }
  return false;
}

bool getCloser2SRWR (bool inverse = false) {
  if (inverse) {
    if (ping::far_e && !ping::far_d && !ping::isOnSLWR) {
      lcd::message(0, lcd::FOUND_SLWR);
      lcd::message(1, lcd::SHIFTING_LEFT);
      legs::shiftLeft();
      return false;
    }
  } else {
    if (ping::far_a && !ping::far_b && !ping::isOnSRWR) {
      lcd::message(0, lcd::FOUND_SRWR);
      lcd::message(1, lcd::SHIFTING_RIGHT);
      legs::shiftRight();
      return false;
    }
  }

  return true;
}

void traceRoute () {
  if (!ping::far_a && !ping::far_b) {
    lcd::message(0, lcd::PATH_ON_RIGHT);
    lcd::message(1, lcd::TURNING_RIGHT);
    legs::turnRight();
  } else if (ping::far_a && !ping::far_c) {
    lcd::message(0, lcd::PATH_ON_FRONT);
    lcd::message(1, lcd::MOVING_FORWARD);
    legs::forward();
  } else if (ping::far_a && ping::far_c && !ping::far_e) {
    lcd::message(0, lcd::PATH_ON_LEFT);
    lcd::message(1, lcd::TURNING_LEFT);
    legs::turnLeft();
  } else {
    lcd::message(0, lcd::NO_PATH);
    lcd::message(1, lcd::ROTATING_CCW);
    legs::rotateCCW(200);
    pingupdate();
  }
}

void traceRouteInverse () {
  if (!ping::far_e && !ping::far_d) {
    lcd::message(0, lcd::PATH_ON_LEFT);
    lcd::message(1, lcd::TURNING_LEFT);
    legs::turnLeft();
  } else if (ping::far_e && !ping::far_c) {
    lcd::message(0, lcd::PATH_ON_FRONT);
    lcd::message(1, lcd::MOVING_FORWARD);
    legs::forward();
  } else if (ping::far_e && ping::far_c && !ping::far_a) {
    lcd::message(0, lcd::PATH_ON_RIGHT);
    lcd::message(1, lcd::TURNING_RIGHT);
    legs::turnRight();
  } else {
    lcd::message(0, lcd::NO_PATH);
    lcd::message(1, lcd::ROTATING_CW);
    legs::rotateCW(1000);
    pingupdate();
  }
}

void pingupdate(){
    ping::update();
    ping::update(); 
    ping::update();
    ping::update();
    ping::update();
}
