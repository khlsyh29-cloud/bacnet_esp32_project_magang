#ifndef WIFI_H
#define WIFI_H

#include "esp_netif.h"

// WiFi functions
void wifi_init_sta(void);
void print_wifi_status(void);

// WiFi status getters
bool wifi_is_connected(void);
bool wifi_ip_valid(void);
esp_ip4_addr_t wifi_get_ip(void);

#endif // WIFI_H
