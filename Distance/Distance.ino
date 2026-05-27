#define TRIG_PIN D3
#define ECHO_PIN D4

long duration;
float distance;

void setup() {
  Serial.begin(115200);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
}

void loop() {

  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(5);

  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  duration = pulseIn(ECHO_PIN, HIGH, 30000); // 30ms timeout

  if (duration == 0) {
    Serial.println("No echo received");
  } else {
    distance = duration * 0.034 / 2;
    Serial.print("Distance: ");
    Serial.print(distance);
    Serial.println(" cm");
  }

  delay(1000);
}
