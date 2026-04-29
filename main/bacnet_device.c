#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include "lwip/inet.h"
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "esp_netif.h"

#define WIFI_SSID "Meterin Workshop"
#define WIFI_PASS "123456@#"

static const char *TAG = "wifi";

// ================= WIFI =================
void wifi_init_sta(void)
{
    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
        },
    };

    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    esp_wifi_start();

    ESP_LOGI(TAG, "Connecting to WiFi...");
    esp_wifi_connect();
}

// ================= UDP =================
void udp_test_task(void *pvParameters)
{
    char rx_buffer[128];
    char addr_str[128];

    struct sockaddr_in dest_addr;
    dest_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(47808); // BACnet port

    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);

    if (sock < 0) {
        printf("Socket create failed\n");
        vTaskDelete(NULL);
        return;
    }

    if (bind(sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr)) < 0) {
        printf("Socket bind failed\n");
        close(sock);
        vTaskDelete(NULL);
        return;
    }

    printf("UDP listening on port 47808...\n");

    while (1)
    {
        struct sockaddr_in source_addr;
        socklen_t socklen = sizeof(source_addr);

        int len = recvfrom(sock, rx_buffer, sizeof(rx_buffer) - 1, 0,
                           (struct sockaddr *)&source_addr, &socklen);

        if (len > 0)
        {
            rx_buffer[len] = 0;
            inet_ntoa_r(source_addr.sin_addr, addr_str, sizeof(addr_str));
            printf("Received from %s: %s\n", addr_str, rx_buffer);
        }
    }
}

// ================= MAIN =================
void app_main(void)

{
    nvs_flash_init();
    wifi_init_sta();

    printf("START DELAY...\n");
    vTaskDelay(5000 / portTICK_PERIOD_MS);

    printf("START UDP TASK...\n");
    xTaskCreate(udp_test_task, "udp_test", 4096, NULL, 5, NULL);

    while (1)
    {
        printf("Main loop running...\n");
        vTaskDelay(2000 / portTICK_PERIOD_MS);
    }
}   