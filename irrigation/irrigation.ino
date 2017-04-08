// ----------------------------

const int SENSOR_TEMP_START_PIN = A0; // NUM_SENSORS from here (temp)
const int IRRIGATION_START_PIN = 2; // NUM_SENSORS irrigations from here

const int NUM_SENSORS = 2;
const int NUM_LINES = 2;
const int REFRESH_TIME = 2000;

// ----------------------------

// Each line has NUM_SENSORS sensors (temp, humidity...)
float startThresholds[NUM_LINES][NUM_SENSORS] = {{22.5, 0}, {22.5, 0}};
float stopThresholds[NUM_LINES][NUM_SENSORS] = {{22, 0}, {22, 0}}; 
bool activeLines[2]; 

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
  
  for(int sensorOffset=0; sensorOffset < NUM_LINES; sensorOffset++) {
    pinMode(IRRIGATION_START_PIN + sensorOffset, OUTPUT);
    digitalWrite(IRRIGATION_START_PIN + sensorOffset, LOW);
  }
}

void loop() {
  //Serial.print();
  for(int sensorOffset=0; sensorOffset < NUM_LINES; sensorOffset++) {
    Serial.println("Sensor " + (String) sensorOffset + ": " + 
                               (String) readTemp(SENSOR_TEMP_START_PIN + sensorOffset) + " C " + 
                               "(sTh: " + (String) startThresholds[sensorOffset][0] + ") -> " + 
                               (String) digitalRead(IRRIGATION_START_PIN + sensorOffset));
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
  // Check start conditions
  for(int sensorOffset=0; sensorOffset < NUM_LINES; sensorOffset++) {
    if(readTemp(SENSOR_TEMP_START_PIN + sensorOffset) > startThresholds[sensorOffset][0]) {  // Start condition
      digitalWrite(IRRIGATION_START_PIN + sensorOffset, HIGH);
    } else if(readTemp(SENSOR_TEMP_START_PIN + sensorOffset) < stopThresholds[sensorOffset][0]) { // Stop condition
      digitalWrite(IRRIGATION_START_PIN + sensorOffset, LOW);
    }
  }
  
}

