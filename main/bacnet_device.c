#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include "lwip/inet.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"

#define WIFI_SSID "Meterin Workshop"
#define WIFI_PASS "123456@#"

#define UDP_PORT 47808
#define DEVICE_INSTANCE 301

static const char *TAG = "BACNET";

static volatile bool wifi_connected = false;
static esp_ip4_addr_t got_ip;
static bool ip_valid = false;

// ================= WIFI EVENT =================
static void wifi_event_handler(void *arg,
                               esp_event_base_t event_base,
                               int32_t event_id,
                               void *event_data)
{
    if (event_base == WIFI_EVENT &&
        event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        wifi_connected = false;
        ip_valid = false;
        printf("WiFi disconnected... reconnecting\n");
        esp_wifi_connect();
    }

    if (event_base == IP_EVENT &&
        event_id == IP_EVENT_STA_GOT_IP)
    {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;

        wifi_connected = true;
        got_ip = event->ip_info.ip;
        ip_valid = true;

        ESP_LOGI(TAG, "WiFi connected!");
        ESP_LOGI(TAG, "IP: " IPSTR, IP2STR(&got_ip));
    }
}

// ================= WIFI INIT =================
void wifi_init_sta(void)
{
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_register(
        WIFI_EVENT,
        ESP_EVENT_ANY_ID,
        &wifi_event_handler,
        NULL,
        NULL);

    esp_event_handler_instance_register(
        IP_EVENT,
        IP_EVENT_STA_GOT_IP,
        &wifi_event_handler,
        NULL,
        NULL);

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

// ================= WIFI STATUS PRINT =================
void print_wifi_status()
{
    if (wifi_connected && ip_valid)
    {
        printf("WiFi STATUS: CONNECTED\n");
        printf("IP: %d.%d.%d.%d\n",
               IP2STR(&got_ip));
    }
    else
    {
        printf("WiFi STATUS: NOT CONNECTED\n");
    }
}

// ================= I-AM RESPONSE =================
void bacnet_send_i_am(struct sockaddr_in *dest_addr)
{
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (sock < 0) return;

    uint8_t packet[] = {
        0x81, 0x0A,

        0x00, 0x11,   // length

        0x01,         // NPDU
        0x00,         // no network layer msg

        0x10,         // I-AM

        // Device Instance (301)
        0x00, 0x01, 0x2D,

        // Max APDU length
        0x04, 0x00,

        // segmentation supported = none
        0x00,

        // vendor ID (random simple)
        0x00, 0x00
    };

    sendto(sock, packet, sizeof(packet), 0,
           (struct sockaddr *)dest_addr,
           sizeof(*dest_addr));

    printf("I-AM SENT OK\n");

    close(sock);
}

// ================= UDP TASK =================
void udp_task(void *pv)
{
    char rx[128];
    char ip_str[32];

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(UDP_PORT);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (sock < 0)
    {
        printf("Socket failed\n");
        vTaskDelete(NULL);
    }

    int opt = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        printf("Bind failed\n");
        close(sock);
        vTaskDelete(NULL);
    }

    printf("UDP listening on %d...\n", UDP_PORT);

    while (1)
    {
        struct sockaddr_in source;
        socklen_t len = sizeof(source);

        int r = recvfrom(sock, rx, sizeof(rx), 0,
                         (struct sockaddr *)&source, &len);

        if (r > 0)
        {
            inet_ntoa_r(source.sin_addr, ip_str, sizeof(ip_str));

            printf("Received from %s (HEX): ", ip_str);
            for (int i = 0; i < r; i++)
                printf("%02X ", (unsigned char)rx[i]);
            printf("\n");
            
            printf("Received from %s (ASCII): ", ip_str);
            for (int i = 0; i < r; i++)
            {
                unsigned char c = (unsigned char)rx[i];
                if (c >= 32 && c <= 126)
                    printf("%c", c);
                else
                    printf(".");
            }
            printf("\n");

            // WHO-IS detection (simple)
            if ((unsigned char)rx[3] == 0x00 || (unsigned char)rx[4] == 0x0C)
            {
                printf("WHO-IS detected -> sending I-AM\n");
                bacnet_send_i_am(&source);
            }
        }
        else
        {
            vTaskDelay(pdMS_TO_TICKS(100));
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

    struct sockaddr_in dest;
    dest.sin_family = AF_INET;
    dest.sin_port = htons(UDP_PORT);
    dest.sin_addr.s_addr = inet_addr("255.255.255.255");

    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (sock < 0)
    {
        printf("BACnet socket failed\n");
        return;
    }

    int b = 1;
    setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &b, sizeof(b));

    uint8_t who_is[] = {
        0x81, 0x0B,
        0x00, 0x0C,
        0x01,
        0x20,
        0xFF, 0xFF,
        0x00,
        0xFF,
        0x10, 0x08
    };

    sendto(sock, who_is, sizeof(who_is), 0,
           (struct sockaddr *)&dest,
           sizeof(dest));

    printf("BACnet Who-Is broadcast sent!\n");

    close(sock);
}

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