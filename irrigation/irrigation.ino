// ----------------------------

const int SENSOR_TEMP_START_PIN = A0; // NUM_SENSORS from here (temp)
const int IRRIGATION_START_PIN = 2; // NUM_SENSORS irrigations from here

//const int NUM_SENSORS = 2;
const int NUM_LINES = 2;
const int REFRESH_TIME = 2000;

// ----------------------------

// Each line has NUM_SENSORS sensors (temp, humidity...)
//float startThresholds[NUM_LINES][NUM_SENSORS] = {{22.5, 0}, {22.5, 0}};
//float stopThresholds[NUM_LINES][NUM_SENSORS] = {{22, 0}, {22, 0}};

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
    if(!lines[numSensor].configured) break;
    Serial.println("Checking line " + (String) numSensor + "...");

    currentTemp = readTemp(SENSOR_TEMP_START_PIN + numSensor);
    
    if(currentTemp > lines[numSensor].tempStartThr) {  // Start condition
      digitalWrite(IRRIGATION_START_PIN + numSensor, HIGH);
      
    } else if(currentTemp < lines[numSensor].tempStopThr) { // Stop condition
      digitalWrite(IRRIGATION_START_PIN + numSensor, LOW);
    }
  }
  
}

