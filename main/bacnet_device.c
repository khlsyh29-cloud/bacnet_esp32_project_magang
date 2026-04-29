#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_event.h"

// Module headers
#include "wifi.h"
#include "bacnet.h"
#include "udp.h"

// ================= MAIN =================
void app_main()
{
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    wifi_init_sta();

    vTaskDelay(pdMS_TO_TICKS(3000));

    xTaskCreate(udp_task, "udp_task", 8192, NULL, 5, NULL);

    printf("START SYSTEM...\n");

    while (1)
    {
        print_wifi_status();

        printf("Main loop running...\n");

        bacnet_send_broadcast();

        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}