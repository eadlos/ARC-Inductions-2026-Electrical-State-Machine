#include <Wire.h>
#include <IRremote.hpp>
#include <LiquidCrystal.h>

LiquidCrystal lcd(7, 6, 5, 4, 3, 2);

const int IR_PIN = 11;

int receivedState = 0;
int receivedGas = 0;
int receivedLight = 0;
int buttonCode = -1;
int lastShown = -1;

void setup() {
  Serial.begin(9600);
  IrReceiver.begin(IR_PIN);

  Wire.begin(8);
  Wire.onReceive(receiveEvent);
  Wire.onRequest(requestEvent);

  lcd.begin(16, 2);
  lcd.print("AWAITING RITUAL");
}

void receiveEvent(int numBytes) {
  if (Wire.available() >= 3) {
    receivedState = Wire.read();
    receivedGas = Wire.read();
    receivedLight = Wire.read();
  }
}

void requestEvent() {
  Wire.write(buttonCode);
  buttonCode = -1;
}

void loop() {
  if (IrReceiver.decode()) {
    unsigned long code = IrReceiver.decodedIRData.decodedRawData;
    Serial.println(code, HEX);

    if (code == 0xFF00BF00) buttonCode = 0;
    else if (code == 0xFA05BF00) buttonCode = 1;
    else if (code == 0xFD02BF00) buttonCode = 4;

    IrReceiver.resume();
  }

  updateDisplay();
}

void updateDisplay() {
  if (receivedState == lastShown) return;
  lastShown = receivedState;

  lcd.clear();
  switch (receivedState) {
    case 0: lcd.print("AWAITING RITUAL"); break;
    case 1: lcd.print("GAS: "); lcd.print(receivedGas); lcd.print("%"); break;
    case 2: lcd.print("LIGHT: "); lcd.print(receivedLight); lcd.print("%"); break;
    case 3: lcd.print("TOXIC PURGE"); break;
    case 4: lcd.print("NOCTIS PROTOCOL"); break;
    case 5: lcd.print("COOKED"); break;
    case 6: lcd.print("MULTIPLE PROBLEMS"); break;
  }
}