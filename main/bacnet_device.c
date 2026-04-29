#include "unistd.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include "lwip/inet.h"
#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"

#define WIFI_SSID "Meterin Workshop"
#define WIFI_PASS "123456@#"

static const char *TAG = "wifi";
static volatile bool wifi_connected = false;

static esp_event_handler_instance_t wifi_event_instance;
static esp_event_handler_instance_t ip_event_instance;

// ================= EVENT HANDLER =================
static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        wifi_connected = false;
        printf("WiFi disconnected... reconnecting\n");
        esp_wifi_connect();
    }

    if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        wifi_connected = true;
        printf("WiFi connected!\n");
    }
}

// ================= WIFI INIT =================
void wifi_init_sta(void)
{
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(
        esp_event_handler_instance_register(
            WIFI_EVENT,
            ESP_EVENT_ANY_ID,
            &wifi_event_handler,
            NULL,
            &wifi_event_instance));

    ESP_ERROR_CHECK(
        esp_event_handler_instance_register(
            IP_EVENT,
            IP_EVENT_STA_GOT_IP,
            &wifi_event_handler,
            NULL,
            &ip_event_instance));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_connect());

    esp_wifi_set_ps(WIFI_PS_NONE);

    ESP_LOGI(TAG, "Connecting to WiFi...");
}

// ================= UDP LISTENER =================
void udp_test_task(void *pvParameters)
{
    char rx_buffer[128];
    char addr_str[128];

    struct sockaddr_in addr;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_family = AF_INET;
    addr.sin_port = htons(47808);

    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (sock < 0)
    {
        printf("Socket create failed\n");
        vTaskDelete(NULL);
        return;
    }

    int opt = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        printf("Socket bind failed\n");
        close(sock);
        vTaskDelete(NULL);
        return;
    }

    printf("UDP listening on port 47808...\n");

    
    while (!wifi_connected)
    {
        
    printf("Waiting WiFi...\n");
    vTaskDelay(pdMS_TO_TICKS(500));
}
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

// ================= BACNET BROADCAST =================
void bacnet_send_broadcast()
{
    if (!wifi_connected)
    {
        printf("WiFi not ready, skip BACnet send\n");
        return;
    }

    struct sockaddr_in dest_addr;
    dest_addr.sin_addr.s_addr = inet_addr("255.255.255.255");
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(47808);

    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (sock < 0)
    {
        printf("BACnet socket failed\n");
        return;
    }

    int broadcastEnable = 1;
    setsockopt(sock, SOL_SOCKET, SO_BROADCAST,
               &broadcastEnable, sizeof(broadcastEnable));

    uint8_t bacnet_packet[] = {
        0x81, 0x0A,
        0x00, 0x0C,
        0x01,
        0x20,
        0xFF, 0xFF,
        0x00,
        0xFF,
        0x10,
        0x08
    };

    sendto(sock, bacnet_packet, sizeof(bacnet_packet), 0,
           (struct sockaddr *)&dest_addr, sizeof(dest_addr));

    printf("BACnet Who-Is broadcast sent!\n");

    close(sock);
}

// ================= MAIN =================
void app_main(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    wifi_init_sta();

    vTaskDelay(pdMS_TO_TICKS(5000));

    xTaskCreate(udp_test_task, "udp_task", 4096, NULL, 5, NULL);

    printf("START SYSTEM...\n");

    while (1)
    {
        printf("Main loop running...\n");
        bacnet_send_broadcast();
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}