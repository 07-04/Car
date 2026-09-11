// ============================================
// 肉牛养殖场巡检机器人 - TB6612 电机测试
// 两片 TB6612，每片管一侧（左组/右组）
// 每组 2 个电机，PWM 和方向引脚并联（同向同速）
// ============================================

// 左组（TB6612 #1）
#define L_PWM 4
#define L_IN1 16
#define L_IN2 17

// 右组（TB6612 #2）
#define R_PWM 5
#define R_IN1 18
#define R_IN2 19

void setMotor(int pwm, int in1, int in2, int speed) {
  speed = constrain(speed, -255, 255);
  if (speed > 0) {           // 正转
    digitalWrite(in1, HIGH);
    digitalWrite(in2, LOW);
    analogWrite(pwm, speed);
  } else if (speed < 0) {    // 反转
    digitalWrite(in1, LOW);
    digitalWrite(in2, HIGH);
    analogWrite(pwm, -speed);
  } else {                   // 停止
    digitalWrite(in1, LOW);
    digitalWrite(in2, LOW);
    analogWrite(pwm, 0);
  }
}

void setup() {
  pinMode(L_PWM, OUTPUT);
  pinMode(L_IN1, OUTPUT);
  pinMode(L_IN2, OUTPUT);
  pinMode(R_PWM, OUTPUT);
  pinMode(R_IN1, OUTPUT);
  pinMode(R_IN2, OUTPUT);

  setMotor(L_PWM, L_IN1, L_IN2, 0);
  setMotor(R_PWM, R_IN1, R_IN2, 0);
  delay(500);
}

void loop() {
  // 前进 2 秒
  setMotor(L_PWM, L_IN1, L_IN2, 150);
  setMotor(R_PWM, R_IN1, R_IN2, 150);
  delay(2000);

  // 后退 2 秒
  setMotor(L_PWM, L_IN1, L_IN2, -150);
  setMotor(R_PWM, R_IN1, R_IN2, -150);
  delay(2000);

  // 原地左转 1.5 秒（左反转、右正转）
  setMotor(L_PWM, L_IN1, L_IN2, -150);
  setMotor(R_PWM, R_IN1, R_IN2, 150);
  delay(1500);

  // 停 1 秒
  setMotor(L_PWM, L_IN1, L_IN2, 0);
  setMotor(R_PWM, R_IN1, R_IN2, 0);
  delay(1000);
}
