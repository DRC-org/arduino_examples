#include <Servo.h>

Servo myservo;
const int SV_PIN = 7;

int targetVal = 0;
int currentVal = 0;
int speedDelay = 20;

void setup() {
  Serial.begin(115200);

  myservo.write(currentVal);
  myservo.attach(SV_PIN, 500, 2400);

  Serial.println("Ready.");
}

void loop() {
  if (Serial.available()) {
    targetVal = Serial.parseInt();

    // 残った改行コードなどを捨てる
    while (Serial.available() > 0) { Serial.read(); }

    targetVal = constrain(targetVal, 0, 180);

    Serial.print("Move to: ");
    Serial.print(targetVal);

    if (targetVal > currentVal) {
      for (int i = currentVal; i <= targetVal; i++) {
        myservo.write(i);
        delay(speedDelay);
      }
    } else if (targetVal < currentVal) {
      for (int i = currentVal; i >= targetVal; i--) {
        myservo.write(i);
        delay(speedDelay);
      }
    }

    currentVal = targetVal;

    Serial.println(" ... Done.");
  }
}
