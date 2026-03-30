#include <Arduino.h>
#include <micro_ros_arduino.h>
#include <stdio.h>
#include <rcl/rcl.h>
#include <rcl/error_handling.h>
#include <rclc/rclc.h>
#include <rclc/executor.h>
#include <std_msgs/msg/int32.h>
#include <esp_task_wdt.h>

#define LED_PIN 2
#define WDT_TIMEOUT 10
#define RECONNECT_TIMEOUT 5000

#define RCCHECK(fn) { rcl_ret_t temp_rc = fn; if((temp_rc != RCL_RET_OK)){return false;}}
#define RCSOFTCHECK(fn) { rcl_ret_t temp_rc = fn; if((temp_rc != RCL_RET_OK)){}}

rcl_publisher_t publisher;
std_msgs__msg__Int32 msg;
rclc_executor_t executor;
rclc_support_t support;
rcl_allocator_t allocator;
rcl_node_t node;
rcl_timer_t timer;

bool connected = false;
unsigned long last_attempt = 0;

void timer_callback(rcl_timer_t * timer, int64_t last_call_time) {
  RCLC_UNUSED(last_call_time);
  if (timer != NULL) {
    rcl_ret_t ret = rcl_publish(&publisher, &msg, NULL);
    if (ret == RCL_RET_OK) {
      msg.data++;
      digitalWrite(LED_PIN, !digitalRead(LED_PIN));
    }
  }
}

bool create_entities() {
  allocator = rcl_get_default_allocator();
  RCCHECK(rclc_support_init(&support, 0, NULL, &allocator));
  RCCHECK(rclc_node_init_default(&node, "esp32_node", "", &support));
  RCCHECK(rclc_publisher_init_default(
    &publisher, &node,
    ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, Int32),
    "esp32_publisher"));
  const unsigned int timer_timeout = 1000;
  RCCHECK(rclc_timer_init_default(&timer, &support, RCL_MS_TO_NS(timer_timeout), timer_callback));
  RCCHECK(rclc_executor_init(&executor, &support.context, 1, &allocator));
  RCCHECK(rclc_executor_add_timer(&executor, &timer));
  return true;
}

void destroy_entities() {
  rmw_context_t * rmw_context = rcl_context_get_rmw_context(&support.context);
  (void) rmw_uros_set_context_entity_destroy_session_timeout(rmw_context, 0);
  rcl_publisher_fini(&publisher, &node);
  rcl_timer_fini(&timer);
  rclc_executor_fini(&executor);
  rcl_node_fini(&node);
  rclc_support_fini(&support);
}

void setup() {
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  esp_task_wdt_init(WDT_TIMEOUT, true);
  esp_task_wdt_add(NULL);
  set_microros_transports();
  delay(2000);
  msg.data = 0;
}

void loop() {
  esp_task_wdt_reset();

  if (!connected) {
    if (millis() - last_attempt > RECONNECT_TIMEOUT) {
      last_attempt = millis();
      digitalWrite(LED_PIN, LOW);
      if (rmw_uros_ping_agent(100, 1) == RMW_RET_OK) {
        if (create_entities()) {
          connected = true;
          digitalWrite(LED_PIN, HIGH);
        } else {
          destroy_entities();
        }
      }
    }
  } else {
    if (rclc_executor_spin_some(&executor, RCL_MS_TO_NS(100)) != RCL_RET_OK) {
      destroy_entities();
      connected = false;
      digitalWrite(LED_PIN, LOW);
    }
  }
  delay(10);
}