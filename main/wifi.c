#include <stdio.h>
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "lwip/inet.h"

#define WIFI_SSID "Mikhaila"
#define WIFI_PASS "muayyad2312994"

static const char *TAG = "WIFI";

static volatile bool wifi_connected = false;
static esp_ip4_addr_t got_ip;
static bool ip_valid = false;

// ================= WIFI EVENT HANDLER =================
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

// ================= WIFI GETTERS =================
bool wifi_is_connected(void)
{
    return wifi_connected;
}

bool wifi_ip_valid(void)
{
    return ip_valid;
}

esp_ip4_addr_t wifi_get_ip(void)
{
    return got_ip;
}
