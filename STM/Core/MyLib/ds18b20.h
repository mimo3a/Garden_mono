/**
  ******************************************************************************
  * @file           : ds18b20.h
  * @brief          : DS18B20 temperature sensor driver header
  ******************************************************************************
  */

#ifndef DS18B20_H
#define DS18B20_H

#include "main.h"
#include <stdint.h>

/* DS18B20 Configuration */
#define DS_PORT GPIOB
#define DS_PIN  GPIO_PIN_1

/* DS18B20 Commands */
#define DS18B20_CMD_SKIP_ROM        0xCC
#define DS18B20_CMD_CONVERT_T       0x44
#define DS18B20_CMD_READ_SCRATCHPAD 0xBE

/* DS18B20 Return Values */
#define DS18B20_OK                  0
#define DS18B20_ERROR_NO_DEVICE     1
#define DS18B20_ERROR_CRC           2
#define DS18B20_ERROR_INVALID_DATA  3

/* Function Prototypes */

/**
 * @brief Initialize DWT cycle counter for microsecond delays
 */
void DS18B20_DWT_Init(void);

/**
 * @brief Microsecond delay function
 * @param us Delay time in microseconds
 */
void DS18B20_Delay_us(uint16_t us);

/**
 * @brief 1-Wire bus reset
 * @return 1 if device present, 0 if no device
 */
uint8_t DS18B20_Reset(void);

/**
 * @brief Write one bit to 1-Wire bus
 * @param bit Bit value (0 or 1)
 */
void DS18B20_WriteBit(uint8_t bit);

/**
 * @brief Read one bit from 1-Wire bus
 * @return Bit value (0 or 1)
 */
uint8_t DS18B20_ReadBit(void);

/**
 * @brief Write one byte to 1-Wire bus
 * @param data Byte to write
 */
void DS18B20_WriteByte(uint8_t data);

/**
 * @brief Read one byte from 1-Wire bus
 * @return Byte read from bus
 */
uint8_t DS18B20_ReadByte(void);

/**
 * @brief Calculate CRC8 checksum
 * @param data Pointer to data array
 * @param len Length of data
 * @return CRC8 value
 */
uint8_t DS18B20_CRC8(uint8_t *data, uint8_t len);

/**
 * @brief Read temperature from DS18B20 sensor
 * @param temperature Pointer to store temperature value
 * @return DS18B20_OK if success, error code otherwise
 */
uint8_t DS18B20_ReadTemperature(float *temperature);

/**
 * @brief Read temperature from DS18B20 sensor (integer format)
 * @param temp_integer Pointer to store integer part
 * @param temp_fraction Pointer to store fractional part (0-99)
 * @return DS18B20_OK if success, error code otherwise
 */
uint8_t DS18B20_ReadTemperatureInt(int16_t *temp_integer, int16_t *temp_fraction);

#endif /* DS18B20_H */
