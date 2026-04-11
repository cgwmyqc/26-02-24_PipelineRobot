#include <Arduino.h>
#include <micro_ros_platformio.h>
#include <rmw_microros/rmw_microros.h>
#include <SPI.h>
#include <Ethernet.h>
#include <EthernetUdp.h>
#include <Update.h>
#include <mbedtls/sha256.h>
#include <rclc/rclc.h>
#include <rclc/executor.h>
#include <std_msgs/msg/bool.h>
#include <std_msgs/msg/float32.h>
#include <std_msgs/msg/int32.h>
#include <std_msgs/msg/string.h>

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

#define I2C_SDA 33
#define I2C_SCL 34
#define I2C_ADR 0x77

#define W5500_CS 14
#define W5500_RST 9
#define W5500_INT 10
#define W5500_MISO 12
#define W5500_MOSI 11
#define W5500_SCK 13

#define WATERSENSOR_PIN 38
#define SONAR_ENABLE 39

static const char *OTA_COMMAND_TOPIC = "/mobile_part/ota/command";
static const char *OTA_STATUS_TOPIC = "/mobile_part/ota/status";
static const char *OTA_PROGRESS_TOPIC = "/mobile_part/ota/progress";
static const size_t OTA_URL_BUFFER_SIZE = 256;
static const size_t OTA_JOB_BUFFER_SIZE = 64;
static const size_t OTA_SHA256_BUFFER_SIZE = 65;
static const size_t OTA_FILENAME_BUFFER_SIZE = 96;
static const size_t OTA_COMMAND_BUFFER_SIZE = 512;
static const size_t OTA_STATUS_BUFFER_SIZE = 256;
static const size_t OTA_HTTP_BUFFER_SIZE = 1024;

Adafruit_BME280 env_sensor;

typedef struct
{
  float temperature;
  float humidity;
  bool valid;
  uint32_t update_ms;
} SensorData_t;

typedef enum
{
  AGENT_DISCONNECTED = 0,
  AGENT_CONNECTING,
  AGENT_CONNECTED
} AgentState_t;

typedef struct
{
  bool pending;
  bool in_progress;
  char job_id[OTA_JOB_BUFFER_SIZE];
  char url[OTA_URL_BUFFER_SIZE];
  char sha256[OTA_SHA256_BUFFER_SIZE];
  char filename[OTA_FILENAME_BUFFER_SIZE];
  size_t size;
} OtaRequest_t;

SensorData_t g_sensor_data = {0.0f, 0.0f, false};
SemaphoreHandle_t sensor_data_mutex = NULL;
AgentState_t g_agent_state = AGENT_DISCONNECTED;
volatile bool isInWater = false;
volatile bool g_ota_request_pending = false;
OtaRequest_t g_ota_request = {false, false, "", "", "", "", 0};

byte mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED};
IPAddress client_ip(192, 168, 1, 177);
IPAddress dns(192, 168, 1, 1);
IPAddress gateway(192, 168, 1, 1);
IPAddress netmask(255, 255, 255, 0);
IPAddress agent_ip(192, 168, 1, 132);
const uint16_t agent_port = 8888;
const uint16_t client_port = 8889;

rcl_allocator_t allocator;
rclc_support_t support;
rclc_executor_t executor;
rcl_node_t node;
rcl_publisher_t temp_pub;
rcl_publisher_t hum_pub;
rcl_publisher_t inwater_pub;
rcl_publisher_t ota_status_pub;
rcl_publisher_t ota_progress_pub;
rcl_subscription_t ota_command_sub;
std_msgs__msg__Float32 temp_msg;
std_msgs__msg__Float32 hum_msg;
std_msgs__msg__Bool inwater_msg;
std_msgs__msg__Int32 ota_progress_msg;
std_msgs__msg__String ota_command_msg;
std_msgs__msg__String ota_status_msg;

char g_ota_command_buffer[OTA_COMMAND_BUFFER_SIZE];
char g_ota_status_buffer[OTA_STATUS_BUFFER_SIZE];

EthernetUDP udp;
bool udp_opened = false;

bool init_env_sensor();
bool init_ethernet();
bool init_water_sensor();
bool create_microros_entities();
void destroy_microros_entities();
bool ping_agent();
bool check_agent_alive();
void env_sensor_task(void *parameter);
void micro_ros_task(void *parameter);
void water_sensor_sonar_task(void *parameter);

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

size_t custom_udp_transport_write(struct uxrCustomTransport *transport, const uint8_t *buf, size_t len, uint8_t *errcode)
{
  (void)transport;
  (void)errcode;
  udp.beginPacket(agent_ip, agent_port);
  size_t written = udp.write(buf, len);
  udp.endPacket();
  return written;
}

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

bool json_extract_string(const char *json, const char *key, char *out, size_t out_size)
{
  if (json == NULL || key == NULL || out == NULL || out_size == 0)
  {
    return false;
  }

  char pattern[48];
  snprintf(pattern, sizeof(pattern), "\"%s\"", key);
  const char *key_pos = strstr(json, pattern);
  if (key_pos == NULL)
  {
    return false;
  }

  const char *colon_pos = strchr(key_pos + strlen(pattern), ':');
  if (colon_pos == NULL)
  {
    return false;
  }

  const char *first_quote = strchr(colon_pos, '"');
  if (first_quote == NULL)
  {
    return false;
  }

  const char *cursor = first_quote + 1;
  size_t written = 0;
  while (*cursor != '\0')
  {
    if (*cursor == '"' && *(cursor - 1) != '\\')
    {
      break;
    }
    if (written + 1 < out_size)
    {
      out[written++] = *cursor;
    }
    cursor++;
  }

  if (*cursor != '"')
  {
    return false;
  }

  out[written] = '\0';
  return true;
}

bool json_extract_size(const char *json, const char *key, size_t *value)
{
  if (json == NULL || key == NULL || value == NULL)
  {
    return false;
  }

  char pattern[48];
  snprintf(pattern, sizeof(pattern), "\"%s\"", key);
  const char *key_pos = strstr(json, pattern);
  if (key_pos == NULL)
  {
    return false;
  }

  const char *colon_pos = strchr(key_pos + strlen(pattern), ':');
  if (colon_pos == NULL)
  {
    return false;
  }

  char *end_ptr = NULL;
  unsigned long parsed = strtoul(colon_pos + 1, &end_ptr, 10);
  if (end_ptr == colon_pos + 1)
  {
    return false;
  }

  *value = (size_t)parsed;
  return true;
}

void ota_set_string_msg(std_msgs__msg__String *msg, char *buffer, size_t buffer_size, const char *value)
{
  if (msg == NULL || buffer == NULL || buffer_size == 0 || value == NULL)
  {
    return;
  }

  if (buffer != value)
  {
    snprintf(buffer, buffer_size, "%s", value);
  }
  msg->data.data = buffer;
  msg->data.size = strlen(buffer);
  msg->data.capacity = buffer_size;
}

void ota_publish_progress(int32_t progress)
{
  if (ota_progress_pub.impl == NULL)
  {
    return;
  }

  ota_progress_msg.data = progress;
  RCSOFTCHECK(rcl_publish(&ota_progress_pub, &ota_progress_msg, NULL));
}

void ota_publish_status(const char *job_id, const char *phase, const char *message, int32_t progress)
{
  if (ota_status_pub.impl == NULL)
  {
    return;
  }

  snprintf(
      g_ota_status_buffer,
      sizeof(g_ota_status_buffer),
      "{\"job_id\":\"%s\",\"phase\":\"%s\",\"message\":\"%s\",\"progress\":%ld}",
      job_id == NULL ? "" : job_id,
      phase == NULL ? "idle" : phase,
      message == NULL ? "" : message,
      (long)progress);
  ota_set_string_msg(&ota_status_msg, g_ota_status_buffer, sizeof(g_ota_status_buffer), g_ota_status_buffer);
  RCSOFTCHECK(rcl_publish(&ota_status_pub, &ota_status_msg, NULL));
}

bool ota_parse_http_url(const char *url, char *host, size_t host_size, uint16_t *port, char *path, size_t path_size)
{
  if (url == NULL || host == NULL || path == NULL || port == NULL)
  {
    return false;
  }

  const char *cursor = strstr(url, "http://");
  if (cursor != url)
  {
    return false;
  }
  cursor += 7;

  const char *path_pos = strchr(cursor, '/');
  const char *host_end = path_pos == NULL ? url + strlen(url) : path_pos;
  const char *port_pos = NULL;
  for (const char *it = cursor; it < host_end; ++it)
  {
    if (*it == ':')
    {
      port_pos = it;
      break;
    }
  }

  size_t host_len = (size_t)((port_pos != NULL ? port_pos : host_end) - cursor);
  if (host_len == 0 || host_len + 1 > host_size)
  {
    return false;
  }
  memcpy(host, cursor, host_len);
  host[host_len] = '\0';

  *port = 80;
  if (port_pos != NULL)
  {
    *port = (uint16_t)atoi(port_pos + 1);
  }

  snprintf(path, path_size, "%s", path_pos == NULL ? "/" : path_pos);
  return true;
}

void ota_command_callback(const void *msg_in)
{
  const std_msgs__msg__String *msg = (const std_msgs__msg__String *)msg_in;
  if (msg == NULL || msg->data.data == NULL || msg->data.size == 0)
  {
    return;
  }

  char payload[OTA_COMMAND_BUFFER_SIZE];
  size_t copy_len = msg->data.size;
  if (copy_len >= sizeof(payload))
  {
    copy_len = sizeof(payload) - 1;
  }
  memcpy(payload, msg->data.data, copy_len);
  payload[copy_len] = '\0';

  if (g_ota_request.pending || g_ota_request.in_progress)
  {
    char rejected_job_id[OTA_JOB_BUFFER_SIZE] = "";
    json_extract_string(payload, "job_id", rejected_job_id, sizeof(rejected_job_id));
    ota_publish_status(rejected_job_id, "error", "设备正在刷写中，请稍后再试", ota_progress_msg.data);
    return;
  }

  OtaRequest_t request = {false, false, "", "", "", "", 0};
  if (!json_extract_string(payload, "job_id", request.job_id, sizeof(request.job_id)) ||
      !json_extract_string(payload, "url", request.url, sizeof(request.url)) ||
      !json_extract_string(payload, "sha256", request.sha256, sizeof(request.sha256)) ||
      !json_extract_string(payload, "filename", request.filename, sizeof(request.filename)) ||
      !json_extract_size(payload, "size", &request.size))
  {
    ota_publish_status("", "error", "OTA 命令字段缺失", 0);
    return;
  }

  g_ota_request = request;
  g_ota_request.pending = true;
  g_ota_request.in_progress = false;
  g_ota_request_pending = true;
  ota_publish_status(g_ota_request.job_id, "queued", "OTA 命令已接收，等待开始刷写", 0);
}

bool ota_download_and_apply()
{
  if (!g_ota_request.pending)
  {
    return false;
  }

  char host[64];
  char path[192];
  uint16_t port = 80;
  if (!ota_parse_http_url(g_ota_request.url, host, sizeof(host), &port, path, sizeof(path)))
  {
    ota_publish_status(g_ota_request.job_id, "error", "OTA URL 无效", 0);
    g_ota_request.pending = false;
    g_ota_request_pending = false;
    return false;
  }

  EthernetClient client;
  ota_publish_progress(0);
  ota_publish_status(g_ota_request.job_id, "downloading", "开始下载固件", 0);

  if (!client.connect(host, port))
  {
    ota_publish_status(g_ota_request.job_id, "error", "无法连接固件服务器", 0);
    g_ota_request.pending = false;
    g_ota_request_pending = false;
    return false;
  }

  client.print("GET ");
  client.print(path);
  client.print(" HTTP/1.1\r\nHost: ");
  client.print(host);
  client.print("\r\nConnection: close\r\n\r\n");

  String status_line = client.readStringUntil('\n');
  status_line.trim();
  if (status_line.indexOf("200") < 0)
  {
    client.stop();
    ota_publish_status(g_ota_request.job_id, "error", "固件下载 HTTP 状态异常", 0);
    g_ota_request.pending = false;
    g_ota_request_pending = false;
    return false;
  }

  size_t content_length = 0;
  while (client.connected())
  {
    String header_line = client.readStringUntil('\n');
    header_line.trim();
    if (header_line.length() == 0)
    {
      break;
    }
    if (header_line.startsWith("Content-Length:"))
    {
      content_length = (size_t)header_line.substring(15).toInt();
    }
  }

  if (g_ota_request.size > 0 && content_length > 0 && g_ota_request.size != content_length)
  {
    client.stop();
    ota_publish_status(g_ota_request.job_id, "error", "固件大小与声明不一致", 0);
    g_ota_request.pending = false;
    g_ota_request_pending = false;
    return false;
  }

  size_t update_size = g_ota_request.size > 0 ? g_ota_request.size : content_length;
  if (!Update.begin(update_size == 0 ? UPDATE_SIZE_UNKNOWN : update_size))
  {
    client.stop();
    ota_publish_status(g_ota_request.job_id, "error", "OTA 分区初始化失败", 0);
    g_ota_request.pending = false;
    g_ota_request_pending = false;
    return false;
  }

  ota_publish_status(g_ota_request.job_id, "writing", "正在写入固件", 0);

  mbedtls_sha256_context sha_ctx;
  mbedtls_sha256_init(&sha_ctx);
  mbedtls_sha256_starts(&sha_ctx, 0);

  uint8_t buffer[OTA_HTTP_BUFFER_SIZE];
  size_t total_written = 0;
  int32_t last_progress = -1;
  uint32_t download_deadline = millis();

  while (client.connected() || client.available())
  {
    int available = client.available();
    if (available <= 0)
    {
      if (millis() - download_deadline > 5000)
      {
        Update.abort();
        client.stop();
        ota_publish_status(g_ota_request.job_id, "error", "固件下载超时", last_progress < 0 ? 0 : last_progress);
        g_ota_request.pending = false;
        g_ota_request_pending = false;
        return false;
      }
      vTaskDelay(pdMS_TO_TICKS(10));
      continue;
    }

    download_deadline = millis();
    size_t to_read = (size_t)available;
    if (to_read > sizeof(buffer))
    {
      to_read = sizeof(buffer);
    }

    int read_len = client.read(buffer, to_read);
    if (read_len <= 0)
    {
      continue;
    }

    mbedtls_sha256_update(&sha_ctx, buffer, (size_t)read_len);
    size_t written = Update.write(buffer, (size_t)read_len);
    if (written != (size_t)read_len)
    {
      mbedtls_sha256_free(&sha_ctx);
      Update.abort();
      client.stop();
      ota_publish_status(g_ota_request.job_id, "error", "固件写入失败", last_progress < 0 ? 0 : last_progress);
      g_ota_request.pending = false;
      g_ota_request_pending = false;
      return false;
    }

    total_written += written;
    size_t expected_total = g_ota_request.size > 0 ? g_ota_request.size : content_length;
    int32_t progress = expected_total > 0 ? (int32_t)((total_written * 100UL) / expected_total) : 0;
    if (progress > 100)
    {
      progress = 100;
    }

    if (progress != last_progress)
    {
      last_progress = progress;
      ota_publish_progress(progress);
      ota_publish_status(g_ota_request.job_id, "writing", "正在写入固件", progress);
    }
  }

  client.stop();

  if (g_ota_request.size > 0 && total_written != g_ota_request.size)
  {
    mbedtls_sha256_free(&sha_ctx);
    Update.abort();
    ota_publish_status(g_ota_request.job_id, "error", "固件下载不完整", last_progress < 0 ? 0 : last_progress);
    g_ota_request.pending = false;
    g_ota_request_pending = false;
    return false;
  }

  ota_publish_status(g_ota_request.job_id, "validating", "正在校验固件", 100);

  uint8_t digest[32];
  mbedtls_sha256_finish(&sha_ctx, digest);
  mbedtls_sha256_free(&sha_ctx);

  char digest_hex[65];
  for (size_t i = 0; i < sizeof(digest); ++i)
  {
    snprintf(digest_hex + i * 2, sizeof(digest_hex) - i * 2, "%02x", digest[i]);
  }

  if (strcasecmp(digest_hex, g_ota_request.sha256) != 0)
  {
    Update.abort();
    ota_publish_status(g_ota_request.job_id, "error", "固件 SHA256 校验失败", 100);
    g_ota_request.pending = false;
    g_ota_request_pending = false;
    return false;
  }

  if (!Update.end(true))
  {
    ota_publish_status(g_ota_request.job_id, "error", "OTA 刷写结束失败", 100);
    g_ota_request.pending = false;
    g_ota_request_pending = false;
    return false;
  }

  g_ota_request.pending = false;
  g_ota_request_pending = false;
  ota_publish_progress(100);
  ota_publish_status(g_ota_request.job_id, "success", "固件刷写成功，设备即将重启", 100);
  delay(1200);
  ESP.restart();
  return true;
}

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

bool init_water_sensor()
{
  pinMode(WATERSENSOR_PIN, INPUT_PULLUP);
  Serial.println("[WATER_SENSOR] Init done");
  pinMode(SONAR_ENABLE, OUTPUT);
  Serial.println("[SONAR] Init done");
  return true;
}

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

bool create_microros_entities()
{
  Serial.println("[micro-ROS] create node and publisher begin");

  allocator = rcl_get_default_allocator();

  support = (rclc_support_t){0};
  executor = rclc_executor_get_zero_initialized_executor();
  node = rcl_get_zero_initialized_node();
  temp_pub = rcl_get_zero_initialized_publisher();
  hum_pub = rcl_get_zero_initialized_publisher();
  inwater_pub = rcl_get_zero_initialized_publisher();
  ota_status_pub = rcl_get_zero_initialized_publisher();
  ota_progress_pub = rcl_get_zero_initialized_publisher();
  ota_command_sub = rcl_get_zero_initialized_subscription();

  ota_command_msg.data.data = g_ota_command_buffer;
  ota_command_msg.data.size = 0;
  ota_command_msg.data.capacity = sizeof(g_ota_command_buffer);
  ota_status_msg.data.data = g_ota_status_buffer;
  ota_status_msg.data.size = 0;
  ota_status_msg.data.capacity = sizeof(g_ota_status_buffer);

  RCCHECK(rclc_support_init(&support, 0, NULL, &allocator));
  RCCHECK(rclc_node_init_default(&node, "esp32_mobile_node", "", &support));

  RCCHECK(rclc_publisher_init_default(&temp_pub, &node, ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, Float32), "/env_sensor/temperature"));
  RCCHECK(rclc_publisher_init_default(&hum_pub, &node, ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, Float32), "/env_sensor/humidity"));
  RCCHECK(rclc_publisher_init_default(&inwater_pub, &node, ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, Bool), "/water_sensor"));
  RCCHECK(rclc_publisher_init_default(&ota_status_pub, &node, ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, String), OTA_STATUS_TOPIC));
  RCCHECK(rclc_publisher_init_default(&ota_progress_pub, &node, ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, Int32), OTA_PROGRESS_TOPIC));

  RCCHECK(rclc_subscription_init_default(
      &ota_command_sub,
      &node,
      ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, String),
      OTA_COMMAND_TOPIC));

  RCCHECK(rclc_executor_init(&executor, &support.context, 1, &allocator));
  RCCHECK(rclc_executor_add_subscription(&executor, &ota_command_sub, &ota_command_msg, &ota_command_callback, ON_NEW_DATA));

  ota_publish_status("", "idle", "OTA 服务就绪", 0);
  Serial.println("[micro-ROS] create node and publisher ok");
  return true;
}

void destroy_microros_entities()
{
  Serial.println("[micro-ROS] destroy node&pub begin");

  if (ota_command_sub.impl != NULL)
  {
    RCSOFTCHECK(rcl_subscription_fini(&ota_command_sub, &node));
    ota_command_sub = rcl_get_zero_initialized_subscription();
  }

  if (ota_progress_pub.impl != NULL)
  {
    RCSOFTCHECK(rcl_publisher_fini(&ota_progress_pub, &node));
    ota_progress_pub = rcl_get_zero_initialized_publisher();
  }

  if (ota_status_pub.impl != NULL)
  {
    RCSOFTCHECK(rcl_publisher_fini(&ota_status_pub, &node));
    ota_status_pub = rcl_get_zero_initialized_publisher();
  }

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

  RCSOFTCHECK(rclc_executor_fini(&executor));
  executor = rclc_executor_get_zero_initialized_executor();

  if (node.impl != NULL)
  {
    RCSOFTCHECK(rcl_node_fini(&node));
    node = rcl_get_zero_initialized_node();
  }

  RCSOFTCHECK(rclc_support_fini(&support));
  support = (rclc_support_t){0};

  Serial.println("[micro-ROS] destroy node&pub done");
}

void env_sensor_task(void *parameter)
{
  (void)parameter;

  const TickType_t period = pdMS_TO_TICKS(1000);
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

void micro_ros_task(void *parameter)
{
  (void)parameter;

  SensorData_t local_data;
  uint32_t last_ping_check_ms = 0;
  uint32_t last_env_publish_ms = 0;
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
          last_env_publish_ms = millis();
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
        RCSOFTCHECK(rclc_executor_spin_some(&executor, RCL_MS_TO_NS(10)));

        if (g_ota_request_pending && !g_ota_request.in_progress)
        {
          g_ota_request.in_progress = true;
          ota_download_and_apply();
          g_ota_request.in_progress = false;
        }

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

        if (now - last_env_publish_ms >= publish_env_period_ms)
        {
          last_env_publish_ms = now;

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
            hum_msg.data = local_data.humidity;

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

  sensor_data_mutex = xSemaphoreCreateMutex();
  if (sensor_data_mutex == NULL)
  {
    Serial.println("[system] create mutex failed");
    while (1)
    {
      delay(1000);
    }
  }

  if (!init_env_sensor())
  {
    Serial.println("[system] BME280 init failed, stop");
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
    Serial.println("[system] Ethernet init failed, stop");
    while (1)
    {
      delay(1000);
    }
  }

  init_water_sensor();

  xTaskCreate(env_sensor_task, "env_sensor_task", 4096, NULL, 1, NULL);
  xTaskCreate(micro_ros_task, "micro_ros_task", 16384, NULL, 5, NULL);
  xTaskCreate(water_sensor_sonar_task, "water_sensor_sonar_task", 2048, NULL, 3, NULL);

  Serial.println("[system] tasks created");
}

void loop()
{
}
