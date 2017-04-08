
const int SENSOR_TEMP_1 = A0;
const int SENSOR_TEMP_2 = A1;

float readTemp(int sensor);

// -------------------------------------------------------------

void setup() {
  // init serial communication
  Serial.begin(9600);

  //configure input/outputs
  pinMode(LED_BUILTIN, OUTPUT);

  //init values
  digitalWrite(LED_BUILTIN, LOW);
}

void loop() {
  Serial.print(readTemp(SENSOR_TEMP_1));
  Serial.println(" C");

  digitalWrite(LED_BUILTIN, HIGH);   // turn the LED on (HIGH is the voltage level)
  delay(100);                       // wait for a second
  digitalWrite(LED_BUILTIN, LOW);    // turn the LED off by making the voltage LOW
  delay(1000);
}

// -------------------------------------------------------------

// Translate the LM35 read to celsius (given the input pin)
float readTemp(int sensor) {
  int value = analogRead(sensor);
  float millivolts = (value / 1023.0) * 5000;
  return  millivolts / 10; // one celsius for each 10 millivolts (LM35)
}

