#include <stdio.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include "adc_bsp.h"

static adc_cali_handle_t cali_handle;
static adc_oneshot_unit_handle_t adc1_handle;
static bool adc_ready = false;


bool Adc_PortInit(void) {
    adc_cali_curve_fitting_config_t cali_config = {};
    cali_config.unit_id = ADC_UNIT_1;
    cali_config.atten = ADC_ATTEN_DB_12;
    cali_config.bitwidth = ADC_BITWIDTH_12;
	esp_err_t ret = adc_cali_create_scheme_curve_fitting(&cali_config, &cali_handle);
	if (ret != ESP_OK) {
		ESP_LOGW("ADC", "ADC calibration init failed: %s", esp_err_to_name(ret));
		return false;
	}

    adc_oneshot_unit_init_cfg_t init_config1 = {};
    init_config1.unit_id = ADC_UNIT_1;
	ret = adc_oneshot_new_unit(&init_config1, &adc1_handle);
	if (ret != ESP_OK) {
		ESP_LOGW("ADC", "ADC unit init failed: %s", esp_err_to_name(ret));
		adc_cali_delete_scheme_curve_fitting(cali_handle);
		cali_handle = NULL;
		return false;
	}
  	adc_oneshot_chan_cfg_t config = {};
    config.bitwidth = ADC_BITWIDTH_12;            
    config.atten = ADC_ATTEN_DB_12;
	ret = adc_oneshot_config_channel(adc1_handle, ADC_CHANNEL_3, &config);
	if (ret != ESP_OK) {
		ESP_LOGW("ADC", "ADC channel init failed: %s", esp_err_to_name(ret));
		adc_oneshot_del_unit(adc1_handle);
		adc1_handle = NULL;
		adc_cali_delete_scheme_curve_fitting(cali_handle);
		cali_handle = NULL;
		return false;
	}
	adc_ready = true;
	return true;
}

float Adc_GetBatteryVoltage(int *data) {
	int value = 0;
  	int tage = 0;
    float vol = 0;
  	esp_err_t err;
	if (!adc_ready) {
		if (data) {
			*data = 0;
		}
		return 0.0f;
	}
  	err = adc_oneshot_read(adc1_handle,ADC_CHANNEL_3,&value);
	if(adc_ready && err == ESP_OK) {
    	adc_cali_raw_to_voltage(cali_handle,value,&tage);
    	vol = 0.001 * tage * 3;
	}
	if(data) {
		*data = value;
	}
	return vol;
}

uint8_t Adc_GetBatteryLevel(void) {
	float vol = Adc_GetBatteryVoltage(NULL);
    if(vol < 3.0) {
        return 0;
    }
    if(vol > 4.12) {
        return 100;
    }
    float level = ((vol-3.0) / 1.12) * 100;
    return (uint8_t)level;
}
