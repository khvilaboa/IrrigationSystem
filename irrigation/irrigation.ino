#include <LiquidCrystal.h>

LiquidCrystal lcd(8, 9, 10, 11, 12, 13);

// ----------------------------

const int SENSOR_TEMP_START_PIN = A2; // NUM_SENSORS from here (temp)
const int SENSOR_HUM_START_PIN = A8; // NUM_SENSORS from here (temp)
const int IRRIGATION_START_PIN = 2; // NUM_SENSORS irrigations from here
const int BUTTONS_START_PIN = 18; // Buttons to handle LCD
const int MOTOR_START_PIN = 47; // Step motor
const int MOTOR_EXT_PIN = 34;

const int DEBOUNCE_MILLIS = 200; // Millis to wait in a button interrupt to handle debounce

//const int NUM_SENSORS = 2;
const int NUM_LINES = 4;
const int REFRESH_TIME = 2000;

const char SERIAL_DELIM = ';';
const char SERIAL_MAX_FRAGS = 7;

const char CMD_SET_INIT = 'I';
const char CMD_SET_STOP = 'S';
const char CMD_REQUEST_COMMANDS = 'R';
const char CMD_UPDATE = 'U';
const char CMD_ENABLE_GREENHOUSE = 'G';
const char CMD_DISABLE_GREENHOUSE = 'H';
const char CMD_ENABLE_LINE = 'L';
const char CMD_DISABLE_LINE = 'M';

const int OP_NONE = 0;
const int OP_LESS = 1;
const int OP_GREATER = 2;
const int OP_AND = 3;
const int OP_OR = 4;

// ----------------------------

// Each line has NUM_SENSORS sensors (temp, humidity...)
//float startThresholds[NUM_LINES][NUM_SENSORS] = {{22.5, 0}, {22.5, 0}};
//float stopThresholds[NUM_LINES][NUM_SENSORS] = {{22, 0}, {22, 0}};

String inputString = "";
long nextButtonMillis = millis();  // to control rebounds

typedef enum {SENSOR_STATUS, MAIN_MENU, CONDITION_MENU, GREENHOUSE_MENU} eLcdState;
eLcdState lcdState = SENSOR_STATUS;
int lcdSelectedLine = -1;
int lcdMenuIndex = 0;
int lcdMenuIndex2 = 0;
bool lcdOptionSelected = false;
int lcdOptionOffset = 0;
bool lcdStartCondition = false;

int motorStatus = 1;
bool motorDir = false;

struct irrigationLine {
  bool configured;
  char name[12];
  int maxIrrSeconds;  // Maximum duration of a irrigation
  int minIntSeconds;  // Minimum interval between irrigations

  // Temperature
  int tempStartOp;      // Operator
  float tempStartThr;   // Threshold
  int tempStopOp;
  float tempStopThr;

  int startMidOp;
  int stopMidOp;

  // Humidity
  int humStartOp;
  float humStartThr;
  int humStopOp;
  float humStopThr;

  // Greenhouse
  bool hasGreenHouse;
  float extStartTemp;
  float extStopTemp;
  bool extStarted;
  float doorOpenTemp;
  float doorCloseTemp;
  bool doorOpened;
} lines[NUM_LINES];

// Custom chars from arduino examples
byte armsDown[8] = {
  0b00100,
  0b01010,
  0b00100,
  0b00100,
  0b01110,
  0b10101,
  0b00100,
  0b01010
};

byte armsUp[8] = {
  0b00100,
  0b01010,
  0b00100,
  0b10101,
  0b01110,
  0b00100,
  0b00100,
  0b01010
};

// ----------------------------

float readTemp(int sensor);
void updateLines();
bool checkCondition(float value, int op, float value2);
bool checkCondition(bool value, int op, bool value2);

// Callbacks for buttons interrupts
void btnBack();
void btnDown();
void btnUp();
void btnForward();

// Function to show data in the LCD display
void updateLcd();
void lcdSensorsStatus();
void lcdMenu();
void lcdCondition();

// Motor
void motorStep(int steps, bool inverse);

// Extractor
void start_ext(int numLine);
void stop_ext(int numLine);

void setStartCommand(int numSensor, int tempOp, float tempValue, int midOp, int humOp, float humValue);
void setStopCommand(int numSensor, int tempOp, float tempValue, int midOp, int humOp, float humValue);

void enableGreenhouse(int numLine);
void disableGreenhouse(int numLine);
void enableLine(int numLine);
void disableLine(int numLine);

void sendCommands();
void sendCondtition(int numLine, bool startCond);
void sendUpdates();

// -------------------------------------------------------------

void setup() {
  // init serial communication
  Serial.begin(9600);

  lcd.begin(20, 4);
  lcd.print("Irrigation System");
  lcd.setCursor(0, 2);
  lcd.print("Starting");
  int nextDotPos = 8;

  // create new characters
  lcd.createChar(0, armsDown);
  lcd.createChar(1, armsUp);

  //configure input/outputs and init values
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);

  // Buttons
  for(int i=0; i < 4; i++) pinMode(BUTTONS_START_PIN + i, INPUT);
  attachInterrupt(digitalPinToInterrupt(BUTTONS_START_PIN), btnBack, FALLING);
  attachInterrupt(digitalPinToInterrupt(BUTTONS_START_PIN + 1), btnDown, FALLING);
  attachInterrupt(digitalPinToInterrupt(BUTTONS_START_PIN + 2), btnUp, FALLING);
  attachInterrupt(digitalPinToInterrupt(BUTTONS_START_PIN + 3), btnForward, FALLING);

  // Step motor
  for(int i=0; i < 4; i++) pinMode(MOTOR_START_PIN + i*2, OUTPUT);

  // Extractor
  for(int i=0; i < 4; i++) pinMode(MOTOR_EXT_PIN + i, OUTPUT);
  
  // Sensor / Irrigation outputs
  for(int numSensor=0; numSensor < NUM_LINES; numSensor++) {
    pinMode(IRRIGATION_START_PIN + numSensor, OUTPUT);
    digitalWrite(IRRIGATION_START_PIN + numSensor, LOW);

    pinMode(SENSOR_TEMP_START_PIN  + numSensor, INPUT);
    pinMode(SENSOR_HUM_START_PIN  + numSensor, INPUT);
  }

  inputString.reserve(100);

  // Fixed config for test 
  lines[0].configured = true;
  lines[0].tempStartThr = 22;
  lines[0].tempStartOp = OP_GREATER;
  lines[0].humStartThr = 80;
  lines[0].humStartOp = OP_GREATER;
  lines[0].startMidOp = OP_AND;
  
  lines[0].tempStopThr = 22;
  lines[0].tempStopOp = OP_LESS;
  lines[0].humStopThr = 50;
  lines[0].humStopOp = OP_LESS;
  lines[0].stopMidOp = OP_OR;

  lines[0].hasGreenHouse = true;
  lines[0].extStartTemp = 25;
  lines[0].extStopTemp = 24;
  lines[0].extStarted = false;
  lines[0].doorOpenTemp = 24;
  lines[0].doorCloseTemp = 23;
  lines[0].doorOpened = false;
  
  lines[1].configured = true;
  lines[1].tempStartThr = 23;
  lines[1].tempStartOp = OP_GREATER;
  lines[1].humStartThr = 50;
  lines[1].humStartOp = OP_GREATER;
  lines[1].startMidOp = OP_AND;
  
  lines[1].tempStopThr = 22;
  lines[1].tempStopOp = OP_LESS;
  lines[1].humStopThr = 30;
  lines[1].humStopOp = OP_LESS;
  lines[1].stopMidOp = OP_AND;

  lines[1].hasGreenHouse = false;
  lines[1].doorOpened = false;
  lines[1].extStarted = false;
  
  for(int numSensor=2; numSensor < NUM_LINES; numSensor++) lines[numSensor].configured = false;

  // Start LCD animation
  for(int i=0; i<3; i++) {
    // arms down
    lcd.setCursor(15, 2);
    lcd.write((byte) 0);
    delay(200);

    // add a dot
    lcd.setCursor(nextDotPos++, 2);
    lcd.print(".");
    delay(200);

    // arms up
    lcd.setCursor(15, 2);
    lcd.write((byte) 1);
    
    delay(1000);
  }
  lcd.clear();
}

void loop() {
  //Serial.print();
  /*for(int numSensor=0; numSensor < NUM_LINES; numSensor++) {
    Serial.println("\n\nTemp " + (String) numSensor + ": " + 
                               (String) readTemp(SENSOR_TEMP_START_PIN + numSensor) + " C " + 
                               "(sTh: " + (String) lines[numSensor].tempStartThr + " / " + (String) lines[numSensor].tempStopThr + ") -> " + 
                               (String) digitalRead(IRRIGATION_START_PIN + numSensor));
    delay(100); 
    Serial.println("Hum " + (String) numSensor + ": " + 
                               (String) analogRead(SENSOR_HUM_START_PIN + numSensor) + " % " + 
                               "(sTh: " + (String) lines[numSensor].humStartThr + " / " + (String) lines[numSensor].humStopThr + ") -> " + 
                               (String) digitalRead(IRRIGATION_START_PIN + numSensor));
  }*/
  updateLines();
  updateLcd();
  //motorStep(26, motorDir);  // Uncomment this for test the motor
  //motorDir = !motorDir;
  
  digitalWrite(LED_BUILTIN, LOW);   // turn the LED on (HIGH is the voltage level)
  delay(100);                        // wait for a second
  digitalWrite(LED_BUILTIN, HIGH);    // turn the LED off by making the voltage LOW
  delay(2000);
}

// -------------------------------------------------------------

void motorStep(int steps, bool inverse) {
  for(int i = 0; i < steps; i++) {
    digitalWrite(MOTOR_START_PIN, (motorStatus & 0x1) != 0);
    digitalWrite(MOTOR_START_PIN + 2, (motorStatus & 0x2) != 0);
    digitalWrite(MOTOR_START_PIN + 4, (motorStatus & 0x4) != 0);
    digitalWrite(MOTOR_START_PIN + 6, (motorStatus & 0x8) != 0);

    if(inverse) {
      motorStatus <<= 1;
      if(motorStatus > 8) motorStatus = 1;
    } else {
      motorStatus >>= 1;
      if(motorStatus < 1) motorStatus = 8;
    }
    
    delay(10);
  }

  digitalWrite(MOTOR_START_PIN, 0);
  digitalWrite(MOTOR_START_PIN + 2, 0);
  digitalWrite(MOTOR_START_PIN + 4, 0);
  digitalWrite(MOTOR_START_PIN + 6, 0);
}

void start_ext(int numLine) {
  digitalWrite(MOTOR_EXT_PIN + numLine, HIGH);
}

void stop_ext(int numLine) {
  digitalWrite(MOTOR_EXT_PIN + numLine, LOW);
}

// -------------------------------------------------------------

void btnBack() {
  //Serial.print("<");
  if(millis() > nextButtonMillis) {
    //Serial.println("Back button pressed");
    if(lcdState == SENSOR_STATUS) {
      lcdSelectedLine = -1;
      lcd.noCursor();
      updateLcd();
    } else if(lcdOptionSelected) {
      lcdOptionSelected = false;
      lcdOptionOffset = 0;
      updateLcd();
    } else if(lcdState == MAIN_MENU) {
      lcdMenuIndex = 0;
      lcdState = SENSOR_STATUS;
      updateLcd();
    } else if(lcdState == CONDITION_MENU || lcdState == GREENHOUSE_MENU) {
      lcdState = MAIN_MENU;
      updateLcd();
    }
    nextButtonMillis = millis() + DEBOUNCE_MILLIS;
    //Serial.println((String) millis() + ", " + nextButtonMillis);
  }
}

void btnDown() {
  //Serial.print("v");
  if(millis() > nextButtonMillis) {
    //Serial.println("Down button pressed");
    if(lcdState == SENSOR_STATUS && lcdSelectedLine >= 0) {
      lcdSelectedLine--;
      if(lcdSelectedLine < 0) {
        lcd.noCursor();
      } else {
        lcd.setCursor(0, lcdSelectedLine);
        lcd.cursor();
      }
    } else if(lcdState == MAIN_MENU && lcdMenuIndex > 0 && !lcdOptionSelected) {
      lcdMenuIndex--;
    } else if(lcdState == MAIN_MENU && lcdOptionSelected && lcdMenuIndex == 0) {
      lines[lcdSelectedLine].configured = !lines[lcdSelectedLine].configured;
    } else if((lcdState == CONDITION_MENU || lcdState == GREENHOUSE_MENU) && !lcdOptionSelected && lcdMenuIndex2 > 0) {
      lcdMenuIndex2--;
    } else if(lcdState == CONDITION_MENU && lcdOptionSelected) {
      if(lcdMenuIndex2 == 0) {
        if(lcdOptionOffset == 1) {
          if(lcdStartCondition) {
            lines[lcdSelectedLine].tempStartOp = lines[lcdSelectedLine].tempStartOp == OP_LESS ? OP_GREATER: OP_LESS;
          } else {
            lines[lcdSelectedLine].tempStopOp = lines[lcdSelectedLine].tempStopOp == OP_LESS ? OP_GREATER: OP_LESS;
          }
        } if(lcdOptionOffset == 2) {
          if(lcdStartCondition) {
            lines[lcdSelectedLine].tempStartThr -= 0.1;
          } else {
            lines[lcdSelectedLine].tempStopThr -= 0.1;
          }
        }
      } else { // lcdMenuIndex == 1
        if(lcdOptionOffset == 1) {
          if(lcdStartCondition) {
            lines[lcdSelectedLine].humStartOp = lines[lcdSelectedLine].humStartOp == OP_LESS ? OP_GREATER: OP_LESS;
          } else {
            lines[lcdSelectedLine].humStopOp = lines[lcdSelectedLine].humStopOp == OP_LESS ? OP_GREATER: OP_LESS;
          }
        } if(lcdOptionOffset == 2) {
          if(lcdStartCondition) {
            lines[lcdSelectedLine].humStartThr -= 1;
          } else {
            lines[lcdSelectedLine].humStopThr -= 1;
          }
        }
      }
      sendCondtition(lcdSelectedLine, lcdStartCondition);
    } else if(lcdState == GREENHOUSE_MENU && lcdOptionSelected) {
      if(lcdMenuIndex2 == 0) {
        lines[lcdSelectedLine].hasGreenHouse = !lines[lcdSelectedLine].hasGreenHouse;
      } else if(lcdMenuIndex2 == 1) {
        if(lcdOptionOffset == 1) {
          lines[lcdSelectedLine].doorOpenTemp -= 0.1;
        } else if(lcdOptionOffset == 2) {
          lines[lcdSelectedLine].doorCloseTemp -= 0.1;
        }
      } else if(lcdMenuIndex2 == 2) {
        if(lcdOptionOffset == 1) {
          lines[lcdSelectedLine].extStartTemp -= 0.1;
        } else if(lcdOptionOffset == 2) {
          lines[lcdSelectedLine].extStopTemp -= 0.1;
        }
      }
    }
    updateLcd();
    nextButtonMillis = millis() + DEBOUNCE_MILLIS;
    //Serial.println((String) millis() + ", " + nextButtonMillis);
  }
}

void btnUp() {
  //Serial.print("^");
  if(millis() > nextButtonMillis) {
    //Serial.println("Up button pressed");
    if(lcdState == SENSOR_STATUS && lcdSelectedLine < NUM_LINES - 1) {
      lcdSelectedLine++;
      lcd.setCursor(0, lcdSelectedLine);
      lcd.cursor();      
    } else if(lcdState == MAIN_MENU && !lcdOptionSelected && lcdMenuIndex < 3) {
      lcdMenuIndex++;
    } else if(lcdState == MAIN_MENU && lcdOptionSelected && lcdMenuIndex == 0) {
      lines[lcdSelectedLine].configured = !lines[lcdSelectedLine].configured;
    } else if((lcdState == CONDITION_MENU && !lcdOptionSelected && lcdMenuIndex2 < 1) || (lcdState == GREENHOUSE_MENU && !lcdOptionSelected && lcdMenuIndex2 < 2)) {
      lcdMenuIndex2++;
    } else if(lcdState == CONDITION_MENU && lcdOptionSelected) {
      if(lcdMenuIndex2 == 0) {
        if(lcdOptionOffset == 1) {
          if(lcdStartCondition) {
            lines[lcdSelectedLine].tempStartOp = lines[lcdSelectedLine].tempStartOp == OP_LESS ? OP_GREATER: OP_LESS;
          } else {
            lines[lcdSelectedLine].tempStopOp = lines[lcdSelectedLine].tempStopOp == OP_LESS ? OP_GREATER: OP_LESS;
          }
        } if(lcdOptionOffset == 2) {
          if(lcdStartCondition) {
            lines[lcdSelectedLine].tempStartThr += 0.1;
          } else {
            lines[lcdSelectedLine].tempStopThr += 0.1;
          }
        }
      } else { // lcdMenuIndex == 1
        if(lcdOptionOffset == 1) {
          if(lcdStartCondition) {
            lines[lcdSelectedLine].humStartOp = lines[lcdSelectedLine].humStartOp == OP_LESS ? OP_GREATER: OP_LESS;
          } else {
            lines[lcdSelectedLine].humStopOp = lines[lcdSelectedLine].humStopOp == OP_LESS ? OP_GREATER: OP_LESS;
          }
        } if(lcdOptionOffset == 2) {
          if(lcdStartCondition) {
            lines[lcdSelectedLine].humStartThr += 1;
          } else {
            lines[lcdSelectedLine].humStopThr += 1;
          }
        }
      }
      sendCondtition(lcdSelectedLine, lcdStartCondition);
    } else if(lcdState == GREENHOUSE_MENU && lcdOptionSelected) {
      if(lcdMenuIndex2 == 0) {
        lines[lcdSelectedLine].hasGreenHouse = !lines[lcdSelectedLine].hasGreenHouse;
      } else if(lcdMenuIndex2 == 1) {
        if(lcdOptionOffset == 1) {
          lines[lcdSelectedLine].doorOpenTemp += 0.1;
        } else if(lcdOptionOffset == 2) {
          lines[lcdSelectedLine].doorCloseTemp += 0.1;
        }
      } else if(lcdMenuIndex2 == 2) {
        if(lcdOptionOffset == 1) {
          lines[lcdSelectedLine].extStartTemp += 0.1;
        } else if(lcdOptionOffset == 2) {
          lines[lcdSelectedLine].extStopTemp += 0.1;
        }
      }
    }
    updateLcd();
    nextButtonMillis = millis() + DEBOUNCE_MILLIS;
    //Serial.println((String) millis() + ", " + nextButtonMillis);
  }
}

void btnForward() {
  //Serial.println((String) millis() + ", " + nextButtonMillis);
  //Serial.print(">");
  if(millis() > nextButtonMillis) {
    //Serial.println("Forward button pressed");
    if(lcdState == SENSOR_STATUS && lcdSelectedLine != -1) {
      lcdState = MAIN_MENU;
    } else if(lcdState == MAIN_MENU) {
      switch(lcdMenuIndex) {
        case 0: lcdOptionSelected = !lcdOptionSelected; break;
        case 1: lcdStartCondition = true; lcdState = CONDITION_MENU; break;
        case 2: lcdStartCondition = false; lcdState = CONDITION_MENU; break;
        case 3: lcdState = GREENHOUSE_MENU; break;
      }
    } else if(lcdState == CONDITION_MENU) {
      lcdOptionOffset = (lcdOptionOffset + 1) % 3;
      lcdOptionSelected = lcdOptionOffset != 0;
    } else if(lcdState == GREENHOUSE_MENU) {
      switch(lcdMenuIndex2) {
        case 0: lcdOptionSelected = !lcdOptionSelected; break;
        case 1: case 2: lcdOptionOffset = (lcdOptionOffset + 1) % 3; lcdOptionSelected = lcdOptionOffset != 0; break;
      }
    }
    updateLcd();
    nextButtonMillis = millis() + DEBOUNCE_MILLIS;
    //Serial.println((String) millis() + ", " + nextButtonMillis);
  }
}

void updateLcd() {
  switch(lcdState) {
    case SENSOR_STATUS: lcdSensorsStatus(); break;
    case MAIN_MENU: lcdMenu(); break;
    case CONDITION_MENU: lcdCondition(); break;
    case GREENHOUSE_MENU: lcdGreenhouse(); break;
  }

  // Set cursor
  if(lcdState == SENSOR_STATUS && lcdSelectedLine != -1) {
    lcd.setCursor(0, lcdSelectedLine);
  } else if(lcdState == MAIN_MENU && !lcdOptionSelected) {
    lcd.setCursor(0, lcdMenuIndex);
  } else if(lcdState == MAIN_MENU && lcdOptionSelected && lcdMenuIndex == 0) {
    lcd.setCursor(9, 0); // Enabled option
  } else if(lcdState == CONDITION_MENU) {
    if(lcdOptionSelected) {
      if(lcdOptionOffset == 1) {
        lcd.setCursor(8, lcdMenuIndex2 + 1);
      } else if(lcdOptionOffset == 2) {
        lcd.setCursor(10, lcdMenuIndex2 + 1);
      }
    } else {
      lcd.setCursor(0, lcdMenuIndex2 + 1);
    }
  } else if(lcdState == GREENHOUSE_MENU && lcdOptionSelected && lcdMenuIndex2 == 0) {
    lcd.setCursor(9, 0); // Enabled option
  } else if(lcdState == GREENHOUSE_MENU) {
    if(lcdOptionSelected) {
      if(lcdOptionOffset == 1) {
        lcd.setCursor(6, lcdMenuIndex2);
      } else if(lcdOptionOffset == 2) {
        lcd.setCursor(15, lcdMenuIndex2);
      }
    } else {
      lcd.setCursor(0, lcdMenuIndex2);
    }
  }
}

void lcdSensorsStatus() {
  lcd.clear();
  for(int numLine=0; numLine < NUM_LINES; numLine++) {
    lcd.setCursor(0, numLine);
    if(lines[numLine].configured) {
      lcd.print("L" + (String) numLine + "  " + (String) readTemp(SENSOR_TEMP_START_PIN + numLine) + "C  " + (String) readHum(SENSOR_HUM_START_PIN + numLine) + "%");
    } else {
      lcd.print("L" + (String) numLine + "  -       -");
    }
  }
}

void lcdMenu() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Enabled: " + (String)(lines[lcdSelectedLine].configured?"Yes":"No"));
  lcd.setCursor(0, 1);
  lcd.print("Start condition");
  lcd.setCursor(0, 2);
  lcd.print("Stop condition");
  lcd.setCursor(0, 3);
  lcd.print("Greenhouse");
}

void lcdCondition() {
  lcd.clear();
  lcd.setCursor(0, 0);
  
  float temp, tempOp, hum, humOp;
  
  if(lcdStartCondition) {
    lcd.print("Start condition (" + (String) lcdSelectedLine + "):");
    temp = lines[lcdSelectedLine].tempStartThr;
    tempOp = lines[lcdSelectedLine].tempStartOp;
    hum = lines[lcdSelectedLine].humStartThr;
    humOp = lines[lcdSelectedLine].humStartOp;
  } else {
    lcd.print("Stop condition (" + (String) lcdSelectedLine + "):");
    temp = lines[lcdSelectedLine].tempStopThr;
    tempOp = lines[lcdSelectedLine].tempStopOp;
    hum = lines[lcdSelectedLine].humStopThr;
    humOp = lines[lcdSelectedLine].humStopOp;
  }

  lcd.setCursor(0, 1);
  lcd.print("TEMP:   " + (String)(tempOp == OP_LESS ? "<" : ">") + " " + (String) temp + "C");
  lcd.setCursor(0, 2);
  lcd.print("HUM :   " + (String)(humOp == OP_LESS ? "<" : ">") + " " + (String) hum + "%");
}

void lcdGreenhouse() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Enabled: " + (String)(lines[lcdSelectedLine].hasGreenHouse?"Yes":"No"));
  lcd.setCursor(0, 1);
  lcd.print("Door: " + (String)(lines[lcdSelectedLine].doorOpenTemp) + " -> " + (String)(lines[lcdSelectedLine].doorCloseTemp));
  lcd.setCursor(0, 2);
  lcd.print("Extr: " + (String)(lines[lcdSelectedLine].extStartTemp) + " -> " + (String)(lines[lcdSelectedLine].extStopTemp));
}

// -------------------------------------------------------------

// Translate the LM35 read to celsius (given the input pin)
float readTemp(int sensor) {
  int value = analogRead(sensor);
  float millivolts = (value / 1023.0) * 5000;
  return  millivolts / 10; // one celsius for each 10 millivolts (LM35)
}

// Translate the soil humidity read to humidity percentage (given the input pin)
float readHum(int sensor) {
  int value = analogRead(sensor);
  float perc = map(value, 0, 1023, 100, 0);
  return  perc; 
}

// Check if the lines accomplish the conditions to start/stop
void updateLines() {
  float currentTemp, currentHum;
  bool cond1, cond2;
  
  // Check conditions
  for(int numSensor=0; numSensor < NUM_LINES; numSensor++) {
    if(!lines[numSensor].configured) continue;
    //Serial.println("Checking line " + (String) numSensor + "...");

    currentHum = readHum(SENSOR_HUM_START_PIN + numSensor);
    //delay(500); 
    currentTemp = readTemp(SENSOR_TEMP_START_PIN + numSensor);
    //delay(500); 

    // Start condition
    //Serial.println("Line (" + (String) numSensor + "):");
    cond1 = checkCondition(currentTemp, lines[numSensor].tempStartOp, lines[numSensor].tempStartThr);
    cond2 = checkCondition(currentHum, lines[numSensor].humStartOp, lines[numSensor].humStartThr);
    if(checkCondition(cond1, lines[numSensor].startMidOp, cond2)) digitalWrite(IRRIGATION_START_PIN + numSensor, HIGH);

    // Stop condition
    cond1 = checkCondition(currentTemp, lines[numSensor].tempStopOp, lines[numSensor].tempStopThr);
    cond2 = checkCondition(currentHum, lines[numSensor].humStopOp, lines[numSensor].humStopThr);
    if(checkCondition(cond1, lines[numSensor].stopMidOp, cond2)) digitalWrite(IRRIGATION_START_PIN + numSensor, LOW);

    if(lines[numSensor].hasGreenHouse) {
      if(currentTemp >= lines[numSensor].doorOpenTemp && !lines[numSensor].doorOpened) {
        lines[numSensor].doorOpened = true;
        motorStep(26, true);
      } else if(currentTemp <= lines[numSensor].doorCloseTemp && lines[numSensor].doorOpened) {
        lines[numSensor].doorOpened = false;
        motorStep(26, false);
      } else if(currentTemp >= lines[numSensor].extStartTemp && !lines[numSensor].extStarted) {
        lines[numSensor].extStarted = true;
        start_ext(numSensor);
        Serial.println("STARTING EXTRACTOR");
      } else if(currentTemp <= lines[numSensor].extStopTemp && lines[numSensor].extStarted) {
        lines[numSensor].extStarted = false;
        stop_ext(numSensor);
        Serial.println("STOPING EXTRACTOR");
      }
    }
  } 
}

bool checkCondition(float value, int op, float value2) {
  //Serial.println("cond " + (String) value + ", " + (String) op + ", " + (String) value2 + " -> " + (String) ((op == OP_LESS && value < value2) || (op == OP_GREATER && value > value2)));
  return (op == OP_LESS && value < value2) || (op == OP_GREATER && value > value2);
}

bool checkCondition(bool value, int op, bool value2) {
  return op == OP_NONE || (op == OP_AND && value && value2) || (op == OP_OR && (value || value2));
}

// Set the start conditions to activate a irrigation line
void setStartCommand(int numSensor, int tempOp, float tempValue, int midOp, int humOp, float humValue) {
  lines[numSensor].tempStartOp = tempOp;
  lines[numSensor].tempStartThr = tempValue;
  lines[numSensor].humStartOp = humOp;
  lines[numSensor].humStartThr = humValue;
  lines[numSensor].startMidOp = midOp;

  // if the the line wans't previously configured init stop conditions to the start ones
  if(!lines[numSensor].configured) {
    lines[numSensor].tempStopOp = tempOp;
    lines[numSensor].tempStopThr = tempValue;
    lines[numSensor].humStopOp = humOp;
    lines[numSensor].humStopThr = humValue;
    lines[numSensor].stopMidOp = midOp;
    lines[numSensor].configured = true;
  }
}

// Set the stop conditions to activate a irrigation line
void setStopCommand(int numSensor, int tempOp, float tempValue, int midOp, int humOp, float humValue) {
  lines[numSensor].tempStopOp = tempOp;
  lines[numSensor].tempStopThr = tempValue;
  lines[numSensor].humStopOp = humOp;
  lines[numSensor].humStopThr = humValue;
  lines[numSensor].stopMidOp = midOp;

  if(!lines[numSensor].configured) {
    lines[numSensor].tempStartOp = tempOp;
    lines[numSensor].tempStartThr = tempValue;
    lines[numSensor].humStartOp = humOp;
    lines[numSensor].humStartThr = humValue;
    lines[numSensor].startMidOp = midOp;
    lines[numSensor].configured = true;
  }
}

void enableGreenhouse(int numLine) {
  lines[numLine].hasGreenHouse = true;
}

void disableGreenhouse(int numLine) {
  lines[numLine].hasGreenHouse = false;
}

void enableLine(int numLine) {
  lines[numLine].configured = true;
}

void disableLine(int numLine) {
  lines[numLine].configured = false;
}

// Send all the commands via serial
void sendCommands() {
  for(int numLine=0; numLine < NUM_LINES; numLine++) {
    if(!lines[numLine].configured) continue;
    sendCondtition(numLine, true);
    sendCondtition(numLine, false);
  }
}

void sendCondtition(int numLine, bool startCond) {
  String cmd;

  if(startCond) {
    cmd = (String) CMD_SET_INIT + SERIAL_DELIM + (String) numLine + SERIAL_DELIM + 
          (String) lines[numLine].tempStartOp + SERIAL_DELIM + 
          (String) lines[numLine].tempStartThr + SERIAL_DELIM + 
          (String) lines[numLine].startMidOp + SERIAL_DELIM +
          (String) lines[numLine].humStartOp + SERIAL_DELIM + 
          (String) lines[numLine].humStartThr; 
  } else {  // Stop
    cmd = (String) CMD_SET_STOP + SERIAL_DELIM + (String) numLine + SERIAL_DELIM + 
          (String) lines[numLine].tempStopOp + SERIAL_DELIM + 
          (String) lines[numLine].tempStopThr + SERIAL_DELIM + 
          (String) lines[numLine].stopMidOp + SERIAL_DELIM +
          (String) lines[numLine].humStopOp + SERIAL_DELIM + 
          (String) lines[numLine].humStopThr; 
  }

  Serial.println(cmd);
}

// Send updated sensor reads
void sendUpdates() {
  String updates = "U";
  for(int numSensor=0; numSensor < NUM_LINES; numSensor++) {
    if(!lines[numSensor].configured) continue;
    updates += SERIAL_DELIM + (String) numSensor + 
               SERIAL_DELIM + (String) readTemp(SENSOR_TEMP_START_PIN + numSensor) + 
               SERIAL_DELIM + (String) readHum(SENSOR_HUM_START_PIN + numSensor); 
  }
  Serial.println(updates);
}

// -------------------------------------------------------------

// Function called when a serial message arrive
void serialEvent() {
  char inChar, cmd; 
  String frags[SERIAL_MAX_FRAGS];
  int numFrag = 0;
  
  while (Serial.available()) {
    inChar = (char) Serial.read();
    
    if (inChar == SERIAL_DELIM) {
      //Serial.println(inputString);
      frags[numFrag++] = inputString;
      inputString = "";
      
      if(numFrag == SERIAL_MAX_FRAGS) {
        break;
      }
    } else {
      inputString += inChar;
    }
  }

  if(numFrag < SERIAL_MAX_FRAGS && inputString != "") {
    frags[numFrag++] = inputString;
  }

  inputString = "";
  if(frags[0] == "") return;
  cmd = frags[0].charAt(0);

  if(numFrag == 7 && cmd == CMD_SET_INIT) {
    setStartCommand(frags[1].toInt(), frags[2].toInt(), frags[3].toFloat(), frags[4].toInt(), frags[5].toInt(), frags[6].toFloat());
  } else if(numFrag == 7 && cmd == CMD_SET_STOP) {
    setStopCommand(frags[1].toInt(), frags[2].toInt(), frags[3].toFloat(), frags[4].toInt(), frags[5].toInt(), frags[6].toFloat());
  } else if(numFrag == 1 && cmd == CMD_REQUEST_COMMANDS) {
    sendCommands();
  } else if(numFrag == 1 && cmd == CMD_UPDATE) {
    sendUpdates();
  } else if(numFrag == 2 && cmd == CMD_ENABLE_GREENHOUSE) {
    enableGreenhouse(frags[1].toInt());
  } else if(numFrag == 2 && cmd == CMD_DISABLE_GREENHOUSE) {
    disableGreenhouse(frags[1].toInt());
  } else if(numFrag == 2 && cmd == CMD_ENABLE_LINE) {
    enableLine(frags[1].toInt());
  } else if(numFrag == 2 && cmd == CMD_DISABLE_LINE) {
    disableLine(frags[1].toInt());
  }
}

