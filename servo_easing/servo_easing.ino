#include <Arduino.h>
#include <Servo.h>

#define SV_COUNT 4
const uint8_t SV_PINS[SV_COUNT] = { 4, 5, 6, 7 };

// true にしたサーボだけ 270° スケーリングが適用される
const bool IS_270[SV_COUNT] = { true, false, true, true };

// true にしたサーボはイージングをスキップして servo.write() を直接呼ぶ
const bool DIRECT_WRITE[SV_COUNT] = { false, true, false, false };

void updateServos();

Servo servos[SV_COUNT];

float startVal[SV_COUNT];
float targetVal[SV_COUNT];
float currentVal[SV_COUNT];
uint32_t motionStartTime[SV_COUNT];
uint32_t motionDuration[SV_COUNT];
int8_t state[SV_COUNT];

// 90° を約2秒で移動する平均角速度
const float SERVO_AVG_SPEED = 45.0f;     // 度/秒
const uint32_t UPDATE_INTERVAL_MS = 10;  // 更新間隔 (100Hz)

uint32_t lastUpdateTime = 0;

void setup() {
  Serial.begin(115200);

  for (int i = 0; i < SV_COUNT; i++) {
    currentVal[i] = 90.0f;
    startVal[i] = 90.0f;
    targetVal[i] = 90.0f;
    state[i] = 0;
    servos[i].write(90);
    servos[i].attach(SV_PINS[i], 500, 2400);
  }

  Serial.println("Ready. Format: \"<index(0-3)> <angle>\"");
}

void loop() {
  if (Serial.available()) {
    int index = Serial.parseInt();
    float angle = Serial.parseFloat();

    // 残った改行コードなどを捨てる
    while (Serial.available() > 0) {
      Serial.read();
    }

    if (index < 0 || index >= SV_COUNT) {
      Serial.println("Error: index out of range (0-3)");
    } else {
      if (IS_270[index]) angle = angle * (180.0f / 270.0f);
      angle = constrain(angle, 0.0f, 180.0f);

      if (DIRECT_WRITE[index]) {1
        currentVal[index] = angle;
        state[index] = 0;
        servos[index].write((int)roundf(angle));
        Serial.print("SV");
        Serial.print(index);
        Serial.print(" Direct write: ");
        Serial.println(angle);
      } else {
        float dist = fabsf(angle - currentVal[index]);
        if (dist < 0.5f) {
          Serial.print("SV");
          Serial.print(index);
          Serial.println(" ... Already there.");
        } else {
          startVal[index] = currentVal[index];
          targetVal[index] = angle;
          motionDuration[index] = (uint32_t)(dist / SERVO_AVG_SPEED * 1000.0f);
          motionStartTime[index] = millis();
          state[index] = 1;

          Serial.print("SV");
          Serial.print(index);
          Serial.print(" Move to: ");
          Serial.println(angle);
        }
      }
    }
  }

  uint32_t now = millis();
  if (now - lastUpdateTime >= UPDATE_INTERVAL_MS) {
    lastUpdateTime = now;
    updateServos();
  }
}

void updateServos() {
  uint32_t now = millis();

  for (int i = 0; i < SV_COUNT; i++) {
    if (state[i] == 0) continue;

    uint32_t elapsed = now - motionStartTime[i];

    if (elapsed >= motionDuration[i]) {
      currentVal[i] = targetVal[i];
      state[i] = 0;
      Serial.print("SV");
      Serial.print(i);
      Serial.println(" ... Done.");
    } else {
      float t = (float)elapsed / (float)motionDuration[i];
      // サイン波イージング: 開始・終了を滑らかにして慣性を抑制
      float ease = (1.0f - cosf(PI * t)) / 2.0f;
      currentVal[i] = startVal[i] + (targetVal[i] - startVal[i]) * ease;
    }

    servos[i].write((int)roundf(currentVal[i]));
  }
}
