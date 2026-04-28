// #include <Arduino.h>

// // ===============================
// // 输出模式选择
// // 1 = 串口监视器详细调试
// // 2 = Serial Plotter 画图
// // ===============================
// #define OUTPUT_MODE_MONITOR 1
// #define OUTPUT_MODE_PLOT    2

// #define OUTPUT_MODE OUTPUT_MODE_PLOT
// // 想画图时改成：
// // #define OUTPUT_MODE OUTPUT_MODE_PLOT

// // ===============================
// // UART1 引脚映射
// // RX = GPIO42
// // TX = GPIO41
// // ===============================
// static const int RS485_RX_PIN = 42;
// static const int RS485_TX_PIN = 41;

// HardwareSerial RS485Serial(1);

// // Modbus RTU 连续读2个寄存器
// // 从机地址 0x01，功能码 0x03，起始地址 0x0000，寄存器数量 0x0002
// static const uint8_t CMD_READ_2REGS[8] = {
//   0x01, 0x03, 0x00, 0x00, 0x00, 0x02, 0xC4, 0x0B
// };

// // 打印HEX到USB串口（仅Monitor模式用）
// void printHexToUsb(const uint8_t *data, size_t len) {
//   for (size_t i = 0; i < len; i++) {
//     if (data[i] < 0x10) {
//       Serial.print("0");
//     }
//     Serial.print(data[i], HEX);
//     if (i < len - 1) {
//       Serial.print(" ");
//     }
//   }
//   Serial.println();
// }

// // 清空UART接收缓冲
// void clearUartBuffer(HardwareSerial &uart) {
//   while (uart.available()) {
//     uart.read();
//   }
// }

// // 发送命令
// void sendReadCommand() {
//   RS485Serial.write(CMD_READ_2REGS, sizeof(CMD_READ_2REGS));
//   RS485Serial.flush();
// }

// // 接收响应并解析
// bool receiveAndParse(uint16_t &distance_mm, uint16_t &confidence,
//                      uint8_t *rxBuf, size_t &rxLen) {
//   rxLen = 0;
//   const unsigned long timeoutMs = 200;
//   unsigned long start = millis();

//   while (millis() - start < timeoutMs) {
//     while (RS485Serial.available()) {
//       uint8_t b = RS485Serial.read();
//       if (rxLen < 64) {
//         rxBuf[rxLen++] = b;
//       }
//     }

//     // 正常响应长度应为9字节：
//     // 01 03 04 DIST_H DIST_L CONF_H CONF_L CRC_L CRC_H
//     if (rxLen >= 9) {
//       break;
//     }
//   }

//   if (rxLen < 9) {
//     return false;
//   }

//   // 暂不做CRC校验，只做基本帧判断
//   if (rxBuf[0] != 0x01) return false;
//   if (rxBuf[1] != 0x03) return false;
//   if (rxBuf[2] != 0x04) return false;

//   distance_mm = ((uint16_t)rxBuf[3] << 8) | rxBuf[4];
//   confidence  = ((uint16_t)rxBuf[5] << 8) | rxBuf[6];

//   return true;
// }

// void setup() {
//   // USB串口：给 Monitor/Plotter 用
//   Serial.begin(115200);
//   delay(1000);

//   // UART1：给 RS485 通信用
//   RS485Serial.begin(38400, SERIAL_8N1, RS485_RX_PIN, RS485_TX_PIN);
//   clearUartBuffer(RS485Serial);

// #if OUTPUT_MODE == OUTPUT_MODE_MONITOR
//   Serial.println();
//   Serial.println("ESP32-S3 RS485 Laser Demo Start");
//   Serial.println("Mode: MONITOR");
//   Serial.println("UART1 -> RX: GPIO42, TX: GPIO41");
// #elif OUTPUT_MODE == OUTPUT_MODE_PLOT
//   // Plot模式下不要输出多余文字，避免干扰绘图
// #endif
// }

// void loop() {
//   uint16_t distance_mm = 0;
//   uint16_t confidence = 0;
//   uint8_t rxBuf[64];
//   size_t rxLen = 0;

//   clearUartBuffer(RS485Serial);
//   sendReadCommand();

//   bool ok = receiveAndParse(distance_mm, confidence, rxBuf, rxLen);

// #if OUTPUT_MODE == OUTPUT_MODE_MONITOR

//   Serial.println("------------------------------");
//   Serial.print("发送HEX: ");
//   printHexToUsb(CMD_READ_2REGS, sizeof(CMD_READ_2REGS));

//   if (ok) {
//     Serial.print("收到HEX: ");
//     printHexToUsb(rxBuf, rxLen);

//     Serial.print("解析距离: ");
//     Serial.print(distance_mm);
//     Serial.println(" mm");
//     .....................................................................................................................................................

//     Serial.print("解析置信度: ");
//     Serial.println(confidence);
//   } else {
//     if (rxLen > 0) {
//       Serial.print("收到HEX: ");
//       printHexToUsb(rxBuf, rxLen);
//       Serial.println("解析失败: 返回帧格式不符合预期");
//     } else {
//       Serial.println("收到HEX: <无数据>");
//     }
//   }

// #elif OUTPUT_MODE == OUTPUT_MODE_PLOT

//   // Plot模式只输出纯数字
//   // 第一列：distance_mm
//   // 第二列：confidence
//   // Serial Plotter 会画出两条线
//   if (ok) {
//     Serial.print(distance_mm);
//     Serial.print(",");
//     Serial.println(confidence);
//   }

// #endif

//   delay(100);
// }

#include <Arduino.h>

// ===============================
// 输出模式选择
// 1 = 串口监视器详细调试
// 2 = Serial Plotter 画图
// ===============================
#define OUTPUT_MODE_MONITOR 1
#define OUTPUT_MODE_PLOT    2

#define OUTPUT_MODE OUTPUT_MODE_PLOT
// 想画图时改成：
// #define OUTPUT_MODE OUTPUT_MODE_PLOT

// ===============================
// UART1 引脚映射
// RX = GPIO42
// TX = GPIO41
// ===============================
static const int RS485_RX_PIN = 42;
static const int RS485_TX_PIN = 41;

HardwareSerial RS485Serial(1);

// ===============================
// 传感器配置
// ===============================
static const uint8_t SENSOR_ADDRS[] = {0x01, 0x02, 0x03};
static const uint8_t SENSOR_COUNT = sizeof(SENSOR_ADDRS) / sizeof(SENSOR_ADDRS[0]);

static const uint16_t CONF_THRESHOLD = 80;

// 单个传感器请求间隔
// 传感器约30Hz，周期约33ms。
// 这里每个地址间隔40ms，三台轮询一圈约120ms。
static const uint32_t SENSOR_INTERVAL_MS = 40;

// 一帧响应等待超时
static const uint32_t RESPONSE_TIMEOUT_MS = 80;

// ===============================
// CRC16 Modbus
// ===============================
uint16_t modbusCRC16(const uint8_t *data, size_t len) {
  uint16_t crc = 0xFFFF;

  for (size_t i = 0; i < len; i++) {
    crc ^= data[i];

    for (uint8_t j = 0; j < 8; j++) {
      if (crc & 0x0001) {
        crc >>= 1;
        crc ^= 0xA001;
      } else {
        crc >>= 1;
      }
    }
  }

  return crc;
}

// ===============================
// 打印HEX
// ===============================
void printHexToUsb(const uint8_t *data, size_t len) {
  for (size_t i = 0; i < len; i++) {
    if (data[i] < 0x10) Serial.print("0");
    Serial.print(data[i], HEX);
    if (i < len - 1) Serial.print(" ");
  }
  Serial.println();
}

// ===============================
// 清空UART接收缓冲
// ===============================
void clearUartBuffer(HardwareSerial &uart) {
  while (uart.available()) {
    uart.read();
  }
}

// ===============================
// 构造读命令
// 读寄存器 0x0000 开始，连续2个寄存器
// ===============================
void buildReadCommand(uint8_t addr, uint8_t *cmd) {
  cmd[0] = addr;
  cmd[1] = 0x03;
  cmd[2] = 0x00;
  cmd[3] = 0x00;
  cmd[4] = 0x00;
  cmd[5] = 0x02;

  uint16_t crc = modbusCRC16(cmd, 6);

  // Modbus CRC低字节在前
  cmd[6] = crc & 0xFF;
  cmd[7] = (crc >> 8) & 0xFF;
}

// ===============================
// 发送命令
// ===============================
void sendReadCommand(uint8_t addr, uint8_t *cmd) {
  buildReadCommand(addr, cmd);
  RS485Serial.write(cmd, 8);
  RS485Serial.flush();
}

// ===============================
// 接收并解析
// 正常响应：
// addr 03 04 DIST_H DIST_L CONF_H CONF_L CRC_L CRC_H
// ===============================
bool receiveAndParse(uint8_t expectedAddr,
                     uint16_t &distance_mm,
                     uint16_t &confidence,
                     uint8_t *rxBuf,
                     size_t &rxLen) {
  rxLen = 0;
  unsigned long start = millis();

  while (millis() - start < RESPONSE_TIMEOUT_MS) {
    while (RS485Serial.available()) {
      uint8_t b = RS485Serial.read();
      if (rxLen < 64) {
        rxBuf[rxLen++] = b;
      }
    }

    if (rxLen >= 9) {
      break;
    }
  }

  if (rxLen < 9) {
    return false;
  }

  if (rxBuf[0] != expectedAddr) return false;
  if (rxBuf[1] != 0x03) return false;
  if (rxBuf[2] != 0x04) return false;

  uint16_t crcCalc = modbusCRC16(rxBuf, 7);
  uint16_t crcRecv = ((uint16_t)rxBuf[8] << 8) | rxBuf[7];

  if (crcCalc != crcRecv) {
    return false;
  }

  distance_mm = ((uint16_t)rxBuf[3] << 8) | rxBuf[4];
  confidence  = ((uint16_t)rxBuf[5] << 8) | rxBuf[6];

  if (confidence < CONF_THRESHOLD) {
    return false;
  }

  return true;
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  RS485Serial.begin(38400, SERIAL_8N1, RS485_RX_PIN, RS485_TX_PIN);
  clearUartBuffer(RS485Serial);

#if OUTPUT_MODE == OUTPUT_MODE_MONITOR
  Serial.println();
  Serial.println("ESP32-S3 RS485 Multi Laser Demo Start");
  Serial.println("Mode: MONITOR");
  Serial.println("UART1 -> RX: GPIO42, TX: GPIO41");
  Serial.println("Sensors: 0x01, 0x02, 0x03");
#elif OUTPUT_MODE == OUTPUT_MODE_PLOT
  // Plot模式不输出说明文字
#endif
}

void loop() {
  uint16_t distances[SENSOR_COUNT] = {0};
  uint16_t confidences[SENSOR_COUNT] = {0};
  bool valid[SENSOR_COUNT] = {false};

  for (uint8_t i = 0; i < SENSOR_COUNT; i++) {
    uint8_t addr = SENSOR_ADDRS[i];

    uint8_t txCmd[8];
    uint8_t rxBuf[64];
    size_t rxLen = 0;

    uint16_t distance_mm = 0;
    uint16_t confidence = 0;

    clearUartBuffer(RS485Serial);
    sendReadCommand(addr, txCmd);

    bool ok = receiveAndParse(addr, distance_mm, confidence, rxBuf, rxLen);

    if (ok) {
      distances[i] = distance_mm;
      confidences[i] = confidence;
      valid[i] = true;
    }

#if OUTPUT_MODE == OUTPUT_MODE_MONITOR

    Serial.println("------------------------------");
    Serial.print("传感器地址: 0x");
    if (addr < 0x10) Serial.print("0");
    Serial.println(addr, HEX);

    Serial.print("发送HEX: ");
    printHexToUsb(txCmd, 8);

    if (rxLen > 0) {
      Serial.print("收到HEX: ");
      printHexToUsb(rxBuf, rxLen);
    } else {
      Serial.println("收到HEX: <无数据>");
    }

    if (ok) {
      Serial.print("距离: ");
      Serial.print(distance_mm);
      Serial.println(" mm");

      Serial.print("置信度: ");
      Serial.println(confidence);
    } else {
      Serial.println("数据无效: 超时 / 帧格式错误 / CRC错误 / 置信度低于80");
    }

#endif

    delay(SENSOR_INTERVAL_MS);
  }

#if OUTPUT_MODE == OUTPUT_MODE_PLOT

  // Plotter模式只输出纯数字。
  // 无效数据输出0，避免Plotter断解析。
  //
  // 输出顺序：
  // sensor1_distance,sensor1_confidence,
  // sensor2_distance,sensor2_confidence,
  // sensor3_distance,sensor3_confidence

  for (uint8_t i = 0; i < SENSOR_COUNT; i++) {
    if (valid[i]) {
      Serial.print(distances[i]);
      Serial.print(",");
      Serial.print(confidences[i]);
    } else {
      Serial.print(0);
      Serial.print(",");
      Serial.print(0);
    }

    if (i < SENSOR_COUNT - 1) {
      Serial.print(",");
    }
  }

  Serial.println();

#endif
}