// 5Vピンから給電時にはSerial関連の処理をコメントアウトすること

#include <Arduino.h>
#include <driver/rtc_io.h>
#include <esp_wifi.h>
#include <esp_bt.h>
#include <esp_bt_main.h>
#include <time.h>
#include "lora.h"

#define UART_TX_PIN 17
#define UART_RX_PIN 16
#define WAKEUP_PIN GPIO_NUM_33
#define TRAP_PIN GPIO_NUM_26

const int lmid_own = 1;
const int lmid_parent = 0;
const int scount = 1;

struct Trap {
    const uint8_t sid = 1;
    const uint8_t type = 0xC1;
    uint8_t value;
} trap;

RTC_DATA_ATTR bool data_sent_flag = false;
RTC_DATA_ATTR time_t data_sent_flag_start = 0;

const time_t DATA_SENT_FLAG_HOLD_SEC = 30; // あとで変更

void setup() {
    init_controller();
    init_serial(Serial2, UART_TX_PIN, UART_RX_PIN);
    init_lora_module(lmid_own);

    wakeup_reason = esp_sleep_get_wakeup_cause();
    Serial.printf("Wakeup reason: %d\r\n", wakeup_reason);
}

void loop() {
}