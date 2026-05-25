/**
  ******************************************************************************
  * @file           : ds18b20.c
  * @brief          : DS18B20 temperature sensor driver implementation
  ******************************************************************************
  */

#include "ds18b20.h"
#include "main.h"

/**
 * @brief Initialize DWT cycle counter for microsecond delays
 */
void DS18B20_DWT_Init(void) {
	if (!(CoreDebug->DEMCR & CoreDebug_DEMCR_TRCENA_Msk)) {
		CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
		DWT->CYCCNT = 0;
		DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
	}
}

/**
 * @brief Microsecond delay function
 * @param us Delay time in microseconds
 */
void DS18B20_Delay_us(uint16_t us) {
	uint32_t start = DWT->CYCCNT;
	uint32_t ticks = us * (HAL_RCC_GetHCLKFreq() / 1000000);
	while ((DWT->CYCCNT - start) < ticks);
}

/**
 * @brief 1-Wire bus reset
 * @return 1 if device present, 0 if no device
 */
uint8_t DS18B20_Reset(void) {
	uint8_t presence;
	GPIO_InitTypeDef GPIO_InitStruct = {0};

	// Pull line low for 480 µs
	GPIO_InitStruct.Pin = DS_PIN;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
	HAL_GPIO_Init(DS_PORT, &GPIO_InitStruct);
	HAL_GPIO_WritePin(DS_PORT, DS_PIN, GPIO_PIN_RESET);
	DS18B20_Delay_us(480);

	// Release line and wait for presence pulse
	GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
	GPIO_InitStruct.Pull = GPIO_NOPULL; // External pull-up required (4.7k)
	HAL_GPIO_Init(DS_PORT, &GPIO_InitStruct);
	DS18B20_Delay_us(70);

	presence = !HAL_GPIO_ReadPin(DS_PORT, DS_PIN);
	DS18B20_Delay_us(410);

	return presence;
}

/**
 * @brief Write one bit to 1-Wire bus
 * @param bit Bit value (0 or 1)
 */
void DS18B20_WriteBit(uint8_t bit) {
	GPIO_InitTypeDef GPIO_InitStruct = {0};
	GPIO_InitStruct.Pin = DS_PIN;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
	HAL_GPIO_Init(DS_PORT, &GPIO_InitStruct);

	HAL_GPIO_WritePin(DS_PORT, DS_PIN, GPIO_PIN_RESET);

	if (bit) {
		DS18B20_Delay_us(6); // Write 1 start
		GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
		HAL_GPIO_Init(DS_PORT, &GPIO_InitStruct);
		DS18B20_Delay_us(64);
	} else {
		DS18B20_Delay_us(60); // Write 0 duration
		GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
		HAL_GPIO_Init(DS_PORT, &GPIO_InitStruct);
		DS18B20_Delay_us(10);
	}
}

/**
 * @brief Read one bit from 1-Wire bus
 * @return Bit value (0 or 1)
 */
uint8_t DS18B20_ReadBit(void) {
	uint8_t bit = 0;
	GPIO_InitTypeDef GPIO_InitStruct = {0};

	// Pull low
	GPIO_InitStruct.Pin = DS_PIN;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
	HAL_GPIO_Init(DS_PORT, &GPIO_InitStruct);

	HAL_GPIO_WritePin(DS_PORT, DS_PIN, GPIO_PIN_RESET);
	DS18B20_Delay_us(6);

	// Release
	GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
	HAL_GPIO_Init(DS_PORT, &GPIO_InitStruct);
	DS18B20_Delay_us(9);

	bit = HAL_GPIO_ReadPin(DS_PORT, DS_PIN);
	DS18B20_Delay_us(55);

	return bit;
}

/**
 * @brief Write one byte to 1-Wire bus
 * @param data Byte to write
 */
void DS18B20_WriteByte(uint8_t data) {
	for (int i = 0; i < 8; i++) {
		DS18B20_WriteBit(data & 0x01);
		data >>= 1;
	}
}

/**
 * @brief Read one byte from 1-Wire bus
 * @return Byte read from bus
 */
uint8_t DS18B20_ReadByte(void) {
	uint8_t value = 0;
	for (int i = 0; i < 8; i++) {
		if (DS18B20_ReadBit())
			value |= (1 << i);
	}
	return value;
}

/**
 * @brief Calculate CRC8 checksum
 * @param data Pointer to data array
 * @param len Length of data
 * @return CRC8 value
 */
uint8_t DS18B20_CRC8(uint8_t *data, uint8_t len) {
	uint8_t crc = 0;
	for (uint8_t i = 0; i < len; i++) {
		uint8_t inbyte = data[i];
		for (uint8_t j = 0; j < 8; j++) {
			uint8_t mix = (crc ^ inbyte) & 0x01;
			crc >>= 1;
			if (mix) 
				crc ^= 0x8C;
			inbyte >>= 1;
		}
	}
	return crc;
}

/**
 * @brief Read temperature from DS18B20 sensor
 * @param temperature Pointer to store temperature value
 * @return DS18B20_OK if success, error code otherwise
 */
uint8_t DS18B20_ReadTemperature(float *temperature) {
	uint8_t scratchpad[9];

	// Check device presence
	if (!DS18B20_Reset()) {
		return DS18B20_ERROR_NO_DEVICE;
	}

	// Start temperature conversion
	DS18B20_WriteByte(DS18B20_CMD_SKIP_ROM);
	DS18B20_WriteByte(DS18B20_CMD_CONVERT_T);

	// Wait for conversion (12-bit resolution = 750ms)
	HAL_Delay(750);

	// Read scratchpad
	if (!DS18B20_Reset()) {
		return DS18B20_ERROR_NO_DEVICE;
	}

	DS18B20_WriteByte(DS18B20_CMD_SKIP_ROM);
	DS18B20_WriteByte(DS18B20_CMD_READ_SCRATCHPAD);

	// Read all 9 bytes
	for (int i = 0; i < 9; i++) {
		scratchpad[i] = DS18B20_ReadByte();
	}

	// Verify CRC
	uint8_t crc = DS18B20_CRC8(scratchpad, 8);
	if (crc != scratchpad[8]) {
		return DS18B20_ERROR_CRC;
	}

	// Calculate temperature
	uint8_t temp_l = scratchpad[0];
	uint8_t temp_h = scratchpad[1];
	int16_t rawTemp = (temp_h << 8) | temp_l;

	// Check for invalid values
	if (rawTemp == 0x0550 || rawTemp == 0xFFFF || rawTemp == 0x0000) {
		return DS18B20_ERROR_INVALID_DATA;
	}

	*temperature = (float)rawTemp / 16.0f;

	return DS18B20_OK;
}

/**
 * @brief Read temperature from DS18B20 sensor (integer format)
 * @param temp_integer Pointer to store integer part
 * @param temp_fraction Pointer to store fractional part (0-99)
 * @return DS18B20_OK if success, error code otherwise
 */
uint8_t DS18B20_ReadTemperatureInt(int16_t *temp_integer, int16_t *temp_fraction) {
	uint8_t scratchpad[9];

	// Check device presence
	if (!DS18B20_Reset()) {
		return DS18B20_ERROR_NO_DEVICE;
	}

	// Start temperature conversion
	DS18B20_WriteByte(DS18B20_CMD_SKIP_ROM);
	DS18B20_WriteByte(DS18B20_CMD_CONVERT_T);

	// Wait for conversion (12-bit resolution = 750ms)
	HAL_Delay(750);

	// Read scratchpad
	if (!DS18B20_Reset()) {
		return DS18B20_ERROR_NO_DEVICE;
	}

	DS18B20_WriteByte(DS18B20_CMD_SKIP_ROM);
	DS18B20_WriteByte(DS18B20_CMD_READ_SCRATCHPAD);

	// Read all 9 bytes
	for (int i = 0; i < 9; i++) {
		scratchpad[i] = DS18B20_ReadByte();
	}

	// Verify CRC
	uint8_t crc = DS18B20_CRC8(scratchpad, 8);
	if (crc != scratchpad[8]) {
		return DS18B20_ERROR_CRC;
	}

	// Calculate temperature
	uint8_t temp_l = scratchpad[0];
	uint8_t temp_h = scratchpad[1];
	int16_t rawTemp = (temp_h << 8) | temp_l;

	// Check for invalid values
	if (rawTemp == 0x0550 || rawTemp == 0xFFFF || rawTemp == 0x0000) {
		return DS18B20_ERROR_INVALID_DATA;
	}

	// Calculate temperature without float in sprintf
	*temp_integer = rawTemp / 16;
	*temp_fraction = ((rawTemp & 0x0F) * 100) / 16;

	// Handle negative temperatures
	if (*temp_integer < 0) {
		*temp_fraction = -*temp_fraction;
	}

	return DS18B20_OK;
}
