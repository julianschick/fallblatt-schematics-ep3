#include "globals.h"

// NVS
#include "nvs.h"
#include "nvs_flash.h"

// GPIO/ADC
#include "driver/gpio.h"
#include "driver/adc.h"

// WIFI
#include "tcpip_adapter.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"

#include "bluetooth.h"
#include "httpserver.h"
#include "httpclient.h"
#include "nvsutil.h"

#define PIN_MOTOR 23
#define PIN_HOME_SENSOR 19
#define PIN_FLAP_SENSOR 21
#define CH_SENSOR_INPUT (adc1_channel_t) ADC_CHANNEL_0


#define F_FALLING_THRESHOLD 500
#define F_RISING_THRESHOLD 1500

#define H_FALLING_THRESHOLD 500
#define H_RISING_THRESHOLD 1500


int fsensor_state;
int fsensor_value;
int fsensor_ch_flag;
int hsensor_state;
int hsensor_value;
int hsensor_ch_flag;

#define FALLING -1
#define NOCHANGE 0
#define RISING 1


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

static esp_err_t event_handler(void *ctx, system_event_t *event)
{
    switch(event->event_id) {
        
        case SYSTEM_EVENT_STA_START:
            esp_wifi_connect();
            break;

        case SYSTEM_EVENT_STA_GOT_IP:

            xSemaphoreTake(ip_semaphore, portMAX_DELAY);
            wifi_client_ip = event->event_info.got_ip.ip_info.ip;
            xSemaphoreGive(ip_semaphore);
            xEventGroupSetBits(event_group, WIFI_CONNECTED_BIT);
            break;

        case SYSTEM_EVENT_STA_DISCONNECTED:

            xEventGroupClearBits(event_group, WIFI_CONNECTED_BIT);
            xSemaphoreTake(ip_semaphore, portMAX_DELAY);
            ip4_addr_set_zero(&wifi_client_ip);
            xSemaphoreGive(ip_semaphore);

            esp_wifi_connect();
            break;

        default:
            break;
    }

    return ESP_OK;
}

static void setup_wifi(void)
{
    tcpip_adapter_init();
    
    ESP_ERROR_CHECK( esp_event_loop_init(event_handler, NULL) );
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK( esp_wifi_init(&cfg) );
    //ESP_ERROR_CHECK( esp_wifi_set_storage(WIFI_STORAGE_RAM) );
    //ESP_LOGI(TAG_WIFI, "Setting WiFi configuration SSID %s...", wifi_config.sta.ssid);
    ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_STA) );
    //ESP_ERROR_CHECK( esp_wifi_set_config(WIFI_IF_STA, &sta_config) );

    wifi_config_t actual_wifi_config;
    esp_wifi_get_config(ESP_IF_WIFI_STA, &actual_wifi_config);

    if (strlen((char*)actual_wifi_config.sta.ssid) > 0) {
        esp_wifi_start();
    }

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
        }

        read_sensors();

        if (hsensor_ch_flag == RISING) {
            //ESP_LOGI(TAG_FLAP, "Home sensor level RISING");
        } else if (hsensor_ch_flag == FALLING) {
            //ESP_LOGI(TAG_FLAP, "Home sensor level FALLING");
        }

        if (fsensor_ch_flag == RISING) {
            //ESP_LOGI(TAG_FLAP, "Flap sensor level RISING");
        } else if (fsensor_ch_flag == FALLING) {
            //ESP_LOGI(TAG_FLAP, "Flap sensor level FALLING");
        }

        if (fsensor_ch_flag == RISING) {
            current_flap = (current_flap + 1) % NUMBER_OF_FLAPS;
            flap_ch_flag = true;
            //ESP_LOGI(TAG_FLAP, "current_flap = %d", current_flap);

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

        hsensor_ch_flag = NOCHANGE;
        fsensor_ch_flag = NOCHANGE;

        if (flap_ch_flag) {
            if (current_flap == flap_cmd && !current_flap_inaccurate) {
                gpio_set_level(PIN_MOTOR, 1);
                motor_running = false;
                ESP_LOGI(TAG_FLAP, "Motor OFF");
            }
        }

        flap_ch_flag = false;

        if (current_flap != flap_cmd && !motor_running) {
            gpio_set_level(PIN_MOTOR, 0);
            motor_running = true;
            ESP_LOGI(TAG_FLAP, "Motor ON");
        }
    }
}


void app_main()
{
    setup_nvs();
    
    event_group = xEventGroupCreate();
    ip_semaphore = xSemaphoreCreateBinary();
    xSemaphoreGive(ip_semaphore);
    ip4_addr_set_zero(&wifi_client_ip);
    ip4_addr_set_zero(&zero_ip);

    restore_http_pull_data();
    restore_http_pull_bit();

    setup_bluetooth();
    setup_wifi();

    xTaskCreate(&flap_task, "flap_task", 2048, NULL, configMAX_PRIORITIES - 1, &flap_task_h);
    xTaskCreate(&http_server_task, "http_server_task", 4096, NULL, 1, &http_server_task_h);
    xTaskCreate(&http_client_task, "http_client_task", 4096, NULL, 1, &http_client_task_h);
}