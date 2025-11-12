#include <ESP32Servo.h>
#include <NimBLEDevice.h>

// === 舵机定义 ===
Servo clawLeft, clawRight, wristRotate, wristSwing;

#define CLAW_LEFT_PIN 47
#define CLAW_RIGHT_PIN 41
#define WRIST_ROTATE_PIN 3
#define WRIST_SWING_PIN 2

int leftOpen = 35;
int leftClose = 120;
int rightOpen = 140;
int rightClose = 60;

bool clawClosed = false;
bool bleConnected = false;

// === BLE UUID (自定义 UART 服务) ===
#define SERVICE_UUID           "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_RX_UUID "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_TX_UUID "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"

NimBLECharacteristic* pTxCharacteristic;

// === 接收处理回调 ===
class MyCallbacks : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic* pCharacteristic) {
    std::string rxValue = pCharacteristic->getValue();
    if (rxValue.length() == 0) return;

    Serial.print("📩 接收到原始数据: ");
    for (size_t i = 0; i < rxValue.length(); i++) {
      Serial.printf("0x%02X ", (uint8_t)rxValue[i]);
    }
    Serial.println();

    String cmd = String(rxValue.c_str());
    cmd.trim();
    Serial.print("🧭 解码为字符串: ");
    Serial.println(cmd);

    // === 指令判断 ===
    if (cmd == "00") {
      clawClosed = !clawClosed;
      if (clawClosed) {
        clawLeft.write(leftClose);
        clawRight.write(rightClose);
        Serial.println("🦾 夹爪闭合");
        pTxCharacteristic->setValue("claw_closed");
      } else {
        clawLeft.write(leftOpen);
        clawRight.write(rightOpen);
        Serial.println("🖐️ 夹爪打开");
        pTxCharacteristic->setValue("claw_open");
      }
    }
    else if (cmd == "360") {
      wristRotate.writeMicroseconds(1700);
      Serial.println("↻ 腕部正转");
      pTxCharacteristic->setValue("rotate_cw");
    }
    else if (cmd == "-360") {
      wristRotate.writeMicroseconds(1300);
      Serial.println("↺ 腕部反转");
      pTxCharacteristic->setValue("rotate_ccw");
    }
    else if (cmd == "S" || cmd == "s") {
      wristRotate.writeMicroseconds(1500);
      Serial.println("🛑 腕部停止");
      pTxCharacteristic->setValue("stop");
    }
    else if (cmd == "1") {
      wristSwing.write(0);
      Serial.println("↔ 摆动舵机转到 0°");
      pTxCharacteristic->setValue("swing_0");
    }
    else if (cmd == "2") {
      wristSwing.write(90);
      Serial.println("↔ 摆动舵机转到 90°");
      pTxCharacteristic->setValue("swing_90");
    }
    else if (cmd == "3") {
      wristSwing.write(180);
      Serial.println("↔ 摆动舵机转到 180°");
      pTxCharacteristic->setValue("swing_180");
    }
    else {
      Serial.println("⚠️ 未识别的指令");
      pTxCharacteristic->setValue("unknown");
    }

    // 通知手机（反馈）
    pTxCharacteristic->notify();
  }
};

// === 服务器回调 ===
class MyServerCallbacks : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer* pServer) {
    bleConnected = true;
    Serial.println("✅ 蓝牙已连接！");
  }
  void onDisconnect(NimBLEServer* pServer) {
    bleConnected = false;
    Serial.println("❌ 蓝牙断开，重新广播中...");
    NimBLEDevice::startAdvertising();
  }
};

void setup() {
  Serial.begin(115200);
  Serial.println("\n=== ESP32-S3 BLE 舵机控制 启动 ===");

  // 舵机初始化
  clawLeft.attach(CLAW_LEFT_PIN);
  clawRight.attach(CLAW_RIGHT_PIN);
  wristRotate.attach(WRIST_ROTATE_PIN);
  wristSwing.attach(WRIST_SWING_PIN);
  clawLeft.write(leftOpen);
  clawRight.write(rightOpen);
  wristRotate.writeMicroseconds(1500);
  wristSwing.write(90);
  Serial.println("✅ 舵机初始化完成");

  // BLE 初始化
  NimBLEDevice::init("ESP32_Servo_BLE");
  NimBLEServer* pServer = NimBLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  NimBLEService* pService = pServer->createService(SERVICE_UUID);

  // TX (发送给手机)
  pTxCharacteristic = pService->createCharacteristic(
      CHARACTERISTIC_TX_UUID,
      NIMBLE_PROPERTY::NOTIFY
  );

  // RX (接收手机数据)
  NimBLECharacteristic* pRxCharacteristic = pService->createCharacteristic(
      CHARACTERISTIC_RX_UUID,
      NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR
  );
  pRxCharacteristic->setCallbacks(new MyCallbacks());

  pService->start();

  // 广播
  NimBLEAdvertising* pAdvertising = NimBLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setName("ESP32_Servo_Control");
  pAdvertising->start();

  Serial.println("📡 BLE 已启动并广播中... 可用 LightBlue / 蓝牙调试助手 连接");
}

void loop() {
  delay(100);
}
