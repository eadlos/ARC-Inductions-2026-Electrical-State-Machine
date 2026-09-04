#include <Wire.h>
#include <Servo.h>


const int LDR_PIN   = A0;
const int GAS_PIN   = A1;
const int TEMP_PIN  = A2;
const int BUZZER_PIN = 8;
const int SERVO_PIN  = 9;
const int SLAVE_ADDR = 8;

// defining states
const int state1     = 0; // standby
const int state1A    = 1; // active  gas display
const int state1B    = 2; // active light display
const int state2     = 3; // gas alert
const int state3    = 4; // blackout
const int state4     = 5; // temp emergency
const int MULTISTATE  = 6; // multi-fault

int currentState = state1;
int lastState = state1;
int variablecheckstate;


//the red power button == starts the ritual
// the button just under vol+ inside circle = gas and light toggle
//the top right button (funcstop)= escapes cooked
// servo
Servo emergencyServo;
bool lls = true; // temp emergency arm/disarm flag
// variable to maintain last last state used to debug in temp systems


//  Gas 
bool inGasAlert = false; //this variable was only needed cause 
//we enter the rro state at 180 but leave at

int readGas() {
  return analogRead(GAS_PIN);
}

bool checkgaserror() {
  int gasLevel = readGas();
  if (!inGasAlert && gasLevel > 180) {
    inGasAlert = true;
  } else if (inGasAlert && gasLevel < 130) {
    inGasAlert = false;
  }
  return inGasAlert;
}



//  Light 
bool inBlackout = false;
int prevLightLev = 0;
const int LIGHT_THRESHOLD = 900;
int lightLev;

bool checklighterror() {
  int lightLev = analogRead(LDR_PIN);
  if (!inBlackout && lightLev < prevLightLev) {
    inBlackout = true;
  } else if (inBlackout && lightLev > LIGHT_THRESHOLD) {
    inBlackout = false;
  }
  prevLightLev = lightLev;
  return inBlackout;
}




//  Temp reading function
float readTemp() {
  int raw = analogRead(TEMP_PIN);
  float voltage = raw * (5.0 / 1023.0);
  return (voltage - 0.5) * 100.0;
}

//  Buzzer
unsigned long lastBuzzerToggle = 0;
const unsigned long BUZZER_INTERVAL = 400;
bool buzzerOn = false;

void handleBuzzer(unsigned long now) {
  if (currentState == MULTISTATE) {
    if (now - lastBuzzerToggle >= BUZZER_INTERVAL) {
      lastBuzzerToggle = now;
      buzzerOn = !buzzerOn;
      if (buzzerOn) tone(BUZZER_PIN, 1000);
      else noTone(BUZZER_PIN);
    }
  } else {
    noTone(BUZZER_PIN);
  }
}

//  I2C 
int requestFromSlave() {
  Wire.requestFrom(SLAVE_ADDR, 1);
  if (Wire.available()) {
    return Wire.read();
  }
  return -1;
}

void sendStateToSlave() {
  int gasPercent = map(readGas(), 0, 1023, 0, 100);
  int lightPercent = map(prevLightLev, 0, 1023, 0, 100);

  Wire.beginTransmission(SLAVE_ADDR);
  Wire.write(currentState);
  Wire.write(gasPercent);
  Wire.write(lightPercent);
  Wire.endTransmission();

  
  //tinkercad is shit and more number prints flood its memory and crashes everything
 // Serial.println("State sent: ");
  Serial.println(currentState);
 // Serial.println(readTemp());
 // Serial.println(prevLightLev);
 // Serial.println(checkgaserror());
}

//  Setup 
void setup() {
  Serial.begin(9600);
  Wire.begin();
  emergencyServo.attach(SERVO_PIN);
  emergencyServo.write(0);
  pinMode(BUZZER_PIN, OUTPUT);
  //lcd.init();
  //lcd.backlight(); 
}

//  Loop 
void loop() {
  unsigned long now = millis();
  int bpis = requestFromSlave();
  int lightLev = analogRead(LDR_PIN);
//bpis means button pressed in slave

  //  button handling 
  if (bpis == 0 && lastState == state1) {
    currentState = state1A;
    lastState = currentState;
  }
  else if (bpis == 1 && (lastState == state1A || lastState == state1B)) {
    currentState = (lastState == state1A) ? state1B : state1A;
    lastState = currentState;
  }
  else if (bpis == 4 && lastState == state4) {
    currentState = state1A;
    lls = false;
    lastState = currentState;
  }

  //  temperature handling 
  float tempC = readTemp();

  if (tempC < 45) {
    if (!lls && currentState == state4) {
      sendStateToSlave();
      handleBuzzer(now);
      return;
    }
    if (!lls && currentState != state4) {
      lls = true;
    }
  }

  if (tempC > 45) {
    if (!lls) {
      // stay disarmed, do nothing
    } else {
      currentState = state4;
      lastState = currentState;
      emergencyServo.write(180);
      lls = false;
      sendStateToSlave();
      handleBuzzer(now);
      return;
    }
  } else {
    emergencyServo.write(0);
  }


//once temp handeled goto gas light 
  //  gas / light checks 
  bool gasError = checkgaserror();
  bool lightError = checklighterror();

  if (currentState == MULTISTATE) {
    if (!gasError && !lightError) currentState = state1A;
    else if (!gasError) currentState = state3 ;
    else if (!lightError) currentState = state2;
    lastState = currentState;
  }
  else if (currentState == state2 && !gasError) {
    currentState = state1A;
    lastState = currentState;
  }
  else if (currentState == state3&& !lightError) {
    currentState = state1A;
    lastState = currentState;
  }
  else if (lastState != state1) {
    if (gasError && lightError) currentState = MULTISTATE;
    else if (gasError) currentState = state2;
    else if (lightError) currentState = state3 ;
    lastState = currentState;
  }


delay(50);//not to fry tinkercads memory mid way the code



//sending the slave final print
// if (currentState != lastState) {
  //  sendStateToSlave();  // only send when it actually changed
  //  lastState = currentState;
 // }

  //  send + buzzer 
  handleBuzzer(now);
  sendStateToSlave();
}