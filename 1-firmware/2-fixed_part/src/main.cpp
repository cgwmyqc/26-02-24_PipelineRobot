#include <Arduino.h>
#include <micro_ros_platformio.h>
#include <rmw_microros/rmw_microros.h>
#include <SPI.h>
#include <Ethernet.h>
#include <EthernetUdp.h>
#include <rclc/rclc.h>
#include <rclc/executor.h>
#include <std_msgs/msg/bool.h>
#include <std_msgs/msg/int8.h>

// ============================================================
// 宏定义
// ============================================================
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

// ============================================================
// W5500 引脚定义
// ============================================================
#define W5500_CS   14
#define W5500_RST  9
#define W5500_INT  10
#define W5500_MISO 12
#define W5500_MOSI 11
#define W5500_SCK  13

// ============================================================
// 网络配置
// ============================================================
byte mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xEE};
IPAddress client_ip(192, 168, 1, 178);
IPAddress dns(192, 168, 1, 1);
IPAddress gateway(192, 168, 1, 1);
IPAddress netmask(255, 255, 255, 0);
IPAddress agent_ip(192, 168, 1, 199);

const uint16_t agent_port = 8888;
const uint16_t client_port = 8890;

// ============================================================
// IO 定义
// ============================================================
// 模式切换输入：高电平触发一次模式翻转
#define MODE_TOGGLE_IN_PIN   1

// 模式输出：LOW=自动，HIGH=手动
#define MODE_OUT_PIN         0

// 电机使能继电器输出：HIGH=吸合
#define MOTOR_ENABLE_PIN     37

// 电机方向输出
// 同为 HIGH -> 正转
// 同为 LOW  -> 反转
// 不同      -> 停止
#define MOTOR_DIR_PIN_A      35
#define MOTOR_DIR_PIN_B      36

// 手动按钮输入，高有效
#define MANUAL_FORWARD_BTN_PIN  2
#define MANUAL_REVERSE_BTN_PIN  3

// ============================================================
// 状态定义
// ============================================================
typedef enum
{
  AGENT_DISCONNECTED = 0,
  AGENT_CONNECTING,
  AGENT_CONNECTED
} AgentState_t;

typedef enum
{
  MOTOR_STOP = 0,
  MOTOR_FORWARD = 1,
  MOTOR_REVERSE = -1
} MotorRunState_t;

typedef struct
{
  bool manual_mode;          // true=手动, false=自动
  bool motor_enable;         // 当前电机使能状态
  MotorRunState_t motor_run; // 当前运行状态：1正转，0停止，-1反转
  bool btn_forward;          // 手动正转按钮状态
  bool btn_reverse;          // 手动反转按钮状态
  uint32_t update_ms;
} ControllerState_t;

// ============================================================
// 全局变量
// ============================================================
AgentState_t g_agent_state = AGENT_DISCONNECTED;

ControllerState_t g_ctrl_state = {
    false,        // manual_mode: 默认自动
    false,        // motor_enable
    MOTOR_STOP,   // motor_run
    false,        // btn_forward
    false,        // btn_reverse
    0             // update_ms
};

SemaphoreHandle_t ctrl_state_mutex = NULL;

// 自动模式下的逻辑变量（当前先用本地变量占位，后续你可改成订阅ROS2命令）
volatile bool g_auto_enable_cmd = false;
volatile bool g_auto_forward_cmd = false;
volatile bool g_auto_reverse_cmd = false;

// GPIO1 上升沿检测用
bool g_last_mode_toggle_signal = false;

// ============================================================
// micro-ROS 对象
// ============================================================
rcl_allocator_t allocator;
rclc_support_t support;
rcl_node_t node;

// publishers
rcl_publisher_t motor_enable_pub;
rcl_publisher_t control_mode_pub;
rcl_publisher_t motor_state_pub;
rcl_publisher_t btn_forward_pub;
rcl_publisher_t btn_reverse_pub;

// msgs
std_msgs__msg__Bool motor_enable_msg;
std_msgs__msg__Bool control_mode_msg;
std_msgs__msg__Int8 motor_state_msg;
std_msgs__msg__Bool btn_forward_msg;
std_msgs__msg__Bool btn_reverse_msg;

// ============================================================
// UDP 对象
// ============================================================
EthernetUDP udp;
bool udp_opened = false;

// ============================================================
// 函数声明
// ============================================================
bool init_ethernet();
bool init_io();
bool create_microros_entities();
void destroy_microros_entities();
bool ping_agent();
bool check_agent_alive();
void io_control_task(void *parameter);
void micro_ros_task(void *parameter);

// ============================================================
// custom transport
// ============================================================
bool custom_udp_transport_open(uxrCustomTransport *transport)
{
  (void)transport;

  if (udp_opened)
  {
    Serial.println("[UDP] already open");
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

size_t custom_udp_transport_write(struct uxrCustomTransport *transport,
                                  const uint8_t *buf,
                                  size_t len,
                                  uint8_t *errcode)
{
  (void)transport;
  (void)errcode;
  udp.beginPacket(agent_ip, agent_port);
  size_t written = udp.write(buf, len);
  udp.endPacket();
  return written;
}

size_t custom_udp_transport_read(struct uxrCustomTransport *transport,
                                 uint8_t *buf,
                                 size_t len,
                                 int timeout,
                                 uint8_t *errcode)
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

// ============================================================
// 初始化 IO
// ============================================================
bool init_io()
{
  pinMode(MODE_TOGGLE_IN_PIN, INPUT);
  pinMode(MODE_OUT_PIN, OUTPUT);

  pinMode(MOTOR_ENABLE_PIN, OUTPUT);
  pinMode(MOTOR_DIR_PIN_A, OUTPUT);
  pinMode(MOTOR_DIR_PIN_B, OUTPUT);

  pinMode(MANUAL_FORWARD_BTN_PIN, INPUT);
  pinMode(MANUAL_REVERSE_BTN_PIN, INPUT);

  // 上电默认：自动模式、继电器断开、电机停止
  digitalWrite(MODE_OUT_PIN, LOW);        // 自动模式
  digitalWrite(MOTOR_ENABLE_PIN, LOW);    // 电机断电
  digitalWrite(MOTOR_DIR_PIN_A, HIGH);    // 这里设为不一致，表示停机
  digitalWrite(MOTOR_DIR_PIN_B, LOW);

  Serial.println("[IO] init done");
  return true;
}

// ============================================================
// 初始化以太网
// ============================================================
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

// ============================================================
// ping agent
// ============================================================
bool ping_agent()
{
  rmw_ret_t ret = rmw_uros_ping_agent(100, 1);
  return (ret == RMW_RET_OK);
}

bool check_agent_alive()
{
  rmw_ret_t ret = rmw_uros_ping_agent(100, 1);
  return (ret == RMW_RET_OK);
}

// ============================================================
// 创建 micro-ROS 实体
// ============================================================
bool create_microros_entities()
{
  Serial.println("[micro-ROS] create node and publishers begin");

  allocator = rcl_get_default_allocator();

  support = (rclc_support_t){0};
  node = rcl_get_zero_initialized_node();

  motor_enable_pub = rcl_get_zero_initialized_publisher();
  control_mode_pub = rcl_get_zero_initialized_publisher();
  motor_state_pub  = rcl_get_zero_initialized_publisher();
  btn_forward_pub  = rcl_get_zero_initialized_publisher();
  btn_reverse_pub  = rcl_get_zero_initialized_publisher();

  RCCHECK(rclc_support_init(&support, 0, NULL, &allocator));
  RCCHECK(rclc_node_init_default(&node, "esp32_fixed_controller_node", "", &support));

  RCCHECK(rclc_publisher_init_default(
      &motor_enable_pub,
      &node,
      ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, Bool),
      "/fixed_controller/motor_enable"));

  RCCHECK(rclc_publisher_init_default(
      &control_mode_pub,
      &node,
      ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, Bool),
      "/fixed_controller/manual_mode"));

  RCCHECK(rclc_publisher_init_default(
      &motor_state_pub,
      &node,
      ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, Int8),
      "/fixed_controller/motor_run_state"));

  RCCHECK(rclc_publisher_init_default(
      &btn_forward_pub,
      &node,
      ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, Bool),
      "/fixed_controller/manual_forward_button"));

  RCCHECK(rclc_publisher_init_default(
      &btn_reverse_pub,
      &node,
      ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, Bool),
      "/fixed_controller/manual_reverse_button"));

  Serial.println("[micro-ROS] create node and publishers ok");
  return true;
}

// ============================================================
// 销毁 micro-ROS 实体
// ============================================================
void destroy_microros_entities()
{
  Serial.println("[micro-ROS] destroy node&pub begin");

  if (motor_enable_pub.impl != NULL)
  {
    RCSOFTCHECK(rcl_publisher_fini(&motor_enable_pub, &node));
    motor_enable_pub = rcl_get_zero_initialized_publisher();
  }

  if (control_mode_pub.impl != NULL)
  {
    RCSOFTCHECK(rcl_publisher_fini(&control_mode_pub, &node));
    control_mode_pub = rcl_get_zero_initialized_publisher();
  }

  if (motor_state_pub.impl != NULL)
  {
    RCSOFTCHECK(rcl_publisher_fini(&motor_state_pub, &node));
    motor_state_pub = rcl_get_zero_initialized_publisher();
  }

  if (btn_forward_pub.impl != NULL)
  {
    RCSOFTCHECK(rcl_publisher_fini(&btn_forward_pub, &node));
    btn_forward_pub = rcl_get_zero_initialized_publisher();
  }

  if (btn_reverse_pub.impl != NULL)
  {
    RCSOFTCHECK(rcl_publisher_fini(&btn_reverse_pub, &node));
    btn_reverse_pub = rcl_get_zero_initialized_publisher();
  }

  if (node.impl != NULL)
  {
    RCSOFTCHECK(rcl_node_fini(&node));
    node = rcl_get_zero_initialized_node();
  }

  RCSOFTCHECK(rclc_support_fini(&support));
  support = (rclc_support_t){0};

  Serial.println("[micro-ROS] destroy node&pub done");
}

// ============================================================
// IO控制任务
// 负责：
// 1) 采集模式切换输入
// 2) 采集按钮输入
// 3) 根据手动/自动模式计算控制输出
// 4) 更新共享状态
// ============================================================
void io_control_task(void *parameter)
{
  (void)parameter;

  const TickType_t period = pdMS_TO_TICKS(50);
  TickType_t last_wake_time = xTaskGetTickCount();

  for (;;)
  {
    bool toggle_signal = (digitalRead(MODE_TOGGLE_IN_PIN) == HIGH);
    bool btn_fwd = (digitalRead(MANUAL_FORWARD_BTN_PIN) == HIGH);
    bool btn_rev = (digitalRead(MANUAL_REVERSE_BTN_PIN) == HIGH);

    ControllerState_t local_state;

    // 先取出当前状态
    if (xSemaphoreTake(ctrl_state_mutex, pdMS_TO_TICKS(10)) == pdTRUE)
    {
      local_state = g_ctrl_state;
      xSemaphoreGive(ctrl_state_mutex);
    }
    else
    {
      vTaskDelayUntil(&last_wake_time, period);
      continue;
    }

    // --------------------------------------------------------
    // 1) GPIO1 上升沿切换模式
    // --------------------------------------------------------
    if (toggle_signal && !g_last_mode_toggle_signal)
    {
      local_state.manual_mode = !local_state.manual_mode;
      Serial.print("[MODE] toggled -> ");
      Serial.println(local_state.manual_mode ? "MANUAL" : "AUTO");
    }
    g_last_mode_toggle_signal = toggle_signal;

    // --------------------------------------------------------
    // 2) 模式输出 GPIO0
    // LOW=自动，HIGH=手动
    // --------------------------------------------------------
    digitalWrite(MODE_OUT_PIN, local_state.manual_mode ? HIGH : LOW);

    // --------------------------------------------------------
    // 3) 更新按钮状态
    // --------------------------------------------------------
    local_state.btn_forward = btn_fwd;
    local_state.btn_reverse = btn_rev;

    // --------------------------------------------------------
    // 4) 根据模式决定电机命令
    // --------------------------------------------------------
    bool motor_enable = false;
    MotorRunState_t motor_run = MOTOR_STOP;

    if (local_state.manual_mode)
    {
      // 手动模式：按钮决定方向
      if (btn_fwd && !btn_rev)
      {
        motor_enable = true;
        motor_run = MOTOR_FORWARD;
      }
      else if (!btn_fwd && btn_rev)
      {
        motor_enable = true;
        motor_run = MOTOR_REVERSE;
      }
      else
      {
        motor_enable = false;
        motor_run = MOTOR_STOP;
      }
    }
    else
    {
      // 自动模式：由逻辑变量决定
      motor_enable = g_auto_enable_cmd;

      if (g_auto_forward_cmd && !g_auto_reverse_cmd)
      {
        motor_run = MOTOR_FORWARD;
      }
      else if (!g_auto_forward_cmd && g_auto_reverse_cmd)
      {
        motor_run = MOTOR_REVERSE;
      }
      else
      {
        motor_run = MOTOR_STOP;
      }

      // 如果未使能，直接强制停机
      if (!motor_enable)
      {
        motor_run = MOTOR_STOP;
      }
    }

    // --------------------------------------------------------
    // 5) 输出电机使能 GPIO37
    // --------------------------------------------------------
    digitalWrite(MOTOR_ENABLE_PIN, motor_enable ? HIGH : LOW);

    // --------------------------------------------------------
    // 6) 输出电机方向 GPIO35 / GPIO36
    // 同高=正转，同低=反转，不同=停止
    // --------------------------------------------------------
    if (motor_run == MOTOR_FORWARD)
    {
      digitalWrite(MOTOR_DIR_PIN_A, HIGH);
      digitalWrite(MOTOR_DIR_PIN_B, HIGH);
    }
    else if (motor_run == MOTOR_REVERSE)
    {
      digitalWrite(MOTOR_DIR_PIN_A, LOW);
      digitalWrite(MOTOR_DIR_PIN_B, LOW);
    }
    else
    {
      // 停止：输出不同电平
      digitalWrite(MOTOR_DIR_PIN_A, HIGH);
      digitalWrite(MOTOR_DIR_PIN_B, LOW);
    }

    // --------------------------------------------------------
    // 7) 回写共享状态
    // --------------------------------------------------------
    local_state.motor_enable = motor_enable;
    local_state.motor_run = motor_run;
    local_state.update_ms = millis();

    if (xSemaphoreTake(ctrl_state_mutex, pdMS_TO_TICKS(10)) == pdTRUE)
    {
      g_ctrl_state = local_state;
      xSemaphoreGive(ctrl_state_mutex);
    }

    vTaskDelayUntil(&last_wake_time, period);
  }
}

// ============================================================
// micro-ROS 任务
// 周期发布：
// 1) 电机使能
// 2) 手动/自动模式
// 3) 电机运行状态
// 4) 正转按钮状态
// 5) 反转按钮状态
// ============================================================
void micro_ros_task(void *parameter)
{
  (void)parameter;

  ControllerState_t local_state;
  uint32_t last_ping_check_ms = 0;
  uint32_t last_publish_ms = 0;

  const uint32_t alive_check_period_ms = 2000;
  const uint32_t publish_period_ms = 500;

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
          last_publish_ms = millis();
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

        // 1) 检查 agent 在线状态
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

        // 2) 周期发布
        if (now - last_publish_ms >= publish_period_ms)
        {
          last_publish_ms = now;

          bool has_data = false;
          if (xSemaphoreTake(ctrl_state_mutex, pdMS_TO_TICKS(20)) == pdTRUE)
          {
            local_state = g_ctrl_state;
            xSemaphoreGive(ctrl_state_mutex);
            has_data = true;
          }

          if (has_data)
          {
            motor_enable_msg.data = local_state.motor_enable;
            control_mode_msg.data = local_state.manual_mode;
            motor_state_msg.data  = (int8_t)local_state.motor_run;
            btn_forward_msg.data  = local_state.btn_forward;
            btn_reverse_msg.data  = local_state.btn_reverse;

            rcl_ret_t ret1 = rcl_publish(&motor_enable_pub, &motor_enable_msg, NULL);
            rcl_ret_t ret2 = rcl_publish(&control_mode_pub, &control_mode_msg, NULL);
            rcl_ret_t ret3 = rcl_publish(&motor_state_pub, &motor_state_msg, NULL);
            rcl_ret_t ret4 = rcl_publish(&btn_forward_pub, &btn_forward_msg, NULL);
            rcl_ret_t ret5 = rcl_publish(&btn_reverse_pub, &btn_reverse_msg, NULL);

            if (ret1 == RCL_RET_OK &&
                ret2 == RCL_RET_OK &&
                ret3 == RCL_RET_OK &&
                ret4 == RCL_RET_OK &&
                ret5 == RCL_RET_OK)
            {
              Serial.print("[publish] mode=");
              Serial.print(local_state.manual_mode ? "MANUAL" : "AUTO");
              Serial.print(" enable=");
              Serial.print(local_state.motor_enable ? "1" : "0");
              Serial.print(" motor_run=");
              Serial.print((int)local_state.motor_run);
              Serial.print(" btn_fwd=");
              Serial.print(local_state.btn_forward ? "1" : "0");
              Serial.print(" btn_rev=");
              Serial.println(local_state.btn_reverse ? "1" : "0");
            }
            else
            {
              Serial.println("[publish] failed");

              if (!check_agent_alive())
              {
                Serial.println("[publish] agent confirmed lost");
                destroy_microros_entities();
                g_agent_state = AGENT_DISCONNECTED;
                break;
              }
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

// ============================================================
// setup
// ============================================================
void setup()
{
  Serial.begin(115200);
  delay(1000);
  Serial.println();
  Serial.println("========== Fixed Controller Boot ==========");

  ctrl_state_mutex = xSemaphoreCreateMutex();
  if (ctrl_state_mutex == NULL)
  {
    Serial.println("[system] create mutex failed");
    while (1)
    {
      delay(1000);
    }
  }

  if (!init_io())
  {
    Serial.println("[system] IO init failed");
    while (1)
    {
      delay(1000);
    }
  }

  rmw_uros_set_custom_transport(
      false,
      NULL,
      custom_udp_transport_open,
      custom_udp_transport_close,
      custom_udp_transport_write,
      custom_udp_transport_read);

  if (!init_ethernet())
  {
    Serial.println("[system] Ethernet init failed");
    while (1)
    {
      delay(1000);
    }
  }

  xTaskCreate(io_control_task, "io_control_task", 4096, NULL, 3, NULL);
  xTaskCreate(micro_ros_task, "micro_ros_task", 12288, NULL, 5, NULL);

  Serial.println("[system] tasks created");
}

// ============================================================
// loop
// ============================================================
void loop()
{
}