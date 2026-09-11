// ESP32-S3 电机方向自动测试（接线验证，循环执行）
// 序列：前进10s → 停2s → 后退10s → 停2s → 左转6s → 右转6s → 停2s → 循环
// 电机引脚：左组 pwmL=GPIO4, in1L=GPIO5, in2L=GPIO6；右组 pwmR=GPIO7, in1R=GPIO8, in2R=GPIO9
const int pwmL = 4, in1L = 5, in2L = 6;
const int pwmR = 7, in1R = 8, in2R = 9;

int speed = 70;   // 测试速度（30~110）

void setMotor(int pwm, int in1, int in2, int spd) {
  if (spd > 0)      { digitalWrite(in1, HIGH); digitalWrite(in2, LOW); }
  else if (spd < 0) { digitalWrite(in1, LOW);  digitalWrite(in2, HIGH); }
  else              { digitalWrite(in1, LOW);  digitalWrite(in2, LOW); }
  analogWrite(pwm, abs(spd));
}

void setLeft(int spd)  { setMotor(pwmL, in1L, in2L, spd); }
void setRight(int spd) { setMotor(pwmR, in1R, in2R, spd); }

void forward()   { setLeft(speed);  setRight(speed); }
void backward()  { setLeft(-speed); setRight(-speed); }
void turnLeft()  { setLeft(-speed); setRight(speed); }
void turnRight() { setLeft(speed);  setRight(-speed); }
void stopAll()   { setLeft(0);      setRight(0); }

void setup() {
  Serial.begin(115200);
  pinMode(pwmL, OUTPUT); pinMode(in1L, OUTPUT); pinMode(in2L, OUTPUT);
  pinMode(pwmR, OUTPUT); pinMode(in1R, OUTPUT); pinMode(in2R, OUTPUT);
  analogWriteFrequency(pwmL, 20000);   // 20kHz 避免啸叫
  analogWriteFrequency(pwmR, 20000);
  stopAll();
  Serial.println("motor test start");
}

void loop() {
  Serial.println("forward 10s");   forward();   delay(10000);
  Serial.println("stop 2s");       stopAll();   delay(2000);
  Serial.println("backward 10s");  backward();  delay(10000);
  Serial.println("stop 2s");       stopAll();   delay(2000);
  Serial.println("turn left 6s");  turnLeft();  delay(6000);
  Serial.println("turn right 6s"); turnRight(); delay(6000);
  Serial.println("stop 2s");       stopAll();   delay(2000);
}
