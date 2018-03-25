#include "bluetooth.h"

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
    if (strcmp(cmd, "FLAP") == 0) {
        char* ptr;
        int flap = strtol(arg1, &ptr, 10);

        if (ptr != arg1 && flap >= 0 && flap < NUMBER_OF_FLAPS) {
            ESP_LOGI("cmd", "FLAP %d", flap);
            xTaskNotify(flap_task_h, flap, eSetValueWithOverwrite);
        }
    }

    if (strcmp(cmd, "REBOOT") == 0) {
        ESP_LOGI("cmd", "REBOOT");
        esp_restart();
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
        char* delimiter = " ";

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
