/**
 * @file mux_cd4067.h
 * @brief CD4067 16-Channel Analog Multiplexer Driver
 * @details Digunakan untuk membaca 14 sensor IR dengan 1 ADC channel
 */

#ifndef MUX_CD4067_H
#define MUX_CD4067_H

#include "stm32f1xx_hal.h"
#include <stdint.h>
#include <stdbool.h>

/* Konfigurasi MUX */
#define MUX_CHANNELS 16
#define MUX_ACTIVE_CHANNELS 14  // Hanya 14 yang dipakai

/* Struktur Konfigurasi MUX */
typedef struct {
    GPIO_TypeDef *s0_port;
    uint16_t s0_pin;
    GPIO_TypeDef *s1_port;
    uint16_t s1_pin;
    GPIO_TypeDef *s2_port;
    uint16_t s2_pin;
    GPIO_TypeDef *s3_port;
    uint16_t s3_pin;
    GPIO_TypeDef *en_port;
    uint16_t en_pin;
    ADC_HandleTypeDef *hadc;
    uint32_t adc_channel;
} MUX_Config_t;

/* Struktur MUX Handle */
typedef struct {
    MUX_Config_t config;
    uint8_t current_channel;
    bool is_enabled;
    uint16_t settling_time_us;  // Waktu settling setelah switch channel
} MUX_Handle_t;

/* Fungsi Inisialisasi */
HAL_StatusTypeDef MUX_Init(MUX_Handle_t *hmux, MUX_Config_t *config);
void MUX_SetSettlingTime(MUX_Handle_t *hmux, uint16_t time_us);

/* Fungsi Kontrol */
void MUX_Enable(MUX_Handle_t *hmux);
void MUX_Disable(MUX_Handle_t *hmux);
void MUX_SelectChannel(MUX_Handle_t *hmux, uint8_t channel);

/* Fungsi Pembacaan */
uint16_t MUX_ReadChannel(MUX_Handle_t *hmux, uint8_t channel);
HAL_StatusTypeDef MUX_ReadAllChannels(MUX_Handle_t *hmux, uint16_t *data, uint8_t num_channels);

/* Fungsi Utilitas */
uint8_t MUX_GetCurrentChannel(MUX_Handle_t *hmux);

#endif /* MUX_CD4067_H */

/**
 * @file mux_cd4067.c
 * @brief Implementasi CD4067 MUX Driver
 */

// #include "mux_cd4067.h"

/* Delay mikro detik (approximate) */
static void delay_us(uint16_t us) {
    uint32_t cycles = (SystemCoreClock / 1000000) * us / 3;
    while(cycles--) {
        __NOP();
    }
}

/**
 * @brief Inisialisasi MUX CD4067
 */
HAL_StatusTypeDef MUX_Init(MUX_Handle_t *hmux, MUX_Config_t *config) {
    if(hmux == NULL || config == NULL) {
        return HAL_ERROR;
    }
    
    // Copy konfigurasi
    hmux->config = *config;
    hmux->current_channel = 0;
    hmux->is_enabled = false;
    hmux->settling_time_us = 10; // Default 10us
    
    // Set semua pin select ke LOW
    HAL_GPIO_WritePin(hmux->config.s0_port, hmux->config.s0_pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(hmux->config.s1_port, hmux->config.s1_pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(hmux->config.s2_port, hmux->config.s2_pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(hmux->config.s3_port, hmux->config.s3_pin, GPIO_PIN_RESET);
    
    // Disable MUX initially (EN = HIGH)
    HAL_GPIO_WritePin(hmux->config.en_port, hmux->config.en_pin, GPIO_PIN_SET);
    
    return HAL_OK;
}

/**
 * @brief Set settling time setelah switch channel
 */
void MUX_SetSettlingTime(MUX_Handle_t *hmux, uint16_t time_us) {
    hmux->settling_time_us = time_us;
}

/**
 * @brief Enable MUX (EN = LOW)
 */
void MUX_Enable(MUX_Handle_t *hmux) {
    HAL_GPIO_WritePin(hmux->config.en_port, hmux->config.en_pin, GPIO_PIN_RESET);
    hmux->is_enabled = true;
}

/**
 * @brief Disable MUX (EN = HIGH)
 */
void MUX_Disable(MUX_Handle_t *hmux) {
    HAL_GPIO_WritePin(hmux->config.en_port, hmux->config.en_pin, GPIO_PIN_SET);
    hmux->is_enabled = false;
}

/**
 * @brief Select channel (0-15)
 */
void MUX_SelectChannel(MUX_Handle_t *hmux, uint8_t channel) {
    if(channel >= MUX_CHANNELS) {
        return;
    }
    
    // Set S0-S3 sesuai binary channel
    HAL_GPIO_WritePin(hmux->config.s0_port, hmux->config.s0_pin, 
                      (channel & 0x01) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(hmux->config.s1_port, hmux->config.s1_pin,
                      (channel & 0x02) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(hmux->config.s2_port, hmux->config.s2_pin,
                      (channel & 0x04) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(hmux->config.s3_port, hmux->config.s3_pin,
                      (channel & 0x08) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    
    hmux->current_channel = channel;
    
    // Settling time delay
    delay_us(hmux->settling_time_us);
}

/**
 * @brief Baca satu channel
 */
uint16_t MUX_ReadChannel(MUX_Handle_t *hmux, uint8_t channel) {
    if(!hmux->is_enabled) {
        MUX_Enable(hmux);
    }
    
    // Select channel
    MUX_SelectChannel(hmux, channel);
    
    // Baca ADC
    HAL_ADC_Start(hmux->config.hadc);
    HAL_ADC_PollForConversion(hmux->config.hadc, HAL_MAX_DELAY);
    uint16_t value = HAL_ADC_GetValue(hmux->config.hadc);
    HAL_ADC_Stop(hmux->config.hadc);
    
    return value;
}

/**
 * @brief Baca semua channel sekaligus
 */
HAL_StatusTypeDef MUX_ReadAllChannels(MUX_Handle_t *hmux, uint16_t *data, uint8_t num_channels) {
    if(data == NULL || num_channels > MUX_CHANNELS) {
        return HAL_ERROR;
    }
    
    if(!hmux->is_enabled) {
        MUX_Enable(hmux);
    }
    
    for(uint8_t i = 0; i < num_channels; i++) {
        data[i] = MUX_ReadChannel(hmux, i);
    }
    
    return HAL_OK;
}

/**
 * @brief Get current channel
 */
uint8_t MUX_GetCurrentChannel(MUX_Handle_t *hmux) {
    return hmux->current_channel;
}