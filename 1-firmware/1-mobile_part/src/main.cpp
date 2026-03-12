#include <Arduino.h>
#include <micro_ros_platformio.h>
#include <rmw_microros/rmw_microros.h>
#include <SPI.h>
#include <Ethernet.h>
#include <EthernetUdp.h>
#include <rclc/rclc.h>
#include <rclc/executor.h>
#include <std_msgs/msg/float32.h>
#include <std_msgs/msg/bool.h>


/**
 * env_sensor header file
 */
#include "Adafruit_BME280.h"
#include "Adafruit_Sensor.h"
#include "Wire.h"

#define RCCHECK(fn)                   \
  {                                   \
    rcl_ret_t temp_rc = fn;           \
    if ((temp_rc != RCL_RET_OK))      \
    {                                 \
      Serial.print("Error at line "); \
      Serial.print(__LINE__);         \
      Serial.print(": ");             \
      Serial.println((int)temp_rc);   \
      while (1)                       \
        delay(1000);                  \
    }                                 \
  }

#define RCSOFTCHECK(fn)                    \
  {                                        \
    rcl_ret_t temp_rc = fn;                \
    if ((temp_rc != RCL_RET_OK))           \
    {                                      \
      Serial.print("Soft error at line "); \
      Serial.print(__LINE__);              \
      Serial.print(": ");                  \
      Serial.println((int)temp_rc);        \
    }                                      \
  }

/**
 * env sensor iic config
 */
#define I2C_SDA 33
#define I2C_SCL 34
#define I2C_ADR 0x77

Adafruit_BME280 env_sensor;

/**
 * env sensor data struct
 */
typedef struct
{
  float temperature;
  float humidity;
  bool valid;
  uint32_t update_ms;
}SensorData_t;
SensorData_t g_sensor_data = {0.0f, 0.0f, false};
SemaphoreHandle_t sensor_data_mutex = NULL;     

/**
 * microROS state machine
 */
typedef enum
{
  AGENT_DISCONNECTED = 0,
  AGENT_CONNECTING,
  AGENT_CONNECTED
} AgentState_t;
AgentState_t g_agent_state = AGENT_DISCONNECTED;


/**
 *  W5500 Pin Definitions
 */
#define W5500_CS 14   // Chip Select pin
#define W5500_RST 9   // Reset pin (optional, not used here)
#define W5500_INT 10  // Interrupt pin (optional, not used here)
#define W5500_MISO 12 // MISO pin
#define W5500_MOSI 11 // MOSI pin
#define W5500_SCK 13  // Clock pin

/**
 * Static IP Configuration
 */
byte mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED};
IPAddress client_ip(192, 168, 1, 177);
IPAddress dns(192, 168, 1, 1);
IPAddress gateway(192, 168, 1, 1);
IPAddress netmask(255, 255, 255, 0);
IPAddress agent_ip(192, 168, 1, 199);
const uint16_t agent_port = 8888;
const uint16_t client_port = 8889;

/**
 * micro-ROS Object
 */
rcl_allocator_t allocator;
rclc_support_t support;
rclc_executor_t executor;
rcl_node_t node;
rcl_publisher_t temp_pub;
rcl_publisher_t hum_pub;
rcl_publisher_t inwater_pub;
std_msgs__msg__Float32 temp_msg;
std_msgs__msg__Float32 hum_msg;
std_msgs__msg__Bool inwater_msg;

/**
 * UDP Object;
 */
EthernetUDP udp;
bool udp_opened = false;

/**
 * Water sensor 
 */
#define WATERSENSOR_PIN 38
#define SONAR_ENABLE 39
volatile bool isInWater = false;


// ============================================================
// 声明函数
// ============================================================
bool init_env_sensor();
bool init_ethernet();
bool init_water_sensor();
bool create_microros_entities();
void destroy_microros_entities();
bool ping_agent();
bool check_agent_alive();
void env_sensor_task(void *parameter);
void micro_ros_task(void *parameter);


/**
 * custom UDP open function
 */
bool custom_udp_transport_open(uxrCustomTransport *transport)
{
  (void)transport;

  if (udp_opened)
  {
    Serial.println("udp already open");
    return true;
  }

  udp_opened = (udp.begin(client_port) == 1);
  if (udp_opened)
  {
    Serial.print("[UDP] begin ok, local port=");
    Serial.println(client_port);
  }
  else
  {
    Serial.println("[UDP] begin failed");
  }
  return udp_opened;
}

/**
 * custom UDP close function
 */
bool custom_udp_transport_close(uxrCustomTransport *transport)
{
  (void)transport;
  if (udp_opened)
  {
    udp.stop();
    udp_opened = false;
    Serial.println("[UDP] closed");
  }
  return true;
}

/**
 * custom UDP write function
 */
size_t custom_udp_transport_write(struct uxrCustomTransport *transport, const uint8_t *buf, size_t len, uint8_t *errcode)
{
  (void)transport;
  (void)errcode;
  udp.beginPacket(agent_ip, agent_port);
  size_t written = udp.write(buf, len);
  udp.endPacket();
  return written;
}

/**
 * custom UDP read function
 */
size_t custom_udp_transport_read(struct uxrCustomTransport *transport, uint8_t *buf, size_t len, int timeout, uint8_t *errcode)
{
  (void)transport;
  (void)errcode;
  uint32_t start = millis();
  while ((millis() - start) < (uint32_t)timeout)
  {
    int packet_size = udp.parsePacket();
    if (packet_size > 0)
    {
      int n = packet_size > (int)len ? (int)len : packet_size;
      return udp.read(buf, n);
    }
    vTaskDelay(pdMS_TO_TICKS(1));
  }
  return 0;
}

/**
 * Init env sensor
 */
bool init_env_sensor()
{
  Wire.begin(I2C_SDA, I2C_SCL, 400000);
  delay(100);

  if (!env_sensor.begin(I2C_ADR))
  {
    Serial.println("[Env Sensor] sensor not found!");
    return false;
  }

  Serial.println("[Env Sensor] sensor OK");
  return true;
}

/**
 * Init ethernet
 */
bool init_ethernet()
{
  Serial.println("[ETH] SPI begin...");
  SPI.begin(W5500_SCK, W5500_MISO, W5500_MOSI, W5500_CS);

  Ethernet.init(W5500_CS);

  Serial.println("[ETH] Ethernet.begin(...)");
  Ethernet.begin(mac, client_ip, dns, gateway, netmask);
  delay(1000);

  IPAddress local = Ethernet.localIP();
  Serial.print("[ETH] local IP = ");
  Serial.println(local);

  if (local[0] == 0 && local[1] == 0 && local[2] == 0 && local[3] == 0)
  {
    Serial.println("[ETH] invalid local IP");
    return false;
  }

  return true;
}

/**
 * Init water sensor & sonar enable pin
 */
bool init_water_sensor()
{
  pinMode(WATERSENSOR_PIN, INPUT_PULLUP);
  Serial.println("[WATER_SENSOR] Init done");
  pinMode(SONAR_ENABLE, OUTPUT);
  Serial.println("[SONAR] Init done");
  return true;
}

/**
 * Ping agent
 */
bool ping_agent()
{
  rmw_ret_t ret = rmw_uros_ping_agent(100, 1);
  return (ret == RMW_RET_OK);
}

/**
 * check agent alive
 */
bool check_agent_alive()
{
  rmw_ret_t ret = rmw_uros_ping_agent(100, 1);
  return (ret == RMW_RET_OK);
}

/**
 * create ros node and publisher
 */
bool create_microros_entities()
{
  Serial.println("[micro-ROS] create node and publisher begin");

  allocator = rcl_get_default_allocator();

  // 先清空对象，防止误用旧对象
  support = (rclc_support_t){0};
  node = rcl_get_zero_initialized_node();
  temp_pub = rcl_get_zero_initialized_publisher();
  hum_pub = rcl_get_zero_initialized_publisher();
  inwater_pub = rcl_get_zero_initialized_publisher();

  RCCHECK(rclc_support_init(&support, 0, NULL, &allocator));

  RCCHECK(rclc_node_init_default(&node, "esp32_mobile_node", "", &support));

  RCCHECK(rclc_publisher_init_default(&temp_pub, &node, ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, Float32), "/env_sensor/temperature"));

  RCCHECK(rclc_publisher_init_default(&hum_pub, &node, ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, Float32), "/env_sensor/humidity"));

  RCCHECK(rclc_publisher_init_default(&inwater_pub, &node, ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, Bool), "/water_sensor"));

  Serial.println("[micro-ROS] create node and publisher ok");
  return true;
}

/**
 * destroy ros node and pub
 */
void destroy_microros_entities()
{
  Serial.println("[micro-ROS] destroy node&pub begin");

  // publisher fini
  if (temp_pub.impl != NULL)
  {
    RCSOFTCHECK(rcl_publisher_fini(&temp_pub, &node));
    temp_pub = rcl_get_zero_initialized_publisher();
  }

  if (hum_pub.impl != NULL)
  {
    RCSOFTCHECK(rcl_publisher_fini(&hum_pub, &node));
    hum_pub = rcl_get_zero_initialized_publisher();
  }

  if (inwater_pub.impl != NULL)
  {
    RCSOFTCHECK(rcl_publisher_fini(&inwater_pub, &node));
    inwater_pub = rcl_get_zero_initialized_publisher();
  }

  // node fini
  if (node.impl != NULL)
  {
    RCSOFTCHECK(rcl_node_fini(&node));
    node = rcl_get_zero_initialized_node();
  }

  // support 中 context 是否有效由 fini 内部处理
  // 某些版本可直接 fini support
  RCSOFTCHECK(rclc_support_fini(&support));
  support = (rclc_support_t){0};

  Serial.println("[micro-ROS] destroy node&pub done");
}

/**
 * env sensor freeRTOS task
 */
void env_sensor_task(void *parameter)
{
  (void)parameter;

  const TickType_t period = pdMS_TO_TICKS(1000); // 1Hz
  TickType_t last_wake_time = xTaskGetTickCount();

  for (;;)
  {
    float t = env_sensor.readTemperature();
    float h = env_sensor.readHumidity();

    bool data_ok = !(isnan(t) || isnan(h));

    if (data_ok)
    {
      if (xSemaphoreTake(sensor_data_mutex, pdMS_TO_TICKS(50)) == pdTRUE)
      {
        g_sensor_data.temperature = t;
        g_sensor_data.humidity = h;
        g_sensor_data.valid = true;
        g_sensor_data.update_ms = millis();
        xSemaphoreGive(sensor_data_mutex);
      }

      Serial.print("[sensor] T=");
      Serial.print(t);
      Serial.print(" C, H=");
      Serial.print(h);
      Serial.println(" %");
    }
    else
    {
      Serial.println("[sensor] read failed (NaN)");
    }

    vTaskDelayUntil(&last_wake_time, period);
  }
}


// ============================================================
// micro-ROS 状态机任务
//
// 状态说明：
// 1) AGENT_DISCONNECTED
//    - 周期 ping agent
//    - 如果 agent 在线，进入 CONNECTING
//
// 2) AGENT_CONNECTING
//    - 创建 node / publisher
//    - 成功后进入 CONNECTED
//    - 失败则销毁残留对象，退回 DISCONNECTED
//
// 3) AGENT_CONNECTED
//    - 周期读取共享数据并发布
//    - 同时定期检测 agent 是否掉线
//    - 掉线则销毁实体并退回 DISCONNECTED
// ============================================================
void micro_ros_task(void *parameter)
{
  (void)parameter;

  SensorData_t local_data;
  uint32_t last_ping_check_ms = 0;
  uint32_t last_env_publish_ms  = 0;
  uint32_t last_inwater_publish_ms = 0;

  const uint32_t publish_env_period_ms = 1000;
  const uint32_t alive_check_period_ms = 2000;
  const uint32_t publish_inwater_period_ms = 500;

  for (;;)
  {
    switch (g_agent_state)
    {
      case AGENT_DISCONNECTED:
      {
        Serial.println("[state] AGENT_DISCONNECTED -> ping agent");

        if (ping_agent())
        {
          Serial.println("[state] agent reachable");
          g_agent_state = AGENT_CONNECTING;
        }
        else
        {
          Serial.println("[state] agent not reachable");
          vTaskDelay(pdMS_TO_TICKS(2000));
        }
        break;
      }

      case AGENT_CONNECTING:
      {
        Serial.println("[state] AGENT_CONNECTING");

        if (create_microros_entities())
        {
          g_agent_state = AGENT_CONNECTED;
          last_ping_check_ms = millis();
          last_env_publish_ms  = millis();
          last_inwater_publish_ms = millis();
          Serial.println("[state] AGENT_CONNECTED");
        }
        else
        {
          Serial.println("[state] create entities failed");
          destroy_microros_entities();
          g_agent_state = AGENT_DISCONNECTED;
          vTaskDelay(pdMS_TO_TICKS(2000));
        }
        break;
      }

      case AGENT_CONNECTED:
      {
        uint32_t now = millis();

        // 1) 周期检查 agent 是否在线
        if (now - last_ping_check_ms >= alive_check_period_ms)
        {
          last_ping_check_ms = now;

          if (!check_agent_alive())
          {
            Serial.println("[state] agent lost, destroy entities");
            destroy_microros_entities();
            g_agent_state = AGENT_DISCONNECTED;
            break;
          }
        }

        // 2) 周期发布环境数据
        if (now - last_env_publish_ms  >= publish_env_period_ms)
        {
          last_env_publish_ms  = now;

          bool has_data = false;

          if (xSemaphoreTake(sensor_data_mutex, pdMS_TO_TICKS(50)) == pdTRUE)
          {
            local_data = g_sensor_data;
            xSemaphoreGive(sensor_data_mutex);
            has_data = local_data.valid;
          }

          if (has_data)
          {
            temp_msg.data = local_data.temperature;
            hum_msg.data  = local_data.humidity;

            rcl_ret_t ret1 = rcl_publish(&temp_pub, &temp_msg, NULL);
            rcl_ret_t ret2 = rcl_publish(&hum_pub, &hum_msg, NULL);

            if (ret1 == RCL_RET_OK && ret2 == RCL_RET_OK)
            {
              Serial.print("[publish env] T=");
              Serial.print(local_data.temperature);
              Serial.print(" H=");
              Serial.println(local_data.humidity);
            }
            else
            {
              Serial.print("[publish env] failed ret1=");
              Serial.print((int)ret1);
              Serial.print(" ret2=");
              Serial.println((int)ret2);

              // 发布失败不一定说明掉线，但大概率链路有问题
              // 这里进一步检查一次 agent
              if (!check_agent_alive())
              {
                Serial.println("[publish env] agent confirmed lost");
                destroy_microros_entities();
                g_agent_state = AGENT_DISCONNECTED;
                break;
              }
            }
          }
          else
          {
            Serial.println("[publish env] no valid sensor data yet");
          }
        }


        // 3) 每 500ms 发布水传感器状态
        if (now - last_inwater_publish_ms >= publish_inwater_period_ms)
        {
          last_inwater_publish_ms = now;

          inwater_msg.data = isInWater;

          rcl_ret_t ret = rcl_publish(&inwater_pub, &inwater_msg, NULL);

          if (ret == RCL_RET_OK)
          {
            Serial.print("[publish water] isInWater=");
            Serial.println(isInWater ? "true" : "false");
          }
          else
          {
            Serial.print("[publish water] failed ret=");
            Serial.println((int)ret);

            if (!check_agent_alive())
            {
              Serial.println("[publish water] agent confirmed lost");
              destroy_microros_entities();
              g_agent_state = AGENT_DISCONNECTED;
              break;
            }
          }
        }

        vTaskDelay(pdMS_TO_TICKS(50));
        break;
      }

      default:
      {
        g_agent_state = AGENT_DISCONNECTED;
        break;
      }
    }
  }
}

/**
 * Water sensor and sonar task
 */
void water_sensor_sonar_task(void *parameter)
{
  (void)parameter;
  for (;;)
  {
    isInWater = !digitalRead(WATERSENSOR_PIN);
    if (isInWater)
    {
      digitalWrite(SONAR_ENABLE, HIGH);
      Serial.println("[WATER] in water");
    }
    else
    {
      digitalWrite(SONAR_ENABLE, LOW);
      Serial.println("[WATER] out of");
    }
    vTaskDelay(pdMS_TO_TICKS(500));
  }
}


void setup()
{

  Serial.begin(115200);
  delay(1000);
  Serial.println();
  Serial.println("========== System Boot ==========");

  // 1. 创建互斥锁
  sensor_data_mutex = xSemaphoreCreateMutex();
  if (sensor_data_mutex == NULL)
  {
    Serial.println("[system] create mutex failed");
    while (1)
    {
      delay(1000);
    }
  }

  // 2. 初始化 BME280
  if (!init_env_sensor())
  {
    Serial.println("[system] BME280 init failed, stop");
    while (1)
    {
      delay(1000);
    }
  }

  // 3. 注册 custom transport
  rmw_uros_set_custom_transport(
      false,
      NULL,
      custom_udp_transport_open,
      custom_udp_transport_close,
      custom_udp_transport_write,
      custom_udp_transport_read);

  // 4. 初始化以太网
  if (!init_ethernet())
  {
    Serial.println("[system] Ethernet init failed, stop");
    while (1)
    {
      delay(1000);
    }
  }

  /**
   * Init water sensor and sonar
   */
  init_water_sensor();

  // 5. 创建任务
  xTaskCreate(env_sensor_task, "env_sensor_task", 4096, NULL, 1, NULL);

  xTaskCreate(micro_ros_task, "micro_ros_task", 12288, NULL, 5, NULL);

  xTaskCreate(water_sensor_sonar_task, "water_sensor_sonar_task", 2048, NULL, 3, NULL);

  Serial.println("[system] tasks created");
}

void loop()
{


}
