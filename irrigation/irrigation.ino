// ----------------------------

const int SENSOR_TEMP_START_PIN = A0; // NUM_SENSORS from here (temp)
const int IRRIGATION_START_PIN = 2; // NUM_SENSORS irrigations from here

//const int NUM_SENSORS = 2;
const int NUM_LINES = 2;
const int REFRESH_TIME = 2000;

const char SERIAL_DELIM = ';';
const char SERIAL_MAX_FRAGS = 6;

const char CMD_SET_INIT = 'I';
const char CMD_SET_STOP = 'S';
const char CMD_REQUEST_COMMANDS = 'R';
const char CMD_UPDATE = 'U';

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

  // Humidity
  int humStartOp;
  float humStartThr;
  int humStopOp;
  float humStopThr;
} lines[NUM_LINES];



// ----------------------------

float readTemp(int sensor);
void updateLines();

void setStartCommand(int numSensor, int tempOp, float tempValue, int humOp, float humValue);
void setStopCommand(int numSensor, int tempOp, float tempValue, int humOp, float humValue);
void sendCommands();
void sendUpdates();

// -------------------------------------------------------------

void setup() {
  // init serial communication
  Serial.begin(9600);

  //configure input/outputs and init values
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);
  
  for(int numSensor=0; numSensor < NUM_LINES; numSensor++) {
    pinMode(IRRIGATION_START_PIN + numSensor, OUTPUT);
    digitalWrite(IRRIGATION_START_PIN + numSensor, LOW);
  }

  inputString.reserve(100);

  // Fixed config for test 
  lines[0].configured = true;
  lines[0].tempStartThr = 22;
  lines[0].tempStopThr = 22;

  lines[1].configured = true;
  lines[1].tempStartThr = 23;
  lines[1].tempStopThr = 22;
  
  for(int numSensor=2; numSensor < NUM_LINES; numSensor++)
    lines[numSensor].configured = false;
}

void loop() {
  //Serial.print();
  for(int numSensor=0; numSensor < NUM_LINES; numSensor++) {
    Serial.println("Sensor " + (String) numSensor + ": " + 
                               (String) readTemp(SENSOR_TEMP_START_PIN + numSensor) + " C " + 
                               "(sTh: " + (String) lines[numSensor].tempStartThr + " / " + (String) lines[numSensor].tempStopThr + ") -> " + 
                               (String) digitalRead(IRRIGATION_START_PIN + numSensor));
  }
  updateLines();
  
  digitalWrite(LED_BUILTIN, LOW);   // turn the LED on (HIGH is the voltage level)
  delay(100);                        // wait for a second
  digitalWrite(LED_BUILTIN, HIGH);    // turn the LED off by making the voltage LOW
  delay(1000);
}

// -------------------------------------------------------------

// Translate the LM35 read to celsius (given the input pin)
float readTemp(int sensor) {
  int value = analogRead(sensor);
  float millivolts = (value / 1023.0) * 5000;
  return  millivolts / 10; // one celsius for each 10 millivolts (LM35)
}

// Check if the lines accomplish the conditions to start/stop
void updateLines() {
  float currentTemp;
  
  // Check conditions
  for(int numSensor=0; numSensor < NUM_LINES; numSensor++) {
    if(!lines[numSensor].configured) continue;
    //Serial.println("Checking line " + (String) numSensor + "...");

    currentTemp = readTemp(SENSOR_TEMP_START_PIN + numSensor);
    
    if(currentTemp > lines[numSensor].tempStartThr) {  // Start condition
      digitalWrite(IRRIGATION_START_PIN + numSensor, HIGH);
      
    } else if(currentTemp < lines[numSensor].tempStopThr) { // Stop condition
      digitalWrite(IRRIGATION_START_PIN + numSensor, LOW);
    }
  } 
}

// Set the start conditions to activate a irrigation line
void setStartCommand(int numSensor, int tempOp, float tempValue, int humOp, float humValue) {
  lines[numSensor].tempStartOp = tempOp;
  lines[numSensor].tempStartThr = tempValue;
  lines[numSensor].humStartOp = humOp;
  lines[numSensor].humStartThr = humValue;

  // if the the line wans't previously configured init stop conditions to the start ones
  if(!lines[numSensor].configured) {
    lines[numSensor].tempStopOp = tempOp;
    lines[numSensor].tempStopThr = tempValue;
    lines[numSensor].humStopOp = humOp;
    lines[numSensor].humStopThr = humValue;
    lines[numSensor].configured = true;
  }
}

// Set the stop conditions to activate a irrigation line
void setStopCommand(int numSensor, int tempOp, float tempValue, int humOp, float humValue) {
  lines[numSensor].tempStopOp = tempOp;
  lines[numSensor].tempStopThr = tempValue;
  lines[numSensor].humStopOp = humOp;
  lines[numSensor].humStopThr = humValue;
  lines[numSensor].configured = true;
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
               SERIAL_DELIM + "0";  // Fixed humidity for now
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

  if(numFrag == 6 && cmd == CMD_SET_INIT) {
      setStartCommand(frags[1].toInt(), frags[2].toInt(), frags[3].toFloat(), frags[4].toInt(), frags[5].toFloat());
  } else if(numFrag == 6 && cmd == CMD_SET_STOP) {
    setStopCommand(frags[1].toInt(), frags[2].toInt(), frags[3].toFloat(), frags[4].toInt(), frags[5].toFloat());
  } else if(numFrag == 1 && cmd == CMD_REQUEST_COMMANDS) {
    sendCommands();
  } else if(numFrag == 1 && cmd == CMD_UPDATE) {
    sendUpdates();
  }
}

