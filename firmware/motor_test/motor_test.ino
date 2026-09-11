// 串口遥控电机（BLE 遥控）：手机 -> ESP32(BLE) -> UART -> STM32 -> 电机
// 电机引脚：左组 pwmL=PA0, in1L=PB0, in2L=PB1；右组 pwmR=PA1, in1R=PB10, in2R=PB11
// 串口 USART1：PA9(TX), PA10(RX)，波特率 115200
const int pwmL = PA0, in1L = PB0, in2L = PB1;
const int pwmR = PA1, in1R = PB10, in2R = PB11;

int speed = 70;   // 当前速度档对应值（30~110）

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
  pinMode(pwmL, OUTPUT); pinMode(in1L, OUTPUT); pinMode(in2L, OUTPUT);
  pinMode(pwmR, OUTPUT); pinMode(in1R, OUTPUT); pinMode(in2R, OUTPUT);
  stopAll();
  Serial.begin(115200);
}

void loop() {
  if (Serial.available()) {
    char c = Serial.read();
    if (c == 'F' || c == 'f')           forward();
    else if (c == 'B' || c == 'b')      backward();
    else if (c == 'L' || c == 'l')      turnLeft();
    else if (c == 'R' || c == 'r')      turnRight();
    else if (c == 'S' || c == 's')      stopAll();
    else if (c >= '1' && c <= '9')      speed = 30 + (c - '1') * 10;  // 1=30 ... 9=110
  }
}
