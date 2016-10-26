/*
  Rotary Car Hanger Stepper
  Copyright (c) 2016 Subhajit Das

  Licence Disclaimer:

   This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

  Use:
  Rotary car hanger system.

  Features are:

   Adjustable motor speed (RPM).
   Changeable Steps per revolution value for any bipolar stepper motor to be used. (Though 200 spr or 1.8 degree step angle motor is preferred).
   Adjustable number of stages.
   Individual shift angle for every stage (thus any error in manufacturing can be programatically compensated).
   Bidirectional movement for efficient operation.
   Settable offset.
   Storage of setting, thus adjustment required in first run only.

*/

// include the library code:
#include <LiquidCrystal.h>
#include <EEPROM.h>
#include <Keypad.h>
#include <Stepper.h>
#include<math.h>

/*
   :: LCD Pin Connection ::
   LCD RS pin to digital pin 7
   LCD Enable pin to digital pin 6
   LCD D4 pin to digital pin 5
   LCD D5 pin to digital pin 4
   LCD D6 pin to digital pin 3
   LCD D7 pin to digital pin 2
   LCD R/W pin to ground
   LCD VSS pin to ground
   LCD VCC pin to 5V
   10K resistor:
   ends to +5V and ground
   wiper to LCD VO pin (pin 3)

   :: EEPROM memory layout ::
   pfArray is stored as - angle to shift from curr to nxt in cuurrent index
*/

// initialize the library with the numbers of the interface pins
LiquidCrystal lcd(7, 6, 5, 4, 3, 2);

// EEPROM address for stored datas
const byte stepsPerRevAddr = 0; // 2 byte 3 digit
const byte carsAddr = 2;  // 1 byte 2 digit
const byte speedAddr = 3; // 1 byte 2 digit
const byte revAngleAddr = 4;  // 2 bytes 4 digit
const byte currPfAddr = 6;  // 1 byte 2 digit
const byte pfArrayAddr = 7;  // 2 bytes 3 digit each

// function declerations
int getStepPerRev();
int getSpeed();
int getCars();
int getCurrPf();
void getPfAngles(byte);
unsigned int getValue(byte);
void showSaved(byte, int);
void reset();
int getShift(const byte);

// Keypad setup
const byte rows = 4; //four rows
const byte cols = 3; //three columns
const char keys[rows][cols] = {
  {'1', '2', '3'},
  {'4', '5', '6'},
  {'7', '8', '9'},
  {'*', '0', '#'}
};
byte rowPins[rows] = {11, 12, 13, A0}; //connect to the row pinouts of the keypad
byte colPins[cols] = {10, 9, 8}; //connect to the column pinouts of the keypad
Keypad keypad(makeKeymap(keys), rowPins, colPins, rows, cols);

// dummy initialization of stepper
Stepper motor(0, 0, 0);

// global variables
byte Pf, noOfCars;
int stepsPerRevolution;

void setup() {
  // initialize the serial port:
  Serial.begin(9600);

  // set up the LCD's number of columns and rows:
  lcd.begin(16, 2);
  lcd.clear();
  // Print a message to the LCD.
  lcd.print("Rotary Car Parking");
  lcd.setCursor(0, 1);
  lcd.print("Starting...");
  delay(200);
  lcd.clear();

  // initialize for latest calibration
  initialize();

  // initialize motor
  pinMode(A1, OUTPUT);
  pinMode(A2, OUTPUT);
  pinMode(A3, OUTPUT);
  pinMode(A4, OUTPUT);
  // change this to fit the number of steps per revolution for your motor
  stepsPerRevolution = EEPROM.readWord(stepsPerRevAddr);
  motor = Stepper(stepsPerRevolution, A1, A2, A3, A4);
  // set the speed at desired rpm:
  motor.setSpeed(EEPROM.read(speedAddr));

  Pf = EEPROM.read(currPfAddr);
  noOfCars = EEPROM.read(carsAddr);
}

void loop() {
  double shift;
  byte from, to;
  lcd.begin(16, 2);
  lcd.clear();
  lcd.clear();
  Serial.println("Platform : ");
  lcd.print("Platform : ");
  lcd.print(Pf);
  lcd.setCursor(0, 1);
  lcd.print(">");

  to = (byte)getValue(2);
  if (to <= noOfCars && to > 0) {
    shift = getShift(Pf, to);
    Pf = to;
    EEPROM.write(currPfAddr, Pf);
    // move motor to proper place
    motor.step(shift);
  } else {
    lcd.clear();
    Serial.println("Invalid!");
    lcd.print("Invalid!");
  }
  delay(300);
}

int getShift(byte from, const byte to) {
  int shift = 0; // shift angle in degree
  if (from != to) {
    // counting forward angle to destination
    do {
      // obtain current position to nnext position shift angle
      byte storeAddr = pfArrayAddr + ((from - 1) * 2);
      shift += EEPROM.readWord(storeAddr);
      from++;
      if (from > noOfCars) {
        from = 1;
      }
    } while (from != to);

    unsigned int fullRevAngle = EEPROM.readWord(revAngleAddr);
    // if forward is higher than half path go reverse
    if ((fullRevAngle - shift) < shift) {
      shift = -(fullRevAngle - shift);
    }

  }
  shift = (double)shift * (double)stepsPerRevolution / 360.0;
  return shift;
}

void initialize() {
  lcd.begin(16, 2);
  lcd.clear();
  lcd.clear();
  lcd.print("Calibrate?(*/#):");
  Serial.print("Calibrate?(*/#):");
  lcd.setCursor(0, 1);
  boolean wrongKey = true;
  do {
    char key = keypad.waitForKey();
    if (key) // Check for a valid key.
    {
      switch (key)
      {
        case '*':
          wrongKey = false;
          lcd.print("Yes");
          Serial.println("Yes");
          delay(300);
          getNew();
          break;
        case '#':
          wrongKey = false;
          lcd.print("No");
          Serial.println("No");
          delay(300);
          break;
        default:
          lcd.print("Wrong key");
          Serial.print("Wrong key");
          // reset arduino
          delay(300);
          lcd.begin(16, 2);
          lcd.clear();
          lcd.clear();
          lcd.print("Calibrate?(*/#):");
      }
    }
  } while (wrongKey);
}

void getNew() {
  int val, noOfPf;
  val = getStepPerRev();
  EEPROM.writeWord(stepsPerRevAddr, val);
  showSaved(0, val);

  val = getSpeed();
  EEPROM.write(speedAddr, val);
  showSaved(1, val);

  noOfPf = val = getCars();
  EEPROM.write(carsAddr, val);
  showSaved(2, val);

  getPfAngles(noOfPf);

  val = getCurrPf();
  EEPROM.write(currPfAddr, val);
  showSaved(3, val);
}

void showSaved(byte type, int value) {
  lcd.begin(16, 2);
  lcd.clear();
  lcd.clear();
  lcd.print("Saved...");
  lcd.setCursor(0, 1);
  switch (type) {
    case 0:
      lcd.print("S/R : ");
      break;
    case 1:
      lcd.print("Speed : ");
      break;
    case 2:
      lcd.print("Cars : ");
      break;
    case 3:
      lcd.print("Curr Pf : ");
      break;
    case 4:
      lcd.print("Angle : ");
      break;
    case 5:
      lcd.print("Full Rev : ");
      break;
  }
  lcd.print(value);
  delay(500);
}

int getStepPerRev() {
  lcd.begin(16, 2);
  lcd.clear();
  lcd.clear();
  lcd.print("Steps/Rev(3dig):");
  lcd.setCursor(0, 1);
  Serial.println("Steps/Rev (3dig):");
  return getValue(3);
}

int getSpeed() {
  lcd.begin(16, 2);
  lcd.clear();
  lcd.clear();
  lcd.print("Speed(2dig):");
  lcd.setCursor(0, 1);
  Serial.println("Speed (2digit):");
  return getValue(2);
}

int getCars() {
  lcd.begin(16, 2);
  lcd.clear();
  lcd.clear();
  lcd.print("Cars (2dig):");
  Serial.println("Cars (2dig):");
  return getValue(2);
}

int getCurrPf() {
  lcd.begin(16, 2);
  lcd.clear();
  lcd.clear();
  lcd.print("Current Pf(2dig):");
  Serial.println("Current Pf(2dig):");
  return getValue(2);
}

void getPfAngles(byte noOfPf) {
  unsigned int val, fullRevAngle = 0;
  byte storeAddr;
  lcd.begin(16, 2);
  lcd.clear();
  lcd.clear();
  lcd.print("Enter Angles");
  lcd.setCursor(0, 1);
  lcd.print("(3dig each)");
  Serial.println("Enter Angles");
  Serial.println("(3dig each)");
  delay(300);
  for (int i = 0; i < noOfPf; i++) {
    lcd.begin(16, 2);
    lcd.clear();
    lcd.clear();
    lcd.print("From ");
    lcd.print(i + 1);
    lcd.print(" to ");
    if ((i + 2) <= noOfPf)
      lcd.print(i + 2);
    else
      lcd.print(1);
    lcd.print(" : ");

    Serial.print("From ");
    Serial.print(i + 1);
    Serial.print(" to ");
    if ((i + 2) <= noOfPf)
      Serial.print(i + 2);
    else
      Serial.print(1);
    Serial.println(" : ");

    val = getValue(3);
    fullRevAngle += val;
    storeAddr = pfArrayAddr + (i * 2);
    EEPROM.writeWord(storeAddr, val);
    showSaved(4, val);
  }
  EEPROM.writeWord(revAngleAddr, fullRevAngle);
  showSaved(5, fullRevAngle);
}

unsigned int getValue(byte maxCount) {
  unsigned int value = 0;
  byte count = 0;
  while (count < maxCount) {
    char key = keypad.waitForKey();
    if (key) // Check for a valid key.
    {
      if (key >= '0' && key <= '9') {
        value *= 10;
        value += key - 48;
        count++;
      } else if (key == '#') {
        value /= 10;
        if (count > 0)
          count--;
      } else if (key == '*') {
        break;
      }

      lcd.setCursor(0, 1);
      char c[maxCount];
      String(value).toCharArray(c, maxCount);
      for (byte charCount = count; charCount > 0; charCount--) {
        lcd.print(c[count - charCount]);
      }
      lcd.print("     ");
    }
  }
  return value;
}
