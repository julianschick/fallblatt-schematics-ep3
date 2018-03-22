#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include "nvs.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_gap_bt_api.h"
#include "esp_bt_device.h"
#include "esp_spp_api.h"
#include "driver/gpio.h"
#include "driver/adc.h"

#define PIN_MOTOR 23
#define PIN_HOME_SENSOR 21
#define PIN_FLAP_SENSOR 19
#define CH_SENSOR_INPUT (adc1_channel_t) ADC_CHANNEL_0

#define NUMBER_OF_FLAPS 40

#define F_FALLING_THRESHOLD 500
#define F_RISING_THRESHOLD 1500

#define H_FALLING_THRESHOLD 500
#define H_RISING_THRESHOLD 1500

#define SPP_TAG "BT INPUT"
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

int current_flap;
bool current_flap_inaccurate;
int flap_cmd;

#define BT_BUFFER_LEN 256
char bt_buffer[BT_BUFFER_LEN];
int bt_buffer_end = 0;

bool parse_buffer();

void setup() {
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

            ESP_LOGI(SPP_TAG, "ESP_SPP_INIT_EVT");

            esp_bt_dev_set_device_name(BT_DEVICE_NAME);
            esp_bt_gap_set_scan_mode(ESP_BT_SCAN_MODE_CONNECTABLE_DISCOVERABLE);
            esp_spp_start_srv(ESP_SPP_SEC_NONE,ESP_SPP_ROLE_SLAVE, 0, SPP_SERVER_NAME);
            break;

        case ESP_SPP_DATA_IND_EVT:

            ESP_LOGI(SPP_TAG, "ESP_SPP_DATA_IND_EVT len=%d handle=%d",
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
        ESP_LOGI(SPP_TAG, "ESP_SPP_DISCOVERY_COMP_EVT");
        break;
    case ESP_SPP_OPEN_EVT:
        ESP_LOGI(SPP_TAG, "ESP_SPP_OPEN_EVT");
        break;
    case ESP_SPP_CLOSE_EVT:
        ESP_LOGI(SPP_TAG, "ESP_SPP_CLOSE_EVT");
        break;
    case ESP_SPP_START_EVT:
        ESP_LOGI(SPP_TAG, "ESP_SPP_START_EVT");
        break;
    case ESP_SPP_CL_INIT_EVT:
        ESP_LOGI(SPP_TAG, "ESP_SPP_CL_INIT_EVT");
        break;

         case ESP_SPP_CONG_EVT:
        ESP_LOGI(SPP_TAG, "ESP_SPP_CONG_EVT");
        break;
    case ESP_SPP_WRITE_EVT:
        ESP_LOGI(SPP_TAG, "ESP_SPP_WRITE_EVT");
        break;
    case ESP_SPP_SRV_OPEN_EVT:
        ESP_LOGI(SPP_TAG, "ESP_SPP_SRV_OPEN_EVT");
        break;
        
        default:

            break;
    }

}

void setup_bluetooth() {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK( ret );


    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    if (esp_bt_controller_init(&bt_cfg) != ESP_OK) {
        ESP_LOGE(SPP_TAG, "%s initialize controller failed\n", __func__);
        return;
    }

    if (esp_bt_controller_enable(ESP_BT_MODE_CLASSIC_BT) != ESP_OK) {
        ESP_LOGE(SPP_TAG, "%s enable controller failed\n", __func__);
        return;
    }

    if (esp_bluedroid_init() != ESP_OK) {
        ESP_LOGE(SPP_TAG, "%s initialize bluedroid failed\n", __func__);
        return;
    }

    if (esp_bluedroid_enable() != ESP_OK) {
        ESP_LOGE(SPP_TAG, "%s enable bluedroid failed\n", __func__);
        return;
    }

    if (esp_spp_register_callback(bluetooth_callback) != ESP_OK) {
        ESP_LOGE(SPP_TAG, "%s spp register failed\n", __func__);
        return;
    }

    if (esp_spp_init(ESP_SPP_MODE_CB) != ESP_OK) {
        ESP_LOGE(SPP_TAG, "%s spp init failed\n", __func__);
        return;
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

void app_main()
{
    setup_bluetooth();
	setup();
    init_sensors();

	current_flap = 0;
	bool flap_ch_flag = false;
	current_flap_inaccurate = true;
	flap_cmd = 0;
	bool motor_running = false;
	bool next_flap_is_home = false;

	while (1) {
		read_sensors();

		if (hsensor_ch_flag == RISING) {
			printf("h rising\n");
		} else if (hsensor_ch_flag == FALLING) {
			printf("h falling\n");
		}

		if (fsensor_ch_flag == RISING) {
			printf("f rising\n");
		} else if (fsensor_ch_flag == FALLING) {
			printf("f falling\n");
		}

		if (fsensor_ch_flag == RISING) {
			current_flap = (current_flap + 1) % NUMBER_OF_FLAPS;
			flap_ch_flag = true;
			printf("current_flap = %d\n", current_flap);

			if (next_flap_is_home) {
				if (current_flap != 0) {
					printf("home disagree!\n");
				} else {
					printf("home agree!\n");
				}
				current_flap = 0;
				current_flap_inaccurate = false;
				next_flap_is_home = false;
			}
		}

		if (hsensor_ch_flag == RISING) {
			next_flap_is_home = true;
		}

		char ch = fgetc(stdin);
		if (ch < NUMBER_OF_FLAPS) {
			flap_cmd = ch;
			printf("flap_cmd = %d\n", flap_cmd);
		} else if (ch == 110) {
			flap_cmd++;
			printf("flap_cmd = %d\n", flap_cmd);	
		}

		hsensor_ch_flag = NOCHANGE;
		fsensor_ch_flag = NOCHANGE;

		if (flap_ch_flag) {
			if (current_flap == flap_cmd) {
				gpio_set_level(PIN_MOTOR, 1);
				motor_running = false;
				printf("motor off\n");
			}
		}

		flap_ch_flag = false;

		if ((current_flap != flap_cmd || current_flap_inaccurate) && !motor_running) {
			gpio_set_level(PIN_MOTOR, 0);
			motor_running = true;
			printf("motor on\n");
		}
	}

}




void handle_command(char* cmd, char* arg1, char* arg2) {
    if (strcmp(cmd, "FLAP") == 0) {
        char* ptr;
        int flap = strtol(arg1, &ptr, 10);

        if (ptr != arg1 && flap >= 0 && flap < NUMBER_OF_FLAPS) {
            printf("CMD: FLAP %d\n", flap);
            flap_cmd = flap;
        }
    }

    if (strcmp(cmd, "REBOOT") == 0) {
        printf("CMD: REBOOT");
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
