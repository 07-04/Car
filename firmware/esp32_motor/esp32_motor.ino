// ESP32-S3 单板 BLE 遥控电机（去掉 STM32，直接控制 TB6612 D24A）
// 手机 BLE App -> ESP32-S3 -> D24A -> 电机
// 电机引脚：左组 pwmL=GPIO4, in1L=GPIO5, in2L=GPIO6；右组 pwmR=GPIO7, in1R=GPIO8, in2R=GPIO9
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

#define SERVICE_UUID "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHAR_UUID_TX "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"  // ESP32 -> 手机 (Notify)
#define CHAR_UUID_RX "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"  // 手机 -> ESP32 (Write)

const int pwmL = 4, in1L = 5, in2L = 6;   // 左组电机（左前 D + 左后 A 并联）
const int pwmR = 7, in1R = 8, in2R = 9;   // 右组电机（右前 C + 右后 B 并联）

int speed = 70;   // 当前速度（30~110，对应按键 1~9）

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

void handleCmd(char c) {
  if (c == 'F' || c == 'f')           { forward();  state = 'F'; }
  else if (c == 'B' || c == 'b')      { backward(); state = 'B'; }
  else if (c == 'L' || c == 'l')      { turnLeft(); state = 'L'; }
  else if (c == 'R' || c == 'r')      { turnRight(); state = 'R'; }
  else if (c == 'S' || c == 's')      { stopAll();  state = 'S'; }
  else if (c >= '1' && c <= '9') {
    speed = 30 + (c - '1') * 10;   // 1=30 ... 9=110
    if (state == 'F') forward();       // 运动中改速度立即生效
    else if (state == 'B') backward();
    else if (state == 'L') turnLeft();
    else if (state == 'R') turnRight();
  }
}

class ServerCb : public BLEServerCallbacks {
  void onConnect(BLEServer* s)    { deviceConnected = true; stopAll(); Serial.println("BT connected"); }
  void onDisconnect(BLEServer* s) { deviceConnected = false; stopAll(); Serial.println("BT disconnected"); }  // 断连自动停车
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

  BLEDevice::init("RobotCar");
  BLEServer *pServer = BLEDevice::createServer();
  pServer->setCallbacks(new ServerCb());

  BLEService *pService = pServer->createService(SERVICE_UUID);

  pTxChar = pService->createCharacteristic(CHAR_UUID_TX, BLECharacteristic::PROPERTY_NOTIFY);
  pTxChar->addDescriptor(new BLE2902());

  BLECharacteristic *pRxChar = pService->createCharacteristic(CHAR_UUID_RX, BLECharacteristic::PROPERTY_WRITE);
  pRxChar->setCallbacks(new WriteCb());

  pService->start();
  pServer->getAdvertising()->start();
  Serial.println("RobotCar BLE + motor ready");
}

void loop() {
  delay(10);
}
