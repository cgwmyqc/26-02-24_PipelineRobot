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
#include <std_msgs/msg/int32.h>
#include <std_msgs/msg/float32.h>

// ============================================================
// 错误检查宏
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
// 网络参数
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
// 模式切换输入，按键上升沿触发一次模式翻转
#define MODE_TOGGLE_IN_PIN      1

// 模式输出到继电器，LOW=自动模式，HIGH=人工模式
#define MODE_OUT_PIN            15

// 电机使能输出，HIGH=使能
#define MOTOR_ENABLE_PIN        37

// 电机方向继电器输出
// 两路同时 HIGH 表示后退
// 两路同时 LOW 表示前进
// 一高一低表示停止
#define MOTOR_DIR_PIN_A         35
#define MOTOR_DIR_PIN_B         36

// 人工模式下的本地前进/后退按钮输入
#define MANUAL_FORWARD_BTN_PIN  2
#define MANUAL_REVERSE_BTN_PIN  3

// ============================================================
// 编码器引脚定义
// ============================================================
#define ENCODER_A_PIN  45
#define ENCODER_B_PIN  46
#define ENCODER_Z_PIN  47

// ============================================================
// 编码器与行程参数
// ============================================================
// 360 线增量编码器，AB 相四倍频后为 1440 counts/rev
constexpr int   ENCODER_LINES = 360;
constexpr int   ENCODER_X4_COUNTS_PER_REV = ENCODER_LINES * 4;

// 轮径 26 mm
constexpr float WHEEL_DIAMETER_M = 0.026f;
constexpr float WHEEL_CIRCUMFERENCE_M = 3.1415926f * WHEEL_DIAMETER_M;

// 每米对应的编码器计数
constexpr float COUNTS_PER_METER = ENCODER_X4_COUNTS_PER_REV / WHEEL_CIRCUMFERENCE_M;

// 自动巡检单步前进 0.3 m
constexpr float AUTO_STEP_LENGTH_M = 0.3f;
constexpr int32_t AUTO_STEP_COUNTS = (int32_t)(COUNTS_PER_METER * AUTO_STEP_LENGTH_M + 0.5f);
constexpr float AUTO_FIRST_DETECT_OFFSET_M = 0.5f;
constexpr int32_t AUTO_FIRST_DETECT_COUNTS =
    (int32_t)(COUNTS_PER_METER * AUTO_FIRST_DETECT_OFFSET_M + 0.5f);

// 自动巡检总行程 1.2 m，从首次检测点开始累计
constexpr float AUTO_TOTAL_LENGTH_M = 1.2f;
constexpr int32_t AUTO_TOTAL_COUNTS = (int32_t)(COUNTS_PER_METER * AUTO_TOTAL_LENGTH_M + 0.5f);

// 回零时允许的编码器误差
constexpr int32_t HOME_TOLERANCE_COUNTS = 5;

// ============================================================
// 消抖参数
// ============================================================
constexpr uint32_t DEBOUNCE_MS = 30;

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

typedef enum
{
  AUTO_IDLE = 0,          // 空闲
  AUTO_MOVING_FORWARD,    // 自动前进到目标点
  AUTO_WAIT_DETECT_DONE,  // 到达目标点后等待检测完成
  AUTO_RETURNING_HOME,    // 自动回零
  AUTO_FINISHED           // 自动流程结束
} AutoState_t;

// ============================================================
// 消抖输入状态
// ============================================================
typedef struct
{
  bool raw_state;              // 当前原始输入
  bool stable_state;           // 当前稳定状态
  bool last_stable_state;      // 上一次稳定状态
  uint32_t last_change_ms;     // 原始输入最近一次变化时间
} DebounceInput_t;

// ============================================================
// 控制器状态快照
// ============================================================
typedef struct
{
  bool manual_mode;          // true=人工模式, false=自动模式
  bool motor_enable;         // 电机使能状态
  MotorRunState_t motor_run; // 电机方向状态

  bool btn_forward;          // 合并后的前进命令
  bool btn_reverse;          // 合并后的后退命令

  int32_t encoder_count;     // 编码器计数
  float travel_m;            // 自动巡检相对首次检测点的累计位移
  AutoState_t auto_state;    // 自动巡检状态

  uint32_t update_ms;
} ControllerState_t;

// ============================================================
// 全局状态
// ============================================================
AgentState_t g_agent_state = AGENT_DISCONNECTED;

ControllerState_t g_ctrl_state = {
    false,        // manual_mode: 默认自动模式
    false,        // motor_enable
    MOTOR_STOP,   // motor_run
    false,        // btn_forward
    false,        // btn_reverse
    0,            // encoder_count
    0.0f,         // travel_m
    AUTO_IDLE,    // auto_state
    0
};

SemaphoreHandle_t ctrl_state_mutex = NULL;

// ------------------------------------------------------------
// 远程控制输入
// ------------------------------------------------------------
// 手动模式下，远程前进/后退命令会与本地按钮做或运算。
// 自动模式下，仅保留 start_auto / detect_done 两类流程控制命令。
volatile bool g_auto_forward_cmd = false;
volatile bool g_auto_reverse_cmd = false;
volatile bool g_remote_manual_mode_pending = false;
volatile bool g_remote_manual_mode_value = false;

// ------------------------------------------------------------
// 编码器中断共享状态
// ------------------------------------------------------------
volatile int32_t g_encoder_count = 0;
volatile uint8_t g_encoder_prev_ab = 0;
portMUX_TYPE g_encoder_mux = portMUX_INITIALIZER_UNLOCKED;

// ------------------------------------------------------------
// 自动巡检流程状态
// ------------------------------------------------------------
volatile bool g_start_auto_cmd = false;        // 收到开始自动巡检命令
volatile bool g_detect_done_cmd = false;       // 收到当前检测点完成命令
volatile bool g_motion_reached_event = false;  // 当前目标点到达事件，发布一次后清零

AutoState_t g_auto_state = AUTO_IDLE;
bool g_auto_task_active = false;
int32_t g_detect_origin_count = 0;
bool g_auto_waiting_first_detect = false;

int32_t g_auto_home_count = 0;          // 自动流程起点
int32_t g_auto_target_count = 0;        // 当前目标点
int32_t g_auto_total_target_count = 0;  // 自动巡检终点

// ------------------------------------------------------------
// 消抖器实例
// ------------------------------------------------------------
DebounceInput_t g_mode_toggle_db = {false, false, false, 0};
DebounceInput_t g_btn_forward_db = {false, false, false, 0};
DebounceInput_t g_btn_reverse_db = {false, false, false, 0};

// ============================================================
// micro-ROS 实体
// ============================================================
rcl_allocator_t allocator;
rclc_support_t support;
rcl_node_t node;
rclc_executor_t executor;

// ------------------------------------------------------------
// 发布器
// ------------------------------------------------------------
rcl_publisher_t motor_enable_pub;
rcl_publisher_t control_mode_pub;
rcl_publisher_t motor_state_pub;
rcl_publisher_t btn_forward_pub;
rcl_publisher_t btn_reverse_pub;
rcl_publisher_t motion_reached_pub;
rcl_publisher_t encoder_count_pub;
rcl_publisher_t travel_m_pub;

// ------------------------------------------------------------
// 订阅器
// ------------------------------------------------------------
rcl_subscription_t start_auto_sub;
rcl_subscription_t detect_done_sub;
rcl_subscription_t set_manual_mode_sub;
rcl_subscription_t manual_forward_cmd_sub;
rcl_subscription_t manual_reverse_cmd_sub;

// ------------------------------------------------------------
// 消息对象
// ------------------------------------------------------------
std_msgs__msg__Bool motor_enable_msg;
std_msgs__msg__Bool control_mode_msg;
std_msgs__msg__Int8 motor_state_msg;
std_msgs__msg__Bool btn_forward_msg;
std_msgs__msg__Bool btn_reverse_msg;
std_msgs__msg__Bool motion_reached_msg;
std_msgs__msg__Int32 encoder_count_msg;
std_msgs__msg__Float32 travel_m_msg;

// 订阅消息缓冲
std_msgs__msg__Bool start_auto_msg;
std_msgs__msg__Bool detect_done_msg;
std_msgs__msg__Bool set_manual_mode_msg;
std_msgs__msg__Bool manual_forward_cmd_msg;
std_msgs__msg__Bool manual_reverse_cmd_msg;

// ============================================================
// UDP 传输
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

void IRAM_ATTR encoder_isr();
int32_t get_encoder_count();
void reset_encoder_count(int32_t val);

void start_auto_sequence();
void process_auto_sequence(bool *motor_enable, MotorRunState_t *motor_run, int32_t encoder_now);

void start_auto_callback(const void *msgin);
void detect_done_callback(const void *msgin);
void set_manual_mode_callback(const void *msgin);
void manual_forward_cmd_callback(const void *msgin);
void manual_reverse_cmd_callback(const void *msgin);

bool debounce_update(DebounceInput_t *db, bool raw, uint32_t now_ms);
bool debounce_rising_edge(DebounceInput_t *db);

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
// 编码器中断处理
// ============================================================
// A/B 相任一变化都进入中断，使用四倍频查表更新计数。
// 为了让前进方向在遥测中保持为正值，这里的增减方向已按当前接线校正。
void IRAM_ATTR encoder_isr()
{
  uint8_t a = digitalRead(ENCODER_A_PIN);
  uint8_t b = digitalRead(ENCODER_B_PIN);
  uint8_t ab = (a << 1) | b;

  static const int8_t quad_table[16] = {
      0, -1,  1,  0,
      1,  0,  0, -1,
     -1,  0,  0,  1,
      0,  1, -1,  0
  };

  portENTER_CRITICAL_ISR(&g_encoder_mux);
  uint8_t idx = (g_encoder_prev_ab << 2) | ab;
  // Motor forward/reverse wiring has been swapped, so flip the encoder sign
  // here to keep "forward" positive in encoder_count/travel_m telemetry.
  g_encoder_count += quad_table[idx];
  g_encoder_prev_ab = ab;
  portEXIT_CRITICAL_ISR(&g_encoder_mux);
}

// 读取编码器计数
int32_t get_encoder_count()
{
  int32_t cnt;
  portENTER_CRITICAL(&g_encoder_mux);
  cnt = g_encoder_count;
  portEXIT_CRITICAL(&g_encoder_mux);
  return cnt;
}

// 重置编码器计数，并同步当前 A/B 相位
void reset_encoder_count(int32_t val)
{
  portENTER_CRITICAL(&g_encoder_mux);
  g_encoder_count = val;

  uint8_t a = digitalRead(ENCODER_A_PIN);
  uint8_t b = digitalRead(ENCODER_B_PIN);
  g_encoder_prev_ab = (a << 1) | b;

  portEXIT_CRITICAL(&g_encoder_mux);
}

// ============================================================
// 按键消抖
// ============================================================
// raw 为当前原始输入，now_ms 为当前时间戳。
// 返回值为稳定后的输入状态。
// ============================================================
// ============================================================
bool debounce_update(DebounceInput_t *db, bool raw, uint32_t now_ms)
{
  // 原始输入变化时刷新计时起点
  if (raw != db->raw_state)
  {
    db->raw_state = raw;
    db->last_change_ms = now_ms;
  }

  // 持续稳定超过消抖时间后，更新稳定状态
  if ((now_ms - db->last_change_ms) >= DEBOUNCE_MS)
  {
    db->last_stable_state = db->stable_state;
    db->stable_state = db->raw_state;
  }

  return db->stable_state;
}

// 判断稳定状态是否产生上升沿
bool debounce_rising_edge(DebounceInput_t *db)
{
  return (db->stable_state == true && db->last_stable_state == false);
}

// ============================================================
// ROS2 订阅回调
// ============================================================
void start_auto_callback(const void *msgin)
{
  const std_msgs__msg__Bool *msg = (const std_msgs__msg__Bool *)msgin;
  if (msg->data)
  {
    g_start_auto_cmd = true;
    Serial.println("[ROS] recv start_auto = true");
  }
}

void detect_done_callback(const void *msgin)
{
  const std_msgs__msg__Bool *msg = (const std_msgs__msg__Bool *)msgin;
  if (msg->data)
  {
    g_detect_done_cmd = true;
    Serial.println("[ROS] recv detect_done = true");
  }
}

void set_manual_mode_callback(const void *msgin)
{
  const std_msgs__msg__Bool *msg = (const std_msgs__msg__Bool *)msgin;
  g_remote_manual_mode_value = msg->data;
  g_remote_manual_mode_pending = true;
  Serial.print("[ROS] recv set_manual_mode = ");
  Serial.println(msg->data ? "MANUAL" : "AUTO");
}

void manual_forward_cmd_callback(const void *msgin)
{
  const std_msgs__msg__Bool *msg = (const std_msgs__msg__Bool *)msgin;
  g_auto_forward_cmd = msg->data;
}

void manual_reverse_cmd_callback(const void *msgin)
{
  const std_msgs__msg__Bool *msg = (const std_msgs__msg__Bool *)msgin;
  g_auto_reverse_cmd = msg->data;
}

// ============================================================
// 自动巡检流程
// ============================================================
// 启动后先记录 home 点，再前进到首次检测点。
// 之后每收到一次 detect_done，就前进一个步长。
// 到达最终检测点并收到 detect_done 后，开始回零。
void start_auto_sequence()
{
  int32_t now_cnt = get_encoder_count();

  g_auto_task_active = true;
  g_auto_state = AUTO_MOVING_FORWARD;

  g_auto_home_count = now_cnt;
  g_detect_origin_count = g_auto_home_count + AUTO_FIRST_DETECT_COUNTS;
  g_auto_target_count = g_detect_origin_count;
  g_auto_total_target_count = g_detect_origin_count + AUTO_TOTAL_COUNTS;
  g_auto_waiting_first_detect = true;

  g_detect_done_cmd = false;
  g_motion_reached_event = false;

  Serial.println("[AUTO] start sequence");
  Serial.print("[AUTO] home_count = ");
  Serial.println(g_auto_home_count);
  Serial.print("[AUTO] detect origin = ");
  Serial.println(g_auto_target_count);
  Serial.print("[AUTO] total target = ");
  Serial.println(g_auto_total_target_count);
}

void process_auto_sequence(bool *motor_enable, MotorRunState_t *motor_run, int32_t encoder_now)
{
  *motor_enable = false;
  *motor_run = MOTOR_STOP;

  if (g_start_auto_cmd && !g_auto_task_active)
  {
    g_start_auto_cmd = false;
    start_auto_sequence();
  }

  if (!g_auto_task_active)
  {
    g_auto_state = AUTO_IDLE;
    *motor_enable = false;
    *motor_run = MOTOR_STOP;
    return;
  }

  switch (g_auto_state)
  {
    case AUTO_MOVING_FORWARD:
    {
      *motor_enable = true;
      *motor_run = MOTOR_FORWARD;

      if (encoder_now >= g_auto_target_count)
      {
        *motor_enable = false;
        *motor_run = MOTOR_STOP;
        g_motion_reached_event = true;
        g_auto_state = AUTO_WAIT_DETECT_DONE;

        if (g_auto_waiting_first_detect)
        {
          g_auto_waiting_first_detect = false;
          Serial.println("[AUTO] first detect origin reached, wait detect_done");
        }
        else if (g_auto_target_count >= g_auto_total_target_count)
        {
          Serial.println("[AUTO] final detect point reached, wait detect_done");
        }
        else
        {
          Serial.println("[AUTO] step reached, wait detect_done");
        }
      }
      break;
    }

    case AUTO_WAIT_DETECT_DONE:
    {
      *motor_enable = false;
      *motor_run = MOTOR_STOP;

      if (g_detect_done_cmd)
      {
        g_detect_done_cmd = false;

        if (g_auto_target_count >= g_auto_total_target_count)
        {
          g_auto_state = AUTO_RETURNING_HOME;
          Serial.println("[AUTO] detect done at final point, return home");
        }
        else
        {
          g_auto_target_count += AUTO_STEP_COUNTS;
          if (g_auto_target_count > g_auto_total_target_count)
          {
            g_auto_target_count = g_auto_total_target_count;
          }

          g_auto_state = AUTO_MOVING_FORWARD;
          Serial.print("[AUTO] detect done, next target = ");
          Serial.println(g_auto_target_count);
        }
      }
      break;
    }

    case AUTO_RETURNING_HOME:
    {
      *motor_enable = true;
      *motor_run = MOTOR_REVERSE;

      if (encoder_now <= (g_auto_home_count + HOME_TOLERANCE_COUNTS))
      {
        *motor_enable = false;
        *motor_run = MOTOR_STOP;

        g_auto_task_active = false;
        g_auto_waiting_first_detect = false;
        g_auto_state = AUTO_FINISHED;

        Serial.println("[AUTO] returned home, finished");
      }
      break;
    }

    case AUTO_FINISHED:
    default:
    {
      *motor_enable = false;
      *motor_run = MOTOR_STOP;
      break;
    }
  }
}

// ============================================================
// 初始化 IO
// ============================================================
bool init_io()
{
  // 基础输入输出
  pinMode(MODE_TOGGLE_IN_PIN, INPUT_PULLDOWN);
  pinMode(MODE_OUT_PIN, OUTPUT);

  pinMode(MOTOR_ENABLE_PIN, OUTPUT);
  pinMode(MOTOR_DIR_PIN_A, OUTPUT);
  pinMode(MOTOR_DIR_PIN_B, OUTPUT);

  pinMode(MANUAL_FORWARD_BTN_PIN, INPUT_PULLDOWN);
  pinMode(MANUAL_REVERSE_BTN_PIN, INPUT_PULLDOWN);

  // ----------------------------------------------------------
  // 编码器输入
  // 使用上拉输入，适配当前 NPN 开集电极输出接法。
  // ----------------------------------------------------------
  //
  // ----------------------------------------------------------
  pinMode(ENCODER_A_PIN, INPUT_PULLUP);
  pinMode(ENCODER_B_PIN, INPUT_PULLUP);
  pinMode(ENCODER_Z_PIN, INPUT_PULLUP);

  // 上电时编码器清零
  reset_encoder_count(0);

  // A/B 相均启用中断
  attachInterrupt(digitalPinToInterrupt(ENCODER_A_PIN), encoder_isr, CHANGE);
  attachInterrupt(digitalPinToInterrupt(ENCODER_B_PIN), encoder_isr, CHANGE);

  // 上电默认进入自动模式，电机停止
  digitalWrite(MODE_OUT_PIN, LOW);       // 自动模式
  digitalWrite(MOTOR_ENABLE_PIN, LOW);   // 电机失能
  digitalWrite(MOTOR_DIR_PIN_A, HIGH);   // 停止态
  digitalWrite(MOTOR_DIR_PIN_B, LOW);

  Serial.println("[IO] init done");
  Serial.print("[ENC] COUNTS_PER_METER = ");
  Serial.println(COUNTS_PER_METER);
  Serial.print("[ENC] AUTO_STEP_COUNTS = ");
  Serial.println(AUTO_STEP_COUNTS);
  Serial.print("[ENC] AUTO_TOTAL_COUNTS = ");
  Serial.println(AUTO_TOTAL_COUNTS);

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
// 创建 micro-ROS 节点、发布器和订阅器
// ============================================================
bool create_microros_entities()
{
  Serial.println("[micro-ROS] create node/pubs/subs begin");

  allocator = rcl_get_default_allocator();

  support = (rclc_support_t){0};
  node = rcl_get_zero_initialized_node();
  executor = rclc_executor_get_zero_initialized_executor();

  motor_enable_pub = rcl_get_zero_initialized_publisher();
  control_mode_pub = rcl_get_zero_initialized_publisher();
  motor_state_pub  = rcl_get_zero_initialized_publisher();
  btn_forward_pub  = rcl_get_zero_initialized_publisher();
  btn_reverse_pub  = rcl_get_zero_initialized_publisher();
  motion_reached_pub = rcl_get_zero_initialized_publisher();
  encoder_count_pub = rcl_get_zero_initialized_publisher();
  travel_m_pub = rcl_get_zero_initialized_publisher();

  start_auto_sub = rcl_get_zero_initialized_subscription();
  detect_done_sub = rcl_get_zero_initialized_subscription();
  set_manual_mode_sub = rcl_get_zero_initialized_subscription();
  manual_forward_cmd_sub = rcl_get_zero_initialized_subscription();
  manual_reverse_cmd_sub = rcl_get_zero_initialized_subscription();

  RCCHECK(rclc_support_init(&support, 0, NULL, &allocator));
  RCCHECK(rclc_node_init_default(&node, "esp32_fixed_controller_node", "", &support));

  // ----------------------------------------------------------
  // 发布器
  // ----------------------------------------------------------
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

  RCCHECK(rclc_publisher_init_default(
      &motion_reached_pub,
      &node,
      ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, Bool),
      "/fixed_controller/motion_reached"));

  RCCHECK(rclc_publisher_init_default(
      &encoder_count_pub,
      &node,
      ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, Int32),
      "/fixed_controller/encoder_count"));

  RCCHECK(rclc_publisher_init_default(
      &travel_m_pub,
      &node,
      ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, Float32),
      "/fixed_controller/travel_m"));

  // ----------------------------------------------------------
  // 订阅器
  // ----------------------------------------------------------
  RCCHECK(rclc_subscription_init_default(
      &start_auto_sub,
      &node,
      ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, Bool),
      "/fixed_controller/start_auto"));

  RCCHECK(rclc_subscription_init_default(
      &detect_done_sub,
      &node,
      ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, Bool),
      "/fixed_controller/detect_done"));

  RCCHECK(rclc_subscription_init_default(
      &set_manual_mode_sub,
      &node,
      ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, Bool),
      "/fixed_controller/set_manual_mode"));

  RCCHECK(rclc_subscription_init_default(
      &manual_forward_cmd_sub,
      &node,
      ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, Bool),
      "/fixed_controller/manual_forward_cmd"));

  RCCHECK(rclc_subscription_init_default(
      &manual_reverse_cmd_sub,
      &node,
      ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, Bool),
      "/fixed_controller/manual_reverse_cmd"));

  // ----------------------------------------------------------
  // executor
  // ----------------------------------------------------------
  RCCHECK(rclc_executor_init(&executor, &support.context, 5, &allocator));

  RCCHECK(rclc_executor_add_subscription(
      &executor,
      &start_auto_sub,
      &start_auto_msg,
      &start_auto_callback,
      ON_NEW_DATA));

  RCCHECK(rclc_executor_add_subscription(
      &executor,
      &detect_done_sub,
      &detect_done_msg,
      &detect_done_callback,
      ON_NEW_DATA));

  RCCHECK(rclc_executor_add_subscription(
      &executor,
      &set_manual_mode_sub,
      &set_manual_mode_msg,
      &set_manual_mode_callback,
      ON_NEW_DATA));

  RCCHECK(rclc_executor_add_subscription(
      &executor,
      &manual_forward_cmd_sub,
      &manual_forward_cmd_msg,
      &manual_forward_cmd_callback,
      ON_NEW_DATA));

  RCCHECK(rclc_executor_add_subscription(
      &executor,
      &manual_reverse_cmd_sub,
      &manual_reverse_cmd_msg,
      &manual_reverse_cmd_callback,
      ON_NEW_DATA));

  Serial.println("[micro-ROS] create node/pubs/subs ok");
  return true;
}

// ============================================================
// 销毁 micro-ROS 实体
// ============================================================
void destroy_microros_entities()
{
  Serial.println("[micro-ROS] destroy begin");

  if (motion_reached_pub.impl != NULL)
  {
    RCSOFTCHECK(rcl_publisher_fini(&motion_reached_pub, &node));
    motion_reached_pub = rcl_get_zero_initialized_publisher();
  }

  if (encoder_count_pub.impl != NULL)
  {
    RCSOFTCHECK(rcl_publisher_fini(&encoder_count_pub, &node));
    encoder_count_pub = rcl_get_zero_initialized_publisher();
  }

  if (travel_m_pub.impl != NULL)
  {
    RCSOFTCHECK(rcl_publisher_fini(&travel_m_pub, &node));
    travel_m_pub = rcl_get_zero_initialized_publisher();
  }

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

  if (start_auto_sub.impl != NULL)
  {
    RCSOFTCHECK(rcl_subscription_fini(&start_auto_sub, &node));
    start_auto_sub = rcl_get_zero_initialized_subscription();
  }

  if (detect_done_sub.impl != NULL)
  {
    RCSOFTCHECK(rcl_subscription_fini(&detect_done_sub, &node));
    detect_done_sub = rcl_get_zero_initialized_subscription();
  }

  if (set_manual_mode_sub.impl != NULL)
  {
    RCSOFTCHECK(rcl_subscription_fini(&set_manual_mode_sub, &node));
    set_manual_mode_sub = rcl_get_zero_initialized_subscription();
  }

  if (manual_forward_cmd_sub.impl != NULL)
  {
    RCSOFTCHECK(rcl_subscription_fini(&manual_forward_cmd_sub, &node));
    manual_forward_cmd_sub = rcl_get_zero_initialized_subscription();
  }

  if (manual_reverse_cmd_sub.impl != NULL)
  {
    RCSOFTCHECK(rcl_subscription_fini(&manual_reverse_cmd_sub, &node));
    manual_reverse_cmd_sub = rcl_get_zero_initialized_subscription();
  }

  RCSOFTCHECK(rclc_executor_fini(&executor));
  executor = rclc_executor_get_zero_initialized_executor();

  if (node.impl != NULL)
  {
    RCSOFTCHECK(rcl_node_fini(&node));
    node = rcl_get_zero_initialized_node();
  }

  RCSOFTCHECK(rclc_support_fini(&support));
  support = (rclc_support_t){0};

  Serial.println("[micro-ROS] destroy done");
}

// ============================================================
// IO 控制任务
// 周期性读取输入、处理模式切换、合并本地/远程命令，
// 然后统一更新电机输出和共享状态。
// ============================================================
//
//
//
// ============================================================
void io_control_task(void *parameter)
{
  (void)parameter;

  const TickType_t period = pdMS_TO_TICKS(20);
  TickType_t last_wake_time = xTaskGetTickCount();

  for (;;)
  {
    uint32_t now_ms = millis();

    // --------------------------------------------------------
    // 读取原始输入
    // --------------------------------------------------------
    bool mode_toggle_raw = (digitalRead(MODE_TOGGLE_IN_PIN) == HIGH);
    bool btn_fwd_raw = (digitalRead(MANUAL_FORWARD_BTN_PIN) == HIGH);
    bool btn_rev_raw = (digitalRead(MANUAL_REVERSE_BTN_PIN) == HIGH);

    // --------------------------------------------------------
    // 更新消抖状态
    // --------------------------------------------------------
    bool mode_toggle_stable = debounce_update(&g_mode_toggle_db, mode_toggle_raw, now_ms);
    bool btn_fwd_stable = debounce_update(&g_btn_forward_db, btn_fwd_raw, now_ms);
    bool btn_rev_stable = debounce_update(&g_btn_reverse_db, btn_rev_raw, now_ms);

    bool mode_toggle_rising = debounce_rising_edge(&g_mode_toggle_db);

    int32_t encoder_now = get_encoder_count();

    ControllerState_t local_state;

    // 先复制一份共享状态，避免长时间占用互斥锁
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
    // 1) 本地模式切换按钮
    // --------------------------------------------------------
    if (mode_toggle_stable && mode_toggle_rising)
    {
      local_state.manual_mode = !local_state.manual_mode;

      Serial.print("[MODE] toggled -> ");
      Serial.println(local_state.manual_mode ? "MANUAL" : "AUTO");

      // 切回人工模式时，立即停止自动流程
      if (local_state.manual_mode)
      {
        g_auto_task_active = false;
        g_auto_waiting_first_detect = false;
        g_auto_state = AUTO_IDLE;
        g_start_auto_cmd = false;
        g_detect_done_cmd = false;
      }
    }

    // --------------------------------------------------------
    // 2) 处理远程模式切换命令，并同步到 GPIO15
    // LOW=自动模式，HIGH=人工模式
    // --------------------------------------------------------
    if (g_remote_manual_mode_pending)
    {
      local_state.manual_mode = g_remote_manual_mode_value;
      g_remote_manual_mode_pending = false;

      if (local_state.manual_mode)
      {
        g_auto_task_active = false;
        g_auto_waiting_first_detect = false;
        g_auto_state = AUTO_IDLE;
        g_start_auto_cmd = false;
        g_detect_done_cmd = false;
      }
      else
      {
        g_auto_forward_cmd = false;
        g_auto_reverse_cmd = false;
      }
    }

    digitalWrite(MODE_OUT_PIN, local_state.manual_mode ? HIGH : LOW);

    // --------------------------------------------------------
    // 3) 合并本地按钮和远程命令
    // --------------------------------------------------------
    const bool remote_forward_active = local_state.manual_mode && g_auto_forward_cmd;
    const bool remote_reverse_active = local_state.manual_mode && g_auto_reverse_cmd;
    const bool merged_forward = btn_fwd_stable || remote_forward_active;
    const bool merged_reverse = btn_rev_stable || remote_reverse_active;

    local_state.btn_forward = merged_forward;
    local_state.btn_reverse = merged_reverse;

    // --------------------------------------------------------
    // 4) 计算电机输出
    // --------------------------------------------------------
    bool motor_enable = false;
    MotorRunState_t motor_run = MOTOR_STOP;

    if (local_state.manual_mode)
    {
      // 人工模式下，前后命令互斥；同时按下则停机
      if (merged_forward && !merged_reverse)
      {
        motor_enable = true;
        motor_run = MOTOR_FORWARD;
      }
      else if (!merged_forward && merged_reverse)
      {
        motor_enable = true;
        motor_run = MOTOR_REVERSE;
      }
      else
      {
        motor_enable = false;
        motor_run = MOTOR_STOP;
      }

      g_auto_state = AUTO_IDLE;
    }
    else
    {
      // 自动模式下由自动巡检状态机决定电机输出
      process_auto_sequence(&motor_enable, &motor_run, encoder_now);
    }

    // --------------------------------------------------------
    // 5) 输出电机使能
    // --------------------------------------------------------
    digitalWrite(MOTOR_ENABLE_PIN, motor_enable ? HIGH : LOW);

    // --------------------------------------------------------
    // 6) 输出电机方向
    // 当前接线下：双 LOW=前进，双 HIGH=后退，一高一低=停止
    // --------------------------------------------------------
    if (motor_run == MOTOR_FORWARD)
    {
      // MOTOR_FORWARD now maps to the previous physical reverse relay state.
      digitalWrite(MOTOR_DIR_PIN_A, LOW);
      digitalWrite(MOTOR_DIR_PIN_B, LOW);
    }
    else if (motor_run == MOTOR_REVERSE)
    {
      digitalWrite(MOTOR_DIR_PIN_A, HIGH);
      digitalWrite(MOTOR_DIR_PIN_B, HIGH);
    }
    else
    {
      digitalWrite(MOTOR_DIR_PIN_A, HIGH);
      digitalWrite(MOTOR_DIR_PIN_B, LOW);
    }

    // --------------------------------------------------------
    // 7) 更新共享状态，供 micro-ROS 发布
    // --------------------------------------------------------
    local_state.motor_enable = motor_enable;
    local_state.motor_run = motor_run;
    local_state.encoder_count = encoder_now;
    if (!local_state.manual_mode && g_auto_task_active && encoder_now > g_detect_origin_count)
    {
      local_state.travel_m = (float)(encoder_now - g_detect_origin_count) / COUNTS_PER_METER;
    }
    else
    {
      local_state.travel_m = 0.0f;
    }
    local_state.auto_state = g_auto_state;
    local_state.update_ms = now_ms;

    if (xSemaphoreTake(ctrl_state_mutex, pdMS_TO_TICKS(10)) == pdTRUE)
    {
      g_ctrl_state = local_state;
      xSemaphoreGive(ctrl_state_mutex);
    }

    vTaskDelayUntil(&last_wake_time, period);
  }
}

// micro-ROS 任务
// 负责与 agent 建立连接、轮询订阅回调、周期发布控制器状态。
// ============================================================
void micro_ros_task(void *parameter)
{
  (void)parameter;

  ControllerState_t local_state;
  uint32_t last_ping_check_ms = 0;
  uint32_t last_publish_ms = 0;

  const uint32_t alive_check_period_ms = 2000;
  const uint32_t publish_period_ms = 200;

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

        // ----------------------------------------------------
        // 1) 处理订阅消息
        // ----------------------------------------------------
        RCSOFTCHECK(rclc_executor_spin_some(&executor, RCL_MS_TO_NS(10)));

        // ----------------------------------------------------
        // 2) 定期检查 agent 是否在线
        // ----------------------------------------------------
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

        // ----------------------------------------------------
        // 3) 周期发布状态话题
        // ----------------------------------------------------
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
            encoder_count_msg.data = local_state.encoder_count;
            travel_m_msg.data = local_state.travel_m;

            // motion_reached 只发布一次，到达后下一次发送立即清零
            motion_reached_msg.data = g_motion_reached_event;
            g_motion_reached_event = false;

            rcl_ret_t ret1 = rcl_publish(&motor_enable_pub, &motor_enable_msg, NULL);
            rcl_ret_t ret2 = rcl_publish(&control_mode_pub, &control_mode_msg, NULL);
            rcl_ret_t ret3 = rcl_publish(&motor_state_pub, &motor_state_msg, NULL);
            rcl_ret_t ret4 = rcl_publish(&btn_forward_pub, &btn_forward_msg, NULL);
            rcl_ret_t ret5 = rcl_publish(&btn_reverse_pub, &btn_reverse_msg, NULL);
            rcl_ret_t ret6 = rcl_publish(&motion_reached_pub, &motion_reached_msg, NULL);
            rcl_ret_t ret7 = rcl_publish(&encoder_count_pub, &encoder_count_msg, NULL);
            rcl_ret_t ret8 = rcl_publish(&travel_m_pub, &travel_m_msg, NULL);

            if (ret1 == RCL_RET_OK &&
                ret2 == RCL_RET_OK &&
                ret3 == RCL_RET_OK &&
                ret4 == RCL_RET_OK &&
                ret5 == RCL_RET_OK &&
                ret6 == RCL_RET_OK &&
                ret7 == RCL_RET_OK &&
                ret8 == RCL_RET_OK)
            {
              Serial.print("[publish] mode=");
              Serial.print(local_state.manual_mode ? "MANUAL" : "AUTO");
              Serial.print(" enable=");
              Serial.print(local_state.motor_enable ? "1" : "0");
              Serial.print(" motor_run=");
              Serial.print((int)local_state.motor_run);
              Serial.print(" enc=");
              Serial.print(local_state.encoder_count);
              Serial.print(" travel_m=");
              Serial.print(local_state.travel_m, 4);
              Serial.print(" auto_state=");
              Serial.print((int)local_state.auto_state);
              Serial.print(" reached=");
              Serial.println(motion_reached_msg.data ? "1" : "0");
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

        vTaskDelay(pdMS_TO_TICKS(20));
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

