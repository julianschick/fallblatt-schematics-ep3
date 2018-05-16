#ifndef GLOBALS_H_INCLUDED
#define GLOBALS_H_INCLUDED

// Basis
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>

// FreeRTOS
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"

// Tasks
TaskHandle_t flap_task_h;
TaskHandle_t http_server_task_h;
TaskHandle_t http_client_task_h;

// Logging
#include "esp_log.h"

// ip4_addr_t
#include "tcpip_adapter.h"

// Log Tags
#define TAG_FLAP "flap"
#define TAG_WIFI "wifiU"
#define TAG_BT "bluetooth"
#define TAG_HTTP "http"
#define TAG_HTTP_CLIENT "httpclient"
#define TAG_NVS "nvs"

// Settings
#define NUMBER_OF_FLAPS 40

// Task synchronization
EventGroupHandle_t event_group;
static const int WIFI_CONNECTED_BIT = BIT0;
static const int HTTP_PULL_BIT = BIT1;
SemaphoreHandle_t ip_semaphore;

// HTTP pull settings
char http_pull_server[64];
char http_pull_address[64];

// Assigned IP
ip4_addr_t wifi_client_ip;
ip4_addr_t zero_ip;



#endif //GLOBALS_H_INCLUDED