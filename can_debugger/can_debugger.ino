#include "esp_twai.h"
#include "esp_twai_onchip.h"
#include "freertos/queue.h"

typedef struct {
  uint32_t id;
  uint8_t dlc;
  uint8_t data[8];
} CAN_message_t;

twai_node_handle_t node_hdl = NULL;
QueueHandle_t rx_queue;

IRAM_ATTR bool on_receive(twai_node_handle_t handle, const twai_rx_done_event_data_t* edata, void* user_ctx) {
  uint8_t recv_buff[8];
  twai_frame_t rx_frame = {
    .buffer = recv_buff,
    .buffer_len = sizeof(recv_buff),
  };

  const auto rx_result = twai_node_receive_from_isr(node_hdl, &rx_frame);
  if (rx_result != ESP_OK) {
    return false;
  }

  CAN_message_t msg;
  msg.id = rx_frame.header.id;
  msg.dlc = rx_frame.header.dlc;

  for (uint8_t i = 0; i < msg.dlc; i++) {
    msg.data[i] = rx_frame.buffer[i];
  }

  BaseType_t xHigherPriorityTaskWoken = pdFALSE;
  xQueueSendFromISR(rx_queue, &msg, &xHigherPriorityTaskWoken);

  return xHigherPriorityTaskWoken == pdTRUE;
}

void setup() {
  Serial.begin(115200);

  // 受信用の Queue
  rx_queue = xQueueCreate(20, sizeof(CAN_message_t));
  if (rx_queue == NULL) {
    Serial.println("Failed to create queue");
    return;
  }

  twai_onchip_node_config_t node_config = {};
  node_config.io_cfg.tx = GPIO_NUM_16;       // TWAI TX GPIO pin
  node_config.io_cfg.rx = GPIO_NUM_4;        // TWAI RX GPIO pin
  node_config.bit_timing.bitrate = 1000000;  // 1 mbps bitrate
  node_config.tx_queue_depth = 5;            // Transmit queue depth set to 5

  // Create a new TWAI controller driver instance
  ESP_ERROR_CHECK(twai_new_node_onchip(&node_config, &node_hdl));

  // receive イベントを登録
  twai_event_callbacks_t user_cbs = {};
  user_cbs.on_rx_done = &on_receive;

  if (twai_node_register_event_callbacks(node_hdl, &user_cbs, NULL) == ESP_OK) {
    Serial.println("Register receive event OK!");
  } else {
    Serial.println("Register receive event fail!");
    return;
  }

  // Start the TWAI controller
  ESP_ERROR_CHECK(twai_node_enable(node_hdl));
}

void printHexByte(uint8_t data) {
  Serial.print("0x");
  if (data < 0x10) {
    Serial.print("0");
  }
  Serial.print(data, HEX);
}

void loop() {
  // 受信
  CAN_message_t rx_msg;
  // キューにデータがあある場合のみ処理
  if (xQueueReceive(rx_queue, &rx_msg, 0) == pdTRUE) {
    Serial.print("Received: (id)0x");

    if (rx_msg.id < 0x10) Serial.print("00");
    else if (rx_msg.id < 0x100) Serial.print("0");

    Serial.print(rx_msg.id, HEX);

    for (uint8_t i = 0; i < rx_msg.dlc; i++) {
      Serial.print(", ");
      printHexByte(rx_msg.data[i]);
    }
    Serial.println();
  }

  // 送信 (id)0x7FF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
  static uint32_t last_tx_time = 0;
  if (millis() - last_tx_time > 1000) {  // 1秒ごとに送信
    last_tx_time = millis();

    uint8_t tx_buf[8] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

    twai_frame_t tx_msg = {};
    tx_msg.header.id = 0x7FF;   // Message ID
    tx_msg.header.ide = false;  // Use 29-bit extended ID format
    tx_msg.header.dlc = 8;
    tx_msg.buffer = tx_buf;
    tx_msg.buffer_len = sizeof(tx_buf);  // Length of data to transmit

    esp_err_t res = twai_node_transmit(node_hdl, &tx_msg, 0);

    if (res != ESP_OK) {
      Serial.print("TX Failed Error Code: 0x");
      Serial.println(res, HEX);
    }
  }
}
