#include "nvsutil.h"

void setup_nvs() {
	esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK( ret );
}

void restore_http_pull_data() {
	
	nvs_handle nvs_h;
    if (ESP_OK == nvs_open("http_pull", NVS_READONLY, &nvs_h)) {
        
        size_t server_len = sizeof(http_pull_server);
        size_t address_len = sizeof(http_pull_address);

        if (ESP_OK != nvs_get_str(nvs_h, "server", http_pull_server, &server_len)) {
        	ESP_LOGE(TAG_NVS, "http_pull_server could not be restored. defaulted to empty string.");
            strcpy(http_pull_server, "");
        } else {
        	ESP_LOGI(TAG_NVS, "restored http_pull_server = %s", http_pull_server);
        }

        if (ESP_OK != nvs_get_str(nvs_h, "address", http_pull_address, &address_len)) {
        	ESP_LOGE(TAG_NVS, "http_pull_address could not be restored. defaulted to empty string.");
            strcpy(http_pull_address, "");    
        } else {
        	ESP_LOGI(TAG_NVS, "restored http_pull_address = %s", http_pull_address);
        }

    } else {
    	ESP_LOGE(TAG_NVS, "http pull data could not be restored: error loading nvs page.");
    }

}

void store_http_pull_data() {
	nvs_handle nvs_h;
    if (ESP_OK == nvs_open("http_pull", NVS_READWRITE, &nvs_h)) {
        ESP_ERROR_CHECK(nvs_set_str(nvs_h, "server", http_pull_server));
        ESP_ERROR_CHECK(nvs_set_str(nvs_h, "address", http_pull_address));
        ESP_ERROR_CHECK(nvs_commit(nvs_h));
    } else {
    	ESP_LOGE(TAG_NVS, "http pull data could not be stored: error loading nvs page.");
    }
}


void restore_http_pull_bit() {
	
	nvs_handle nvs_h;
    if (ESP_OK == nvs_open("http_pull", NVS_READONLY, &nvs_h)) {

    	uint8_t bit;
    	if (nvs_get_u8(nvs_h, "bit", &bit) == ESP_OK) {
    		ESP_LOGI(TAG_NVS, "restored http pull bit = %d", bit);

    		if (bit == 1) {
    			xEventGroupSetBits(event_group, HTTP_PULL_BIT);
    		}
    	} else {
    		ESP_LOGE(TAG_NVS, "http pull bit could not be restored. defaulted to 0.");
    	}
    } else {
    	ESP_LOGE(TAG_NVS, "http pull bit could not be restored: error loading nvs page.");
    }
}


void store_http_pull_bit() {
	EventBits_t uxBits = xEventGroupGetBits(event_group);

	uint8_t bit = 0;
    if ((uxBits & HTTP_PULL_BIT) != 0) {
    	bit = 1;
    }

    nvs_handle nvs_h;
    if (ESP_OK == nvs_open("http_pull", NVS_READWRITE, &nvs_h)) {
        ESP_ERROR_CHECK(nvs_set_u8(nvs_h, "bit", bit));
        ESP_ERROR_CHECK(nvs_commit(nvs_h));
    } else {
    	ESP_LOGE(TAG_NVS, "http pull bit could not be stored: error loading nvs page.");
    }
}