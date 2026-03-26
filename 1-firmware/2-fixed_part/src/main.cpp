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
#define MODE_TOGGLE_IN_PIN      1

// 模式输出：LOW=自动，HIGH=手动
#define MODE_OUT_PIN            15

// 电机使能继电器输出：HIGH=吸合
#define MOTOR_ENABLE_PIN        37

// 电机方向输出
// 同为 HIGH -> 正转
// 同为 LOW  -> 反转
// 不同      -> 停止
#define MOTOR_DIR_PIN_A         35
#define MOTOR_DIR_PIN_B         36

// 手动按钮输入，高有效
#define MANUAL_FORWARD_BTN_PIN  2
#define MANUAL_REVERSE_BTN_PIN  3

// ============================================================
// 编码器 IO 定义
// ============================================================
#define ENCODER_A_PIN  45
#define ENCODER_B_PIN  46
#define ENCODER_Z_PIN  47

// ============================================================
// 编码器与运动参数
// ============================================================
// 编码器：360线，AB相4倍频 => 1440 counts/rev
constexpr int   ENCODER_LINES = 360;
constexpr int   ENCODER_X4_COUNTS_PER_REV = ENCODER_LINES * 4;

// 轮径 26mm = 0.026m
constexpr float WHEEL_DIAMETER_M = 0.026f;
constexpr float WHEEL_CIRCUMFERENCE_M = 3.1415926f * WHEEL_DIAMETER_M;

// 每米对应多少个编码器计数
constexpr float COUNTS_PER_METER = ENCODER_X4_COUNTS_PER_REV / WHEEL_CIRCUMFERENCE_M;

// 每次步进 0.3m
constexpr float AUTO_STEP_LENGTH_M = 0.3f;
constexpr int32_t AUTO_STEP_COUNTS = (int32_t)(COUNTS_PER_METER * AUTO_STEP_LENGTH_M + 0.5f);

// 设定总长度（这里先写死，后续可改成订阅参数）
constexpr float AUTO_TOTAL_LENGTH_M = 1.2f;
constexpr int32_t AUTO_TOTAL_COUNTS = (int32_t)(COUNTS_PER_METER * AUTO_TOTAL_LENGTH_M + 0.5f);

// 反向回原点的允许误差
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
  AUTO_MOVING_FORWARD,    // 前进中
  AUTO_WAIT_DETECT_DONE,  // 到位后等待检测完成
  AUTO_RETURNING_HOME,    // 回原点中
  AUTO_FINISHED           // 自动流程完成
} AutoState_t;

// ============================================================
// 按键消抖结构体
// ============================================================
typedef struct
{
  bool raw_state;              // 当前原始采样值
  bool stable_state;           // 当前稳定值
  bool last_stable_state;      // 上一次稳定值
  uint32_t last_change_ms;     // 原始值最后一次变化时间
} DebounceInput_t;

// ============================================================
// 控制器状态
// ============================================================
typedef struct
{
  bool manual_mode;          // true=手动, false=自动
  bool motor_enable;         // 当前电机使能状态
  MotorRunState_t motor_run; // 当前运行状态：1正转，0停止，-1反转

  bool btn_forward;          // 手动正转按钮状态（消抖后）
  bool btn_reverse;          // 手动反转按钮状态（消抖后）

  int32_t encoder_count;     // 当前编码器计数
  float travel_m;            // 相对自动起点位移(米)
  AutoState_t auto_state;    // 自动状态机状态

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
    0,            // encoder_count
    0.0f,         // travel_m
    AUTO_IDLE,    // auto_state
    0
};

SemaphoreHandle_t ctrl_state_mutex = NULL;

// ------------------------------------------------------------
// 自动模式命令变量
// ------------------------------------------------------------
// 注意：这里保留原本的自动控制占位变量，但本版自动流程主要用 start_auto/detect_done
volatile bool g_auto_enable_cmd = false;
volatile bool g_auto_forward_cmd = false;
volatile bool g_auto_reverse_cmd = false;
volatile bool g_remote_manual_mode_pending = false;
volatile bool g_remote_manual_mode_value = false;

// ------------------------------------------------------------
// 编码器相关全局变量
// ------------------------------------------------------------
volatile int32_t g_encoder_count = 0;
volatile uint8_t g_encoder_prev_ab = 0;
portMUX_TYPE g_encoder_mux = portMUX_INITIALIZER_UNLOCKED;

// ------------------------------------------------------------
// 自动运行状态机变量
// ------------------------------------------------------------
volatile bool g_start_auto_cmd = false;        // 上位机发来：开始自动运行
volatile bool g_detect_done_cmd = false;       // 上位机发来：当前位置检测完成
volatile bool g_motion_reached_event = false;  // 本次步进到位事件，供发布线程读取

AutoState_t g_auto_state = AUTO_IDLE;
bool g_auto_task_active = false;

int32_t g_auto_home_count = 0;          // 自动运行起点
int32_t g_auto_target_count = 0;        // 当前步进目标
int32_t g_auto_total_target_count = 0;  // 总目标终点

// ------------------------------------------------------------
// 按键消抖对象
// ------------------------------------------------------------
DebounceInput_t g_mode_toggle_db = {false, false, false, 0};
DebounceInput_t g_btn_forward_db = {false, false, false, 0};
DebounceInput_t g_btn_reverse_db = {false, false, false, 0};

// ============================================================
// micro-ROS 对象
// ============================================================
rcl_allocator_t allocator;
rclc_support_t support;
rcl_node_t node;
rclc_executor_t executor;

// ------------------------------------------------------------
// publishers
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
// subscribers
// ------------------------------------------------------------
rcl_subscription_t start_auto_sub;
rcl_subscription_t detect_done_sub;
rcl_subscription_t set_manual_mode_sub;
rcl_subscription_t manual_forward_cmd_sub;
rcl_subscription_t manual_reverse_cmd_sub;

// ------------------------------------------------------------
// msgs
// ------------------------------------------------------------
std_msgs__msg__Bool motor_enable_msg;
std_msgs__msg__Bool control_mode_msg;
std_msgs__msg__Int8 motor_state_msg;
std_msgs__msg__Bool btn_forward_msg;
std_msgs__msg__Bool btn_reverse_msg;
std_msgs__msg__Bool motion_reached_msg;
std_msgs__msg__Int32 encoder_count_msg;
std_msgs__msg__Float32 travel_m_msg;

// subscriber msg buffer
std_msgs__msg__Bool start_auto_msg;
std_msgs__msg__Bool detect_done_msg;
std_msgs__msg__Bool set_manual_mode_msg;
std_msgs__msg__Bool manual_forward_cmd_msg;
std_msgs__msg__Bool manual_reverse_cmd_msg;

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
// 编码器中断与辅助函数
// ============================================================
// 说明：
// 1) A/B 相都绑定 CHANGE 中断
// 2) 每次中断读取当前 AB 状态
// 3) 通过前一状态 + 当前状态查表，实现 4 倍频计数
// 4) 合法正向跳变 +1，合法反向跳变 -1，非法跳变 0
// ============================================================
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
  // Keep forward travel positive across encoder_count/travel_m/UI telemetry.
  g_encoder_count -= quad_table[idx];
  g_encoder_prev_ab = ab;
  portEXIT_CRITICAL_ISR(&g_encoder_mux);
}

// 安全读取编码器计数
int32_t get_encoder_count()
{
  int32_t cnt;
  portENTER_CRITICAL(&g_encoder_mux);
  cnt = g_encoder_count;
  portEXIT_CRITICAL(&g_encoder_mux);
  return cnt;
}

// 安全重置编码器计数，并同步 prev_ab
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
// 按键消抖函数
// ============================================================
// raw: 当前原始采样值
// now_ms: 当前毫秒计时
// 返回值：stable_state
// ============================================================
bool debounce_update(DebounceInput_t *db, bool raw, uint32_t now_ms)
{
  // 原始值变化，记录变化时间
  if (raw != db->raw_state)
  {
    db->raw_state = raw;
    db->last_change_ms = now_ms;
  }

  // 如果原始值已经持续稳定超过 DEBOUNCE_MS，则更新稳定值
  if ((now_ms - db->last_change_ms) >= DEBOUNCE_MS)
  {
    db->last_stable_state = db->stable_state;
    db->stable_state = db->raw_state;
  }

  return db->stable_state;
}

// 检测稳定状态的上升沿
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
// 自动流程控制
// ============================================================
// 流程：
// 1) 收到 start_auto
// 2) 记录当前位置为 home
// 3) 前进 0.1m 到目标点
// 4) 到位后停车并发布 motion_reached
// 5) 等待 detect_done
// 6) 再前进 0.1m
// 7) 重复直到达到总长度
// 8) 然后反转回起点
// ============================================================
void start_auto_sequence()
{
  int32_t now_cnt = get_encoder_count();

  g_auto_task_active = true;
  g_auto_state = AUTO_MOVING_FORWARD;

  g_auto_home_count = now_cnt;
  g_auto_target_count = g_auto_home_count + AUTO_STEP_COUNTS;
  g_auto_total_target_count = g_auto_home_count + AUTO_TOTAL_COUNTS;

  // 防止第一步超过总目标
  if (g_auto_target_count > g_auto_total_target_count)
  {
    g_auto_target_count = g_auto_total_target_count;
  }

  g_detect_done_cmd = false;
  g_motion_reached_event = false;

  Serial.println("[AUTO] start sequence");
  Serial.print("[AUTO] home_count = ");
  Serial.println(g_auto_home_count);
  Serial.print("[AUTO] first target = ");
  Serial.println(g_auto_target_count);
  Serial.print("[AUTO] total target = ");
  Serial.println(g_auto_total_target_count);
}

void process_auto_sequence(bool *motor_enable, MotorRunState_t *motor_run, int32_t encoder_now)
{
  // 默认停机，后面按状态决定是否动作
  *motor_enable = false;
  *motor_run = MOTOR_STOP;

  // 收到开始命令且当前没在自动流程中 -> 启动
  if (g_start_auto_cmd && !g_auto_task_active)
  {
    g_start_auto_cmd = false;
    start_auto_sequence();
  }

  // 当前没有自动任务 -> 保持空闲
  if (!g_auto_task_active)
  {
    g_auto_state = AUTO_IDLE;
    *motor_enable = false;
    *motor_run = MOTOR_STOP;
    return;
  }

  switch (g_auto_state)
  {
    // --------------------------------------------------------
    // 状态1：前进中
    // --------------------------------------------------------
    case AUTO_MOVING_FORWARD:
    {
      *motor_enable = true;
      *motor_run = MOTOR_FORWARD;

      // 到达本段目标
      if (encoder_now >= g_auto_target_count)
      {
        *motor_enable = false;
        *motor_run = MOTOR_STOP;

        // 产生一次“到位事件”
        g_motion_reached_event = true;

        // 如果已经达到总长度，开始回原点
        if (g_auto_target_count >= g_auto_total_target_count)
        {
          g_auto_state = AUTO_RETURNING_HOME;
          Serial.println("[AUTO] total length reached, return home");
        }
        else
        {
          // 否则等待检测完成
          g_auto_state = AUTO_WAIT_DETECT_DONE;
          Serial.println("[AUTO] step reached, wait detect_done");
        }
      }
      break;
    }

    // --------------------------------------------------------
    // 状态2：等待检测完成
    // --------------------------------------------------------
    case AUTO_WAIT_DETECT_DONE:
    {
      *motor_enable = false;
      *motor_run = MOTOR_STOP;

      if (g_detect_done_cmd)
      {
        g_detect_done_cmd = false;

        // 下一个目标点 = 当前目标 + 一个步长
        g_auto_target_count += AUTO_STEP_COUNTS;

        // 不能超过总终点
        if (g_auto_target_count > g_auto_total_target_count)
        {
          g_auto_target_count = g_auto_total_target_count;
        }

        g_auto_state = AUTO_MOVING_FORWARD;

        Serial.print("[AUTO] detect done, next target = ");
        Serial.println(g_auto_target_count);
      }
      break;
    }

    // --------------------------------------------------------
    // 状态3：反向回起点
    // --------------------------------------------------------
    case AUTO_RETURNING_HOME:
    {
      *motor_enable = true;
      *motor_run = MOTOR_REVERSE;

      if (encoder_now <= (g_auto_home_count + HOME_TOLERANCE_COUNTS))
      {
        *motor_enable = false;
        *motor_run = MOTOR_STOP;

        g_auto_task_active = false;
        g_auto_state = AUTO_FINISHED;

        Serial.println("[AUTO] returned home, finished");
      }
      break;
    }

    // --------------------------------------------------------
    // 状态4：完成 / 异常兜底
    // --------------------------------------------------------
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
  // 普通输入输出
  pinMode(MODE_TOGGLE_IN_PIN, INPUT_PULLDOWN);
  pinMode(MODE_OUT_PIN, OUTPUT);

  pinMode(MOTOR_ENABLE_PIN, OUTPUT);
  pinMode(MOTOR_DIR_PIN_A, OUTPUT);
  pinMode(MOTOR_DIR_PIN_B, OUTPUT);

  pinMode(MANUAL_FORWARD_BTN_PIN, INPUT_PULLDOWN);
  pinMode(MANUAL_REVERSE_BTN_PIN, INPUT_PULLDOWN);

  // ----------------------------------------------------------
  // 编码器输入
  // NPN型编码器建议外部上拉到 3.3V
  // 这里先用 INPUT_PULLUP 方便测试
  // 正式工程仍建议使用外部上拉电阻
  // ----------------------------------------------------------
  pinMode(ENCODER_A_PIN, INPUT_PULLUP);
  pinMode(ENCODER_B_PIN, INPUT_PULLUP);
  pinMode(ENCODER_Z_PIN, INPUT_PULLUP);

  // 初始化编码器状态
  reset_encoder_count(0);

  // A/B 相双边沿中断
  attachInterrupt(digitalPinToInterrupt(ENCODER_A_PIN), encoder_isr, CHANGE);
  attachInterrupt(digitalPinToInterrupt(ENCODER_B_PIN), encoder_isr, CHANGE);

  // 上电默认：自动模式、继电器断开、电机停止
  digitalWrite(MODE_OUT_PIN, LOW);       // 自动模式
  digitalWrite(MOTOR_ENABLE_PIN, LOW);   // 电机断电
  digitalWrite(MOTOR_DIR_PIN_A, HIGH);   // 不同电平表示停机
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
// 创建 micro-ROS 实体
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
// IO控制任务
// 负责：
// 1) 按键采样与消抖
// 2) 手动/自动模式切换
// 3) 自动状态机控制
// 4) 控制电机输出
// 5) 更新共享状态
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
    // 原始输入采样
    // --------------------------------------------------------
    bool mode_toggle_raw = (digitalRead(MODE_TOGGLE_IN_PIN) == HIGH);
    bool btn_fwd_raw = (digitalRead(MANUAL_FORWARD_BTN_PIN) == HIGH);
    bool btn_rev_raw = (digitalRead(MANUAL_REVERSE_BTN_PIN) == HIGH);

    // --------------------------------------------------------
    // 消抖处理
    // --------------------------------------------------------
    bool mode_toggle_stable = debounce_update(&g_mode_toggle_db, mode_toggle_raw, now_ms);
    bool btn_fwd_stable = debounce_update(&g_btn_forward_db, btn_fwd_raw, now_ms);
    bool btn_rev_stable = debounce_update(&g_btn_reverse_db, btn_rev_raw, now_ms);

    bool mode_toggle_rising = debounce_rising_edge(&g_mode_toggle_db);

    int32_t encoder_now = get_encoder_count();

    ControllerState_t local_state;

    // 先取共享状态
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
    // 1) 模式切换按键：稳定上升沿翻转模式
    // --------------------------------------------------------
    if (mode_toggle_stable && mode_toggle_rising)
    {
      local_state.manual_mode = !local_state.manual_mode;

      Serial.print("[MODE] toggled -> ");
      Serial.println(local_state.manual_mode ? "MANUAL" : "AUTO");

      // 切到手动时，强制退出自动流程
      if (local_state.manual_mode)
      {
        g_auto_task_active = false;
        g_auto_state = AUTO_IDLE;
        g_start_auto_cmd = false;
        g_detect_done_cmd = false;
      }
    }

    // --------------------------------------------------------
    // 2) 模式输出 GPIO15
    // LOW=自动，HIGH=手动
    // --------------------------------------------------------
    if (g_remote_manual_mode_pending)
    {
      local_state.manual_mode = g_remote_manual_mode_value;
      g_remote_manual_mode_pending = false;

      if (local_state.manual_mode)
      {
        g_auto_task_active = false;
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
    // 3) 更新按钮状态（用消抖后的稳定值）
    // --------------------------------------------------------
    const bool remote_forward_active = local_state.manual_mode && g_auto_forward_cmd;
    const bool remote_reverse_active = local_state.manual_mode && g_auto_reverse_cmd;
    const bool merged_forward = btn_fwd_stable || remote_forward_active;
    const bool merged_reverse = btn_rev_stable || remote_reverse_active;

    local_state.btn_forward = merged_forward;
    local_state.btn_reverse = merged_reverse;

    // --------------------------------------------------------
    // 4) 计算电机命令
    // --------------------------------------------------------
    bool motor_enable = false;
    MotorRunState_t motor_run = MOTOR_STOP;

    if (local_state.manual_mode)
    {
      // 手动模式：按钮控制
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
      // 自动模式：由自动流程状态机控制
      process_auto_sequence(&motor_enable, &motor_run, encoder_now);
    }

    // --------------------------------------------------------
    // 5) 输出电机使能
    // --------------------------------------------------------
    digitalWrite(MOTOR_ENABLE_PIN, motor_enable ? HIGH : LOW);

    // --------------------------------------------------------
    // 6) 输出电机方向
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
      digitalWrite(MOTOR_DIR_PIN_A, HIGH);
      digitalWrite(MOTOR_DIR_PIN_B, LOW);
    }

    // --------------------------------------------------------
    // 7) 回写共享状态
    // --------------------------------------------------------
    local_state.motor_enable = motor_enable;
    local_state.motor_run = motor_run;
    local_state.encoder_count = encoder_now;
    local_state.travel_m = (float)(encoder_now - g_auto_home_count) / COUNTS_PER_METER;
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

// ============================================================
// micro-ROS任务
// 负责：
// 1) agent 连接/断线重连
// 2) spin executor，处理订阅回调
// 3) 周期发布状态
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
        // 1) 处理订阅回调
        // ----------------------------------------------------
        RCSOFTCHECK(rclc_executor_spin_some(&executor, RCL_MS_TO_NS(10)));

        // ----------------------------------------------------
        // 2) 定期检测 agent 是否在线
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
        // 3) 周期发布状态
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

            // 到位事件：有事件时发布一次 true，随后清零
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
