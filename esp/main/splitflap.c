// Basis
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// Logging
#include "esp_log.h"

// NVS
#include "nvs.h"
#include "nvs_flash.h"

// GPIO/ADC
#include "driver/gpio.h"
#include "driver/adc.h"

// BT
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_gap_bt_api.h"
#include "esp_bt_device.h"
#include "esp_spp_api.h"

// WIFI
#include "tcpip_adapter.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"
#include "freertos/event_groups.h"

// HTTP server
#include "lwip/err.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"
#include "lwip/api.h"


#define PIN_MOTOR 23
#define PIN_HOME_SENSOR 21
#define PIN_FLAP_SENSOR 19
#define CH_SENSOR_INPUT (adc1_channel_t) ADC_CHANNEL_0

#define NUMBER_OF_FLAPS 40

#define F_FALLING_THRESHOLD 500
#define F_RISING_THRESHOLD 1500

#define H_FALLING_THRESHOLD 500
#define H_RISING_THRESHOLD 1500

#define SPP_SERVER_NAME "Splitflap Serial Command Input"
#define BT_DEVICE_NAME "Splitflap"

int fsensor_state;
int fsensor_value;
int fsensor_ch_flag;
int hsensor_state;
int hsensor_value;
int hsensor_ch_flag;

#define FALLING -1
#define NOCHANGE 0
#define RISING 1

// BT
#define BT_BUFFER_LEN 256
char bt_buffer[BT_BUFFER_LEN];
int bt_buffer_end = 0;

// WIFI
static EventGroupHandle_t wifi_event_group;
const int CONNECTED_BIT = BIT0;

//Tasks
TaskHandle_t flap_task_h;
TaskHandle_t http_server_task_h;

// log tags
#define TAG_FLAP "flap"
#define TAG_WIFI "wifiU"
#define TAG_BT "bluetooth"
#define TAG_HTTP "http"

bool parse_buffer();

void setup_gpio() {
	gpio_config_t io_conf;
    io_conf.intr_type = GPIO_PIN_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = BIT(PIN_MOTOR) | BIT(PIN_FLAP_SENSOR) | BIT(PIN_HOME_SENSOR);
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 0;
    gpio_config(&io_conf);

    gpio_set_level(PIN_MOTOR, 1);
    gpio_set_level(PIN_HOME_SENSOR, 0);
    gpio_set_level(PIN_FLAP_SENSOR, 0);

   	adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(CH_SENSOR_INPUT, ADC_ATTEN_DB_11);
}

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

static esp_err_t event_handler(void *ctx, system_event_t *event)
{
    switch(event->event_id) {
    case SYSTEM_EVENT_STA_START:
        esp_wifi_connect();
        break;
    case SYSTEM_EVENT_STA_GOT_IP:
        xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);
        break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
        /* This is a workaround as ESP32 WiFi libs don't currently
           auto-reassociate. */
        esp_wifi_connect();
        xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);
        break;
    default:
        break;
    }
    return ESP_OK;
}

static void setup_wifi(void)
{
    tcpip_adapter_init();
    wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK( esp_event_loop_init(event_handler, NULL) );
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK( esp_wifi_init(&cfg) );
    ESP_ERROR_CHECK( esp_wifi_set_storage(WIFI_STORAGE_RAM) );
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = "WLAN-150040",
            .password = "9539638648597419",
        },
    };
    ESP_LOGI(TAG_WIFI, "Setting WiFi configuration SSID %s...", wifi_config.sta.ssid);
    ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK( esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config) );
    ESP_ERROR_CHECK( esp_wifi_start() );
}

inline void read_sensors_raw_values() {
	gpio_set_level(PIN_FLAP_SENSOR, 1);
	vTaskDelay(1);
    fsensor_value = adc1_get_raw(CH_SENSOR_INPUT);
    gpio_set_level(PIN_FLAP_SENSOR, 0);
    gpio_set_level(PIN_HOME_SENSOR, 1);
    vTaskDelay(1);
    hsensor_value = adc1_get_raw(CH_SENSOR_INPUT);
    gpio_set_level(PIN_HOME_SENSOR, 0);
}

inline void init_sensors() {
    read_sensors_raw_values();

    if (fsensor_value >= F_RISING_THRESHOLD) {
        fsensor_state = 1;
    } else {
        fsensor_state = 0;
    }

    if (hsensor_value >= H_RISING_THRESHOLD) {
        hsensor_state = 1;
    } else {
        hsensor_state = 0;
    }

    fsensor_ch_flag = NOCHANGE;
    hsensor_ch_flag = NOCHANGE;
}

inline void read_sensors() {
    read_sensors_raw_values();

    // detect rising and falling edges for sensors
    if (fsensor_state == 1 && fsensor_value <= F_FALLING_THRESHOLD) {
        fsensor_state = 0;
        fsensor_ch_flag = FALLING;
    } else if (fsensor_state == 0 && fsensor_value >= F_RISING_THRESHOLD) {
        fsensor_state = 1;
        fsensor_ch_flag = RISING;
    }

    if (hsensor_state == 1 && hsensor_value <= H_FALLING_THRESHOLD) {
        hsensor_state = 0;
        hsensor_ch_flag = FALLING;
    } else if (hsensor_state == 0 && hsensor_value >= H_RISING_THRESHOLD) {
        hsensor_state = 1;
        hsensor_ch_flag = RISING;
    }
    
}

void flap_task() {
    setup_gpio();
    init_sensors();

    int current_flap = 0;
    bool flap_ch_flag = false;
    bool current_flap_inaccurate = true;
    int flap_cmd = 0;
    bool motor_running = false;
    bool next_flap_is_home = false;
    uint32_t notificationValue;

    while (1) {

        if (xTaskNotifyWait(0, 0, &notificationValue, 0) == pdTRUE) {
            flap_cmd = notificationValue;            
            ESP_LOGI("flap", "Received notification with value %d", notificationValue);
        }

        read_sensors();

        if (hsensor_ch_flag == RISING) {
            ESP_LOGI(TAG_FLAP, "Home sensor level RISING");
        } else if (hsensor_ch_flag == FALLING) {
            ESP_LOGI(TAG_FLAP, "Home sensor level FALLING");
        }

        if (fsensor_ch_flag == RISING) {
            ESP_LOGI(TAG_FLAP, "Flap sensor level RISING");
        } else if (fsensor_ch_flag == FALLING) {
            ESP_LOGI(TAG_FLAP, "Flap sensor level FALLING");
        }

        if (fsensor_ch_flag == RISING) {
            current_flap = (current_flap + 1) % NUMBER_OF_FLAPS;
            flap_ch_flag = true;
            ESP_LOGI(TAG_FLAP, "current_flap = %d", current_flap);

            if (next_flap_is_home) {
                if (current_flap != 0) {
                    ESP_LOGI(TAG_FLAP, "Home disagree (should be 0 but is %d)", current_flap);
                } else {
                    ESP_LOGI(TAG_FLAP, "Home OK");
                }
                current_flap = 0;
                current_flap_inaccurate = false;
                next_flap_is_home = false;
            }
        }

        if (hsensor_ch_flag == RISING) {
            next_flap_is_home = true;
        }

        /*char ch = fgetc(stdin);
        if (ch < NUMBER_OF_FLAPS) {
            flap_cmd = ch;
            printf("flap_cmd = %d\n", flap_cmd);
        } else if (ch == 110) {
            flap_cmd++;
            printf("flap_cmd = %d\n", flap_cmd);    
        }*/

        hsensor_ch_flag = NOCHANGE;
        fsensor_ch_flag = NOCHANGE;

        if (flap_ch_flag) {
            if (current_flap == flap_cmd) {
                gpio_set_level(PIN_MOTOR, 1);
                motor_running = false;
                ESP_LOGI(TAG_FLAP, "Motor OFF");
            }
        }

        flap_ch_flag = false;

        if ((current_flap != flap_cmd || current_flap_inaccurate) && !motor_running) {
            gpio_set_level(PIN_MOTOR, 0);
            motor_running = true;
            ESP_LOGI(TAG_FLAP, "Motor ON");
        }
    }
}

const static char http_resp_hdr_good[] =
    "HTTP/1.1 200 OK\r\nContent-type: text/html\r\n\r\n";
const static char http_resp_hdr_bad[] =
    "HTTP/1.1 400 Bad Request\r\nContent-type: text/html\r\n\r\n";
const static char http_resp_reboot[] = 
    "<!DOCTYPE html><html><body>REBOOT</body></html>";
const static char http_resp_flap[] = 
    "<!DOCTYPE html><html><body>FLAP XX</body></html>";


static void http_server_netconn_serve(struct netconn *conn)
{
    struct netbuf *inbuf;
    char *buf;
    u16_t buflen;
    err_t err;

    /* Read the data from the port, blocking if nothing yet there.
    We assume the request (the part we care about) is in one netbuf */
    err = netconn_recv(conn, &inbuf);

    bool erroneous_request = true;
    bool reboot = false;

    if (err == ERR_OK) {
        netbuf_data(inbuf, (void**)&buf, &buflen);

        char instr[buflen+1];
        memcpy(instr, buf, buflen);
        instr[buflen] = 0x00;

        if (strncmp(instr, "GET /FLAP/", 10) == 0) {
            if (strlen(instr) > 10) {
                char* number_start = instr + 10;
                char* number_end;
                int flap = strtol(number_start, &number_end, 10);

                if (number_start != number_end && flap >= 0 && flap < NUMBER_OF_FLAPS) {
                    netconn_write(conn, http_resp_hdr_good, sizeof(http_resp_hdr_good)-1, NETCONN_NOCOPY);
                    netconn_write(conn, http_resp_flap, sizeof(http_resp_flap)-1, NETCONN_NOCOPY);

                    ESP_LOGI("cmd", "FLAP %d", flap);
                    xTaskNotify(flap_task_h, flap, eSetValueWithOverwrite); 
                    erroneous_request = false;                     
                }
            }

        } else if (strncmp(instr, "GET /REBOOT", 11) == 0) {
            netconn_write(conn, http_resp_hdr_good, sizeof(http_resp_hdr_good)-1, NETCONN_NOCOPY);
            netconn_write(conn, http_resp_reboot, sizeof(http_resp_reboot)-1, NETCONN_NOCOPY);

            ESP_LOGI("cmd", "REBOOT");
            reboot = true;
            erroneous_request = false;
        }

        if (erroneous_request) {
            netconn_write(conn, http_resp_hdr_bad, sizeof(http_resp_hdr_bad)-1, NETCONN_NOCOPY);
        }

        

        // strncpy(_mBuffer, buf, buflen);

        /* Is this an HTTP GET command? (only check the first 5 chars, since
        there are other formats for GET, and we're keeping it very simple )*/
        /*printf("buffer = %s \n", buf);

        if (buflen>=5 &&
            buf[0]=='G' &&
            buf[1]=='E' &&
            buf[2]=='T' &&
            buf[3]==' ' &&
            buf[4]=='/' ) {
              printf("buf[5] = %c\n", buf[5]);*/
          /* Send the HTML header
                 * subtract 1 from the size, since we dont send the \0 in the string
                 * NETCONN_NOCOPY: our data is const static, so no need to copy it
           */

           /* netconn_write(conn, http_html_hdr, sizeof(http_html_hdr)-1, NETCONN_NOCOPY);
            netconn_write(conn, http_index_hml, sizeof(http_index_hml)-1, NETCONN_NOCOPY);*/
        //}
    }
    /* Close the connection (server closes in HTTP) */
    netconn_close(conn);

    /* Delete the buffer (netconn_recv gives us ownership,
       so we have to make sure to deallocate the buffer) */
    netbuf_delete(inbuf);

    if (reboot) {
        esp_restart();
    }
}


static void http_server_task()
{
  struct netconn *conn, *newconn;
  err_t err;
  conn = netconn_new(NETCONN_TCP);
  netconn_bind(conn, NULL, 80);
  netconn_listen(conn);
  do {
     err = netconn_accept(conn, &newconn);
     if (err == ERR_OK) {
       http_server_netconn_serve(newconn);
       netconn_delete(newconn);
     }
   } while(err == ERR_OK);
   netconn_close(conn);
   netconn_delete(conn);
}



void app_main()
{
    // initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK( ret );

    setup_bluetooth();
    setup_wifi();


    xTaskCreate(&flap_task, "flap_task", 2048, NULL, configMAX_PRIORITIES - 1, &flap_task_h);
    xTaskCreate(&http_server_task, "http_server_task", 2048, NULL, 1, &http_server_task_h);
}




void handle_command(char* cmd, char* arg1, char* arg2) {
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

bool parse_buffer() {
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
