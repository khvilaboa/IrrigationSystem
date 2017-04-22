#include <LiquidCrystal.h>
LiquidCrystal lcd(8, 9, 10, 11, 12, 13);

// ----------------------------

const int SENSOR_TEMP_START_PIN = A2; // NUM_SENSORS from here (temp)
const int SENSOR_HUM_START_PIN = A8; // NUM_SENSORS from here (temp)
const int IRRIGATION_START_PIN = 2; // NUM_SENSORS irrigations from here

//const int NUM_SENSORS = 2;
const int NUM_LINES = 2;
const int REFRESH_TIME = 2000;

const char SERIAL_DELIM = ';';
const char SERIAL_MAX_FRAGS = 7;

const char CMD_SET_INIT = 'I';
const char CMD_SET_STOP = 'S';
const char CMD_REQUEST_COMMANDS = 'R';
const char CMD_UPDATE = 'U';

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
} lines[NUM_LINES];



// ----------------------------

float readTemp(int sensor);
void updateLines();
bool checkCondition(float value, int op, float value2);
bool checkCondition(bool value, int op, bool value2);

void setStartCommand(int numSensor, int tempOp, float tempValue, int midOp, int humOp, float humValue);
void setStopCommand(int numSensor, int tempOp, float tempValue, int midOp, int humOp, float humValue);
void sendCommands();
void sendUpdates();

// -------------------------------------------------------------

void setup() {
  // init serial communication
  Serial.begin(9600);

  lcd.begin(16, 2);
  lcd.print("IrrigationSystem");
  lcd.setCursor(0, 1);
  lcd.print("Starting");

  //configure input/outputs and init values
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);
  
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
  
  for(int numSensor=2; numSensor < NUM_LINES; numSensor++) lines[numSensor].configured = false;

  for(int i=0; i<3; i++) {
    lcd.print(".");
    delay(1000);
  }
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
  
  lcd.setCursor(0, 0);
  lcd.print("L0 " + (String) readTemp(SENSOR_TEMP_START_PIN) + "C " + (String) readHum(SENSOR_HUM_START_PIN) + "%");
  lcd.setCursor(0, 1);
  lcd.print("L1 " + (String) readTemp(SENSOR_TEMP_START_PIN + 1) + "C " + (String) readHum(SENSOR_HUM_START_PIN + 1) + "%");
  
  digitalWrite(LED_BUILTIN, LOW);   // turn the LED on (HIGH is the voltage level)
  delay(100);                        // wait for a second
  digitalWrite(LED_BUILTIN, HIGH);    // turn the LED off by making the voltage LOW
  delay(2000);
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
    Serial.println("Line (" + (String) numSensor + "):");
    cond1 = checkCondition(currentTemp, lines[numSensor].tempStartOp, lines[numSensor].tempStartThr);
    cond2 = checkCondition(currentHum, lines[numSensor].humStartOp, lines[numSensor].humStartThr);
    if(checkCondition(cond1, lines[numSensor].startMidOp, cond2)) digitalWrite(IRRIGATION_START_PIN + numSensor, HIGH);

    // Stop condition
    cond1 = checkCondition(currentTemp, lines[numSensor].tempStopOp, lines[numSensor].tempStopThr);
    cond2 = checkCondition(currentHum, lines[numSensor].humStopOp, lines[numSensor].humStopThr);
    if(checkCondition(cond1, lines[numSensor].stopMidOp, cond2)) digitalWrite(IRRIGATION_START_PIN + numSensor, LOW);
    Serial.println("");
    
    /*if(currentTemp > lines[numSensor].tempStartThr) {  // Start condition
      digitalWrite(IRRIGATION_START_PIN + numSensor, HIGH);
      
    } else if(currentTemp < lines[numSensor].tempStopThr) { // Stop condition
      digitalWrite(IRRIGATION_START_PIN + numSensor, LOW);
    }*/
  } 
}

bool checkCondition(float value, int op, float value2) {
  Serial.println("cond " + (String) value + ", " + (String) op + ", " + (String) value2 + " -> " + (String) ((op == OP_LESS && value < value2) || (op == OP_GREATER && value > value2)));
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
  Serial.println(midOp);

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

// Send all the commands via serial
void sendCommands() {
  
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
  }
}

