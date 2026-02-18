#include <pwm.h>
#include <Arduino_CAN.h>
#include <FastLED_NeoPixel.h>

//PWM関係
#define PWM_N 11
#define PWM_L 10
#define DIS 7
//センサー関係
#define ENC_A 2
#define ENC_B 3
#define ENC_X 15
#define SW_A 16
#define SW_B 17
#define SENS 14
#define INT 12
//通信関係
#define CAN_RX 5
#define CAN_TX 4
#define SCL 19
#define SDA 18
//LED関係
#define LED 13
#define RGB 6
//入力関係
#define SW0 8
#define SW1 9
#define SW2 1
#define SW3 0

//R4では"pwm.h"を用いることでPWM周波数を任意の値に指定できる。analogWrite()の上位互換
PwmOut pwm_n(PWM_N);
PwmOut pwm_l(PWM_L);

FastLED_NeoPixel<1, RGB, NEO_GRB> strip;  //RGBLED制御用

unsigned int CAN_ID;  //CANのID（DCモータドライバは0x100～0x102）

float duty;     //duty比
float maxDuty;  //duty比の最大値（大きすぎると危険なため）

void setup() {
  Serial.begin(115200);  //Serial初期化

  //pinMode宣言
  pinMode(PWM_N, OUTPUT);
  pinMode(PWM_L, OUTPUT);
  pinMode(DIS, OUTPUT);
  // pinMode(ENC_A, INPUT);
  // pinMode(ENC_B, INPUT);
  // pinMode(SENS, INPUT);
  pinMode(LED, OUTPUT);
  pinMode(RGB, OUTPUT);
  pinMode(SW_A, INPUT_PULLUP);
  pinMode(SW_B, INPUT_PULLUP);
  pinMode(SW0, INPUT_PULLUP);
  pinMode(SW1, INPUT_PULLUP);
  pinMode(SW2, INPUT_PULLUP);
  pinMode(SW3, INPUT_PULLUP);

  strip.begin();
  strip.setBrightness(100);

  //モータ暴走防止のための初期化
  digitalWrite(PWM_N, LOW);
  digitalWrite(PWM_L, LOW);
  digitalWrite(DIS, LOW);
  pwm_n.begin(30, 0);  //スイッチング周期30µs（≒33kHz）
  pwm_l.begin(30, 0);  //スイッチング周期30µs（≒33kHz）

  if (!CAN.begin(CanBitRate::BR_1000k)) {
    Serial.println("CAN.begin(...) failed.");
    while (1) {
      strip.setPixelColor(0, strip.Color(255, 0, 0));
      strip.show();
      delay(100);
      strip.setPixelColor(0, strip.Color(0, 0, 0));
      strip.show();
      delay(100);
    }
  }

  readSwitch();  //DIPスイッチの読み出してCAN_IDを指定
}

void loop() {
  //CAN受信
  if (CAN.available()) {
    CanMsg const msg = CAN.read();  //受信したCANの読み出してmsgに代入
    unsigned int rxId = msg.id;     //受信したCANのIDをrxIdに代入

    //IDが一致したらmsgをcommandとvalueに分離
    if (rxId == CAN_ID) {
      Serial.println("Received CAN!");

      unsigned char command = msg.data[0];
      long value = (msg.data[1] << 24) + (msg.data[2] << 16) + (msg.data[3] << 8) + msg.data[4];

      //commadが0x00であれば、dutyにvalueをfloat型にキャストしてdutyに代入（CANでは小数が扱えないから）
      if (command == 0x00) {
        duty = (float)value;
      }
    }
    Serial.print("duty : ");
    Serial.println(duty);
  }

  //Serial受信
  if (Serial.available()) {
    Serial.println("Received Serial!");
    duty = Serial.parseFloat();  //dutyにシリアルモニタで書き込んだ値を代入

    //Serialのバッファをリセットする（多分なくても動く）
    while (Serial.available() > 0) {
      char t = Serial.read();
    }
    Serial.print("duty : ");
    Serial.println(duty);
  }
  motorDrive(duty);
}

//DIPスイッチを読み取ってCAN_IDを指定する
void readSwitch() {
  unsigned int readNumber = !digitalRead(SW0) + 2 * !digitalRead(SW1) + 4 * !digitalRead(SW2) + 8 * !digitalRead(SW3);
  switch (readNumber) {
    case 0:
      strip.setPixelColor(0, strip.Color(255, 100, 0));
      break;
    case 1:
      strip.setPixelColor(0, strip.Color(255, 255, 0));
      break;
    case 2:
      strip.setPixelColor(0, strip.Color(0, 255, 0));
      break;
    case 3:
      strip.setPixelColor(0, strip.Color(0, 0, 255));
      break;
  }
  strip.show();
  CAN_ID = 0x100 + readNumber;
  Serial.print("CAN_ID:");
  Serial.println(CAN_ID);
}

//モータ駆動
void motorDrive(float motorPower) {
  if (motorPower > 0) {  //正転
    motorPower = constrain(motorPower, 0, maxDuty);
    pwm_n.pulse_perc(motorPower);
    pwm_l.pulse_perc(0);
  } else if (motorPower < 0) {  //逆転
    motorPower = constrain(motorPower, -maxDuty, 0);
    pwm_n.pulse_perc(0);
    pwm_l.pulse_perc(-motorPower);
  } else if (motorPower == 0) {  //停止
    pwm_l.pulse_perc(0);
    pwm_n.pulse_perc(0);
  }
}
