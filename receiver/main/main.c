#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_wifi.h"
#include "esp_now.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_mac.h" 

#define ESPNOW_QUEUE_SIZE 10

static const char *TAG = "espnow_receiver";
static QueueHandle_t s_espnow_queue;

typedef struct {
    uint8_t mac_addr[ESP_NOW_ETH_ALEN];
    uint8_t *data;
    int data_len;
} espnow_event_t;

// --- Callback ---
void example_espnow_recv_cb(const esp_now_recv_info_t *esp_now_info, const uint8_t *data, int data_len) {
    espnow_event_t evt;
    if (esp_now_info->src_addr == NULL || data == NULL || data_len <= 0) return;

    memcpy(evt.mac_addr, esp_now_info->src_addr, ESP_NOW_ETH_ALEN);
    evt.data = malloc(data_len);
    if (evt.data == NULL) return;
    memcpy(evt.data, data, data_len);
    evt.data_len = data_len;

    if (xQueueSend(s_espnow_queue, &evt, 0) != pdTRUE) {
        free(evt.data); 
    }
}

// --- Task ---
static void example_espnow_task(void *pvParameter) {
    espnow_event_t evt;
    while (xQueueReceive(s_espnow_queue, &evt, portMAX_DELAY) == pdTRUE) {
        ESP_LOGI(TAG, "Data received from: " MACSTR, MAC2STR(evt.mac_addr));
        ESP_LOGI(TAG, "Message: %.*s", evt.data_len, (char*)evt.data);
        free(evt.data);
    }
}

// --- Main ---
void app_main(void) {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());

    // PRINT MAC
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    ESP_LOGI("MAC", "RECEIVER MAC: " MACSTR, MAC2STR(mac));

    s_espnow_queue = xQueueCreate(ESPNOW_QUEUE_SIZE, sizeof(espnow_event_t));
    xTaskCreate(example_espnow_task, "espnow_task", 4096, NULL, 4, NULL);

    ESP_ERROR_CHECK(esp_now_init());
    ESP_ERROR_CHECK(esp_now_register_recv_cb(example_espnow_recv_cb));
}