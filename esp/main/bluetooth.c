#include "bluetooth.h"

#include "nvsutil.h"
#include "esp_wifi.h"

static uint32_t client = 0;

static void bluetooth_callback(esp_spp_cb_event_t event, esp_spp_cb_param_t *param);
static bool parse_buffer();
static void handle_command(char* cmd, char* arg1, char* arg2);

static void bluetooth_callback(esp_spp_cb_event_t event, esp_spp_cb_param_t *param) {

    uint8_t* input;
    uint16_t len;

    switch (event) {

        case ESP_SPP_INIT_EVT:

            ESP_LOGI(TAG_BT, "ESP_SPP_INIT_EVT");

            esp_bt_dev_set_device_name(BT_DEVICE_NAME);
            esp_bt_gap_set_scan_mode(ESP_BT_SCAN_MODE_CONNECTABLE_DISCOVERABLE);
            esp_spp_start_srv(ESP_SPP_SEC_NONE,ESP_SPP_ROLE_SLAVE, 0, SPP_SERVER_NAME);
            break;

        case ESP_SPP_DATA_IND_EVT:

            ESP_LOGI(TAG_BT, "ESP_SPP_DATA_IND_EVT len=%d handle=%d",
                 param->data_ind.len, param->data_ind.handle);
            esp_log_buffer_hex("",param->data_ind.data,param->data_ind.len);

            input = param->data_ind.data;
            len = param->data_ind.len;

            if (BT_BUFFER_LEN - bt_buffer_end < len) {
                len = BT_BUFFER_LEN - bt_buffer_end;
            }

            if (len != 0) {
                memcpy(bt_buffer + bt_buffer_end, input, len);    
                bt_buffer_end += len;
            }

            while (parse_buffer());

            break;

        case ESP_SPP_DISCOVERY_COMP_EVT:
            ESP_LOGI(TAG_BT, "ESP_SPP_DISCOVERY_COMP_EVT");
            break;
        case ESP_SPP_OPEN_EVT:
            ESP_LOGI(TAG_BT, "ESP_SPP_OPEN_EVT");
            break;
        case ESP_SPP_CLOSE_EVT:
            client = 0;
            ESP_LOGI(TAG_BT, "ESP_SPP_CLOSE_EVT");
            break;
        case ESP_SPP_START_EVT:
            ESP_LOGI(TAG_BT, "ESP_SPP_START_EVT");
            break;
        case ESP_SPP_CL_INIT_EVT:
            ESP_LOGI(TAG_BT, "ESP_SPP_CL_INIT_EVT");
            break;
        case ESP_SPP_CONG_EVT:
            ESP_LOGI(TAG_BT, "ESP_SPP_CONG_EVT");
            break;
        case ESP_SPP_WRITE_EVT:
            ESP_LOGI(TAG_BT, "ESP_SPP_WRITE_EVT");
            break;
        case ESP_SPP_SRV_OPEN_EVT:
            client = param->open.handle;
            ESP_LOGI(TAG_BT, "ESP_SPP_SRV_OPEN_EVT");
                break;
        
        default:

            break;
    }

}

void setup_bluetooth() {

    bt_buffer_end = 0;
    
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    if (esp_bt_controller_init(&bt_cfg) != ESP_OK) {
        ESP_LOGE(TAG_BT, "%s initialize controller failed\n", __func__);
        return;
    }

    if (esp_bt_controller_enable(ESP_BT_MODE_CLASSIC_BT) != ESP_OK) {
        ESP_LOGE(TAG_BT, "%s enable controller failed\n", __func__);
        return;
    }

    if (esp_bluedroid_init() != ESP_OK) {
        ESP_LOGE(TAG_BT, "%s initialize bluedroid failed\n", __func__);
        return;
    }

    if (esp_bluedroid_enable() != ESP_OK) {
        ESP_LOGE(TAG_BT, "%s enable bluedroid failed\n", __func__);
        return;
    }

    if (esp_spp_register_callback(bluetooth_callback) != ESP_OK) {
        ESP_LOGE(TAG_BT, "%s spp register failed\n", __func__);
        return;
    }

    if (esp_spp_init(ESP_SPP_MODE_CB) != ESP_OK) {
        ESP_LOGE(TAG_BT, "%s spp init failed\n", __func__);
        return;
    }
}


static void handle_command(char* cmd, char* arg1, char* arg2) {

    char response[64];
    response[0] = 0;

    if (strcmp(cmd, "WHOAREYOU") == 0) {

        ESP_LOGI("cmd", "WHOAREYOU");
        strcpy(response, "SPLITFLAP\n");

    } else if (strcmp(cmd, "FLAP") == 0) {
        
        char* ptr;
        int flap = strtol(arg1, &ptr, 10);

        if (ptr != arg1 && flap >= 0 && flap < NUMBER_OF_FLAPS) {
            ESP_LOGI("cmd", "FLAP %d", flap);
            xEventGroupClearBits(event_group, HTTP_PULL_BIT);
            xTaskNotify(flap_task_h, flap, eSetValueWithOverwrite);
            store_http_pull_bit();

            sprintf(response, "FLAP %d\n", flap);
        } else {
            strcpy(response, "INVALID COMMAND\n");
        }

    } else if (strcmp(cmd, "REBOOT") == 0) {

        ESP_LOGI("cmd", "REBOOT");
        strcpy(response, "REBOOT\n");
        esp_restart();


    } else if (strcmp(cmd, "SETWIFI") == 0) {
        ESP_LOGI("cmd", "WIFI SSID=%s", arg1);
        ESP_LOGI("cmd", "WIFI Pass=%s", arg2);

        wifi_config_t wifi_config;
        strcpy((char*)wifi_config.sta.ssid, arg1);
        strcpy((char*)wifi_config.sta.password, arg2);
        wifi_config.sta.bssid_set = false;

        bool wifi_active = strlen(arg1) > 0;
        
        if (esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config) == ESP_OK) {

            if (wifi_active) {
                
                esp_wifi_start();

                if ((xEventGroupGetBits(event_group) & WIFI_CONNECTED_BIT) != 0) {
                    esp_wifi_disconnect();    
                } else {
                    esp_wifi_connect();
                }            

            } else {
                esp_wifi_stop();
            }

            wifi_config_t actual_wifi_config;
            esp_wifi_get_config(ESP_IF_WIFI_STA, &actual_wifi_config);

            sprintf(response, "WIFI %s %s\n", actual_wifi_config.sta.ssid, actual_wifi_config.sta.password);

        } else {
            strcpy(response, "COMMAND EXECUTION FAILED\n");
        }

        sprintf(response, "WIFI %s %s\n", arg1, arg2);

    } else if (strcmp(cmd, "GETWIFI") == 0) {

        wifi_config_t wifi_config;
        esp_wifi_get_config(ESP_IF_WIFI_STA, &wifi_config);

        sprintf(response, "WIFI %s %s\n", wifi_config.sta.ssid, wifi_config.sta.password);

    } else if (strcmp(cmd, "GETIP") == 0) {

        xSemaphoreTake(ip_semaphore, portMAX_DELAY);

        if (ip4_addr_cmp(&wifi_client_ip, &zero_ip)) {
            sprintf(response, "IP NONE\n");
        } else {
            sprintf(response, "IP %d.%d.%d.%d\n", IP2STR(&wifi_client_ip));
        }

        xSemaphoreGive(ip_semaphore);


    } else if (strcmp(cmd, "PULL") == 0) {

        ESP_LOGI("cmd", "PULL");
        xEventGroupSetBits(event_group, HTTP_PULL_BIT);
        store_http_pull_bit();

        strcpy(response, "PULL\n");

    } else if (strcmp(cmd, "GETPULLSTATUS") == 0) {

        EventBits_t uxBits = xEventGroupGetBits(event_group);
        bool pull_enabled = (uxBits & HTTP_PULL_BIT) != 0;
        bool wifi_connected = (uxBits & WIFI_CONNECTED_BIT) != 0;

        sprintf(response, "PULLSTATUS %d %d\n", pull_enabled ? 1 : 0, wifi_connected ? 1 : 0);                

    } else if (strcmp(cmd, "SETPULLSERVER") == 0) {
        
        ESP_LOGI("cmd", "CONFIGPULL Server=%s", arg1);
        ESP_LOGI("cmd", "CONFIGPULL Address=%s", arg2);

        strcpy(http_pull_server, arg1);
        strcpy(http_pull_address, arg2);

        store_http_pull_data();

        sprintf(response, "SERVER %s %s\n", http_pull_server, http_pull_address);

    } else if (strcmp(cmd, "GETPULLSERVER") == 0) {

        sprintf(response, "SERVER %s %s\n", http_pull_server, http_pull_address);

    } else {
        strcpy(response, "UNKNOWN COMMAND\n");
    }

    if (strlen(response) > 0 && client != 0) {
        esp_spp_write(client, strlen(response), (uint8_t*) response);
    }


}

static bool parse_buffer() {
    int first_newline = -1;

    for (int i = 0; i < bt_buffer_end; i++) {
        if (bt_buffer[i] == 0x0A) {
            first_newline = i;
            break;
        }
    }

    if (first_newline != -1) {
        bt_buffer[first_newline] = 0x00;

        char* str = (char*)calloc(first_newline + 1, sizeof(char));

        strcpy(str, bt_buffer);
        const char* delimiter = " ";

        char* ptr = strtok(str, delimiter);
        char cmd[256] = "";
        char arg1[256] = "";
        char arg2[256] = "";
        int i = 0;
        while(ptr != NULL) {
            
            if (i == 0) {
                strcpy(cmd, ptr);
            } else if (i == 1) {
                strcpy(arg1, ptr);
            } else if (i == 2) {
                strcpy(arg2, ptr);
            }
            ptr = strtok(NULL, delimiter);
            i++;
        }

        handle_command(cmd, arg1, arg2);
        free(str);

        if (first_newline != 255) {
            for (int i = first_newline + 1; i < 256; i++) {
                bt_buffer[i - (first_newline + 1)] = bt_buffer[i];
            }
            bt_buffer_end -= first_newline + 1;
        } else {
            bt_buffer_end = 0;
        }

        return true;
    } else {
        return false;
    }
}
