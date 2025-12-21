#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_wifi.h"
#include "esp_now.h"
#include "esp_log.h"
#include "nvs_flash.h"


bool message_status;
char message_sent[] = "The message was ssent";
char message_not[] = "The message was not sent";

void messageFunction()
{
    if (message_not == 1)
    {
        printf("%s\n", message_sent);
    }
    else
    {
        print("%s\n", message_not);    
    }
}


static const char *TAG = "espnow_sender";

// --- REPLACE THESE WITH YOUR RECEIVER'S MAC ADDRESS ---
// Example: If MAC is 24:6F:28:1A:2B:3C, write it like this:
uint8_t receiver_mac[ESP_NOW_ETH_ALEN] = {0x24, 0x6F, 0x28, 0x1A, 0x2B, 0x3C};

// --- Callback: Checks if data left the chip successfully ---
void data_sent_cb(const uint8_t *mac_addr, esp_now_send_status_t status) {
    ESP_LOGI(TAG, "Delivery Status: %s", status == ESP_NOW_SEND_SUCCESS ? "SUCCESS" : "FAIL");
}


// --- Task: Sends data every 2 seconds ---
void sender_task(void *pvParameter) {
    int count = 0;
    char data_to_send[32];

    while (1) {
        // 1. Format the data (Just a string for now)
        snprintf(data_to_send, sizeof(data_to_send), "Hello #%d", count++);

        // 2. Send the data
        esp_err_t result = esp_now_send(receiver_mac, (uint8_t *)data_to_send, strlen(data_to_send));
        
        if (result == ESP_OK) {
            ESP_LOGI(TAG, "Sending: %s", data_to_send);
        } else {
            ESP_LOGE(TAG, "Send Error: %s", esp_err_to_name(result));
        }

        // Wait 2 seconds
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

void app_main(void) {
    // 1. Init NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // 2. Init WiFi (Station Mode)
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());

    // 3. Init ESP-NOW
    ESP_ERROR_CHECK(esp_now_init());
    ESP_ERROR_CHECK(esp_now_register_send_cb(data_sent_cb));

    // 4. Register the Peer (The Receiver)
    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, receiver_mac, 6);
    peerInfo.channel = 0;  
    peerInfo.encrypt = false;
    
    if (esp_now_add_peer(&peerInfo) != ESP_OK){
        ESP_LOGE(TAG, "Failed to add peer");
        return;
    }

    // 5. Start the Task
    xTaskCreate(sender_task, "sender_task", 4096, NULL, 4, NULL);
}