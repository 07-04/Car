// ============================================
// 肉牛养殖场巡检机器人 - STM32F103C8T6 执行层（方案B）
// 职责：接收 ESP32 串口运动指令 → 驱动 TB6612
//       读取传感器（SHT30 / MLX90614 / HC-SR04×3）→ JSON 上报
// ============================================
// 接线（详见 项目落地文档.md 1.4）：
//   左组 TB6612#1: PWM=PA0  IN1=PB0  IN2=PB1
//   右组 TB6612#2: PWM=PA1  IN1=PB10 IN2=PB11
//   避障: 前 Trig=PA4 Echo=PA5 | 左 PA6/PA7 | 右 PB12/PB13
//   I2C:  SCL=PB6 SDA=PB7 (SHT30=0x44, MLX90614=0x5A 并联)
//   串口: USART1 TX=PA9 RX=PA10 → 接 ESP32(GPIO16/17)
// ============================================
// 烧录环境：
//   Arduino IDE + STM32duino(STM32F1) 核心 + ST-Link V2 烧录
//   库：Adafruit SHT31 Library、Adafruit MLX90614 Library、Adafruit BusIO
// ============================================
#include <Wire.h>
#include <Adafruit_SHT31.h>
#include <Adafruit_MLX90614.h>

// ---------- 电机引脚 ----------
#define L_PWM PA0
#define L_IN1 PB0
#define L_IN2 PB1
#define R_PWM PA1
#define R_IN1 PB10
#define R_IN2 PB11

// ---------- 超声波 ----------
#define TRIG_F PA4
#define ECHO_F PA5
#define TRIG_L PA6
#define ECHO_L PA7
#define TRIG_R PB12
#define ECHO_R PB13

Adafruit_SHT31 sht;
Adafruit_MLX90614 mlx;
bool hasSht = false, hasMlx = false;

int  speed = 70;               // 0~100，映射到 0~255 PWM
int  cmd   = 0;                // 0停 1前 2后 3左 4右
unsigned long lastCmd = 0;
const unsigned long TIMEOUT_MS = 800;   // 失联 800ms 自动停车
String inBuf = "";

void setMotor(int pwm, int in1, int in2, int spd) {
  spd = constrain(spd, -255, 255);
  if (spd > 0)      { digitalWrite(in1, HIGH); digitalWrite(in2, LOW);  analogWrite(pwm, spd); }
  else if (spd < 0) { digitalWrite(in1, LOW);  digitalWrite(in2, HIGH); analogWrite(pwm, -spd); }
  else              { digitalWrite(in1, LOW);  digitalWrite(in2, LOW);  analogWrite(pwm, 0); }
}

void drive(int dir, int spd) {
  int p = map(constrain(spd, 0, 100), 0, 100, 0, 255);
  switch (dir) {
    case 1:  // 前进：左右同向正转
      setMotor(L_PWM, L_IN1, L_IN2,  p);
      setMotor(R_PWM, R_IN1, R_IN2,  p);
      break;
    case 2:  // 后退
      setMotor(L_PWM, L_IN1, L_IN2, -p);
      setMotor(R_PWM, R_IN1, R_IN2, -p);
      break;
    case 3:  // 原地左转：左反右正
      setMotor(L_PWM, L_IN1, L_IN2, -p);
      setMotor(R_PWM, R_IN1, R_IN2,  p);
      break;
    case 4:  // 原地右转：左正右反
      setMotor(L_PWM, L_IN1, L_IN2,  p);
      setMotor(R_PWM, R_IN1, R_IN2, -p);
      break;
    default: // 停车
      setMotor(L_PWM, L_IN1, L_IN2, 0);
      setMotor(R_PWM, R_IN1, R_IN2, 0);
  }
}

float readDist(int trig, int echo) {
  digitalWrite(trig, LOW);
  delayMicroseconds(2);
  digitalWrite(trig, HIGH);
  delayMicroseconds(10);
  digitalWrite(trig, LOW);
  long us = pulseIn(echo, HIGH, 30000);   // 超时 30ms ≈ 5m
  if (us == 0) return -1.0;               // 超时/无回波
  return us * 0.034 / 2.0;                // cm
}

void reportSensors() {
  float t  = hasSht ? sht.readTemperature() : -1;
  float h  = hasSht ? sht.readHumidity()    : -1;
  float ir = hasMlx ? mlx.readObjectTempC() : -1;
  float df = readDist(TRIG_F, ECHO_F);
  float dl = readDist(TRIG_L, ECHO_L);
  float dr = readDist(TRIG_R, ECHO_R);

  Serial.print("{\"temp\":");   Serial.print(t, 1);
  Serial.print(",\"humi\":");   Serial.print(h, 1);
  Serial.print(",\"ir\":");     Serial.print(ir, 1);
  Serial.print(",\"dist_f\":"); Serial.print(df, 0);
  Serial.print(",\"dist_l\":"); Serial.print(dl, 0);
  Serial.print(",\"dist_r\":"); Serial.print(dr, 0);
  Serial.println("}");
}

void setup() {
  pinMode(L_PWM, OUTPUT); pinMode(L_IN1, OUTPUT); pinMode(L_IN2, OUTPUT);
  pinMode(R_PWM, OUTPUT); pinMode(R_IN1, OUTPUT); pinMode(R_IN2, OUTPUT);
  drive(0, 0);

  pinMode(TRIG_F, OUTPUT); pinMode(ECHO_F, INPUT);
  pinMode(TRIG_L, OUTPUT); pinMode(ECHO_L, INPUT);
  pinMode(TRIG_R, OUTPUT); pinMode(ECHO_R, INPUT);

  Serial.begin(115200);   // USART1, PA9/PA10 → ESP32

  Wire.begin();           // PB6/PB7
  hasSht = sht.begin(0x44);   // SHT30 默认地址 0x44
  hasMlx = mlx.begin();       // MLX90614 默认地址 0x5A
}

void loop() {
  // 收串口指令（ESP32 → "F\n"/"B\n"/"L\n"/"R\n"/"S\n"/"V<n>\n"）
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == '\n') {
      inBuf.trim();
      if (inBuf.length() == 1) {
        switch (inBuf[0]) {
          case 'F': cmd = 1; break;
          case 'B': cmd = 2; break;
          case 'L': cmd = 3; break;
          case 'R': cmd = 4; break;
          case 'S': cmd = 0; break;
        }
      } else if (inBuf[0] == 'V') {
        speed = constrain(inBuf.substring(1).toInt(), 0, 100);
      }
      lastCmd = millis();
      inBuf = "";
    } else {
      inBuf += c;
    }
  }

  // 失联自动停车
  if (cmd != 0 && millis() - lastCmd > TIMEOUT_MS) cmd = 0;

  drive(cmd, speed);

  // 每 1 秒上报一次传感器
  static unsigned long lastRep = 0;
  if (millis() - lastRep >= 1000) {
    lastRep = millis();
    reportSensors();
  }
}
