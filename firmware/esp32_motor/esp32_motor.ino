// ESP32-S3 单板：BLE 遥控 + 超声波/红外自主避障（直接控制 TB6612 D24A）
// 手机 BLE App -> ESP32-S3 -> D24A -> 电机
// 电机引脚：左组 pwmL=GPIO4, in1L=GPIO5, in2L=GPIO6；右组 pwmR=GPIO7, in1R=GPIO8, in2R=GPIO9
// 避障传感器：HC-SR04 Trig=GPIO10 Echo=GPIO11；KY-032 左=GPIO12 右=GPIO13（OUT 低电平=有障碍）
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

#define SERVICE_UUID "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHAR_UUID_TX "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"  // ESP32 -> 手机 (Notify)
#define CHAR_UUID_RX "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"  // 手机 -> ESP32 (Write)

// 电机
const int pwmL = 4, in1L = 5, in2L = 6;   // 左组电机（左前 D + 左后 A 并联）
const int pwmR = 7, in1R = 8, in2R = 9;   // 右组电机（右前 C + 右后 B 并联）

// 避障传感器
const int trigPin = 10;   // HC-SR04 Trig（3.3V 直连）
const int echoPin = 11;   // HC-SR04 Echo（3.3V 供电直连，无分压）
const int irLeft  = 12;   // KY-032 左（OUT 低电平 = 有障碍）
const int irRight = 13;   // KY-032 右

int speed = 70;           // 当前速度（30~110，对应按键 1~9）
const int avoidDist = 30; // 前方避障触发距离（cm）
const int backDist  = 15; // 太近（<15cm）先强制后退再转

bool autoMode = false;    // 自主避障模式（BLE 'A' 开启，'S'/方向键关闭）

// 避障状态机（转向/后退有最短持续时间，避免“刚转一点又往前撞”）
int  avoidState = 0;               // 0=直行 1=左转 2=右转 3=后退
unsigned long stateEnd = 0;
const unsigned long TURN_MS = 450; // 转向最短 450ms
const unsigned long BACK_MS = 350; // 后退最短 350ms

BLECharacteristic *pTxChar;
bool deviceConnected = false;

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

char state = 'S';   // 当前运动状态（F/B/L/R/S），速度档立即重跑用

// ---- 传感器读取 ----
long readDistance() {          // 返回前方距离（cm），超时返回 999
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  long dur = pulseIn(echoPin, HIGH, 30000);   // 30ms 超时 ≈ 5m
  if (dur == 0) return 999;                    // 无回波，视为远处无近障碍
  return dur * 17 / 1000;                      // cm（声速 340m/s，往返）
}

bool leftBlocked()  { return digitalRead(irLeft)  == LOW; }   // KY-032 低电平 = 有障碍
bool rightBlocked() { return digitalRead(irRight) == LOW; }

// ---- 自主避障一步 ----
void avoidStep() {
  long dist = readDistance();
  bool l = leftBlocked();
  bool r = rightBlocked();

  Serial.printf("dist=%ldcm L=%d R=%d st=%d\n", dist, l, r, avoidState);

  // 正在执行带时长的动作，先跑完再重新判断
  if (avoidState != 0 && millis() < stateEnd) {
    if (avoidState == 1)      turnLeft();
    else if (avoidState == 2) turnRight();
    else                      backward();
    return;
  }
  avoidState = 0;

  if (dist > 0 && dist < avoidDist) {          // 前方有障碍
    if (dist < backDist) {                     // 太近：先退
      avoidState = 3; stateEnd = millis() + BACK_MS; backward();
    } else if (l && r) {                       // 死胡同：先退
      avoidState = 3; stateEnd = millis() + BACK_MS; backward();
    } else if (l) {                            // 左挡右空：右转
      avoidState = 2; stateEnd = millis() + TURN_MS; turnRight();
    } else {                                   // 右挡或都空：左转
      avoidState = 1; stateEnd = millis() + TURN_MS; turnLeft();
    }
  } else if (l && !r) turnRight();             // 前方空，左挡：右偏
  else if (r && !l)   turnLeft();              // 前方空，右挡：左偏
  else                forward();               // 都空（或都挡，窄缝直行）
}

void handleCmd(char c) {
  if (c == 'F' || c == 'f')           { autoMode = false; forward();  state = 'F'; }
  else if (c == 'B' || c == 'b')      { autoMode = false; backward(); state = 'B'; }
  else if (c == 'L' || c == 'l')      { autoMode = false; turnLeft(); state = 'L'; }
  else if (c == 'R' || c == 'r')      { autoMode = false; turnRight(); state = 'R'; }
  else if (c == 'S' || c == 's')      { autoMode = false; stopAll();  state = 'S'; }
  else if (c == 'A' || c == 'a')      { autoMode = true;  state = 'A'; Serial.println("auto avoid ON"); }
  else if (c >= '1' && c <= '9') {
    speed = 30 + (c - '1') * 10;   // 1=30 ... 9=110
    if (!autoMode) {                   // 手动运动中改速度立即生效
      if (state == 'F') forward();
      else if (state == 'B') backward();
      else if (state == 'L') turnLeft();
      else if (state == 'R') turnRight();
    }
  }
}

class ServerCb : public BLEServerCallbacks {
  void onConnect(BLEServer* s)    { deviceConnected = true; autoMode = false; stopAll(); Serial.println("BT connected"); }
  void onDisconnect(BLEServer* s) { deviceConnected = false; autoMode = false; stopAll(); Serial.println("BT disconnected"); s->getAdvertising()->start(); }  // 断连自动停车 + 重新广播
};

class WriteCb : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pChar) {
    String v = pChar->getValue();
    Serial.printf("RX %d bytes:", v.length());
    for (int i = 0; i < (int)v.length(); i++) Serial.printf(" 0x%02X", (uint8_t)v[i]);
    Serial.println();
    for (int i = 0; i < (int)v.length(); i++) handleCmd(v[i]);
  }
};

void setup() {
  Serial.begin(115200);

  pinMode(pwmL, OUTPUT); pinMode(in1L, OUTPUT); pinMode(in2L, OUTPUT);
  pinMode(pwmR, OUTPUT); pinMode(in1R, OUTPUT); pinMode(in2R, OUTPUT);
  analogWriteFrequency(pwmL, 20000);   // 20kHz，避免电机啸叫
  analogWriteFrequency(pwmR, 20000);
  stopAll();

  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
  pinMode(irLeft, INPUT);
  pinMode(irRight, INPUT);

  BLEDevice::init("RobotCar");
  BLEDevice::setPower(ESP_PWR_LVL_P9);   // 最大发射功率，增强连接稳定性
  BLEServer *pServer = BLEDevice::createServer();
  pServer->setCallbacks(new ServerCb());

  BLEService *pService = pServer->createService(SERVICE_UUID);

  pTxChar = pService->createCharacteristic(CHAR_UUID_TX, BLECharacteristic::PROPERTY_NOTIFY);
  pTxChar->addDescriptor(new BLE2902());

  BLECharacteristic *pRxChar = pService->createCharacteristic(CHAR_UUID_RX, BLECharacteristic::PROPERTY_WRITE);
  pRxChar->setCallbacks(new WriteCb());

  pService->start();
  pServer->getAdvertising()->start();
  Serial.println("RobotCar BLE + motor + avoid ready");
}

void loop() {
  if (autoMode) {
    avoidStep();
    delay(50);        // 小步进，连续避障
  } else {
    delay(10);
  }
}
