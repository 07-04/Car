// ESP32-S3 BLE 串口透传（Nordic UART Service / NUS）
// 手机 BLE App <-> ESP32-S3 <-> STM32
// 串口接 STM32：GPIO17(TX) -> STM32 PA10(RX)，GPIO18(RX) <- STM32 PA9(TX)
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

#define SERVICE_UUID "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHAR_UUID_TX "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"  // ESP32 -> 手机 (Notify)
#define CHAR_UUID_RX "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"  // 手机 -> ESP32 (Write)

BLECharacteristic *pTxChar;
bool deviceConnected = false;

class ServerCb : public BLEServerCallbacks {
  void onConnect(BLEServer* s)    { deviceConnected = true; }
  void onDisconnect(BLEServer* s) { deviceConnected = false; }
};

class WriteCb : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pChar) {
    String v = pChar->getValue();
    if (v.length() > 0) {
      for (int i = 0; i < (int)v.length(); i++) {
        Serial1.print(v[i]);       // 手机发来的字符，串口转给 STM32
      }
    }
  }
};

void setup() {
  Serial.begin(115200);                        // USB 调试串口
  Serial1.begin(115200, SERIAL_8N1, 18, 17);   // UART1: RX=GPIO18, TX=GPIO17 接 STM32

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
  Serial.println("BLE NUS ready, name: RobotCar");
}

void loop() {
  // STM32 回传数据 -> BLE notify 给手机（回显，可留空）
  if (Serial1.available()) {
    String s = Serial1.readString();
    if (deviceConnected) {
      pTxChar->setValue(s);
      pTxChar->notify();
    }
  }
  delay(10);
}
