#ifndef ADS1115_H
#define ADS1115_H

#include "main.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------------- I2C address ----------------
   ADDR to GND -> 0x48
   ADDR to VDD -> 0x49
   ADDR to SDA -> 0x4A
   ADDR to SCL -> 0x4B
*/
#define ADS1115_ADDR_GND   (0x48 << 1)
#define ADS1115_ADDR_VDD   (0x49 << 1)
#define ADS1115_ADDR_SDA   (0x4A << 1)
#define ADS1115_ADDR_SCL   (0x4B << 1)

/* Registers */
#define ADS1115_REG_CONVERSION   0x00
#define ADS1115_REG_CONFIG       0x01
#define ADS1115_REG_LO_THRESH    0x02
#define ADS1115_REG_HI_THRESH    0x03

/* Channels: single-ended */
typedef enum
{
    ADS1115_CHANNEL_0 = 0,
    ADS1115_CHANNEL_1,
    ADS1115_CHANNEL_2,
    ADS1115_CHANNEL_3
} ADS1115_Channel_t;

/* PGA / Full-scale range */
typedef enum
{
    ADS1115_PGA_6_144V = 0,   // ±6.144V
    ADS1115_PGA_4_096V,       // ±4.096V
    ADS1115_PGA_2_048V,       // ±2.048V
    ADS1115_PGA_1_024V,       // ±1.024V
    ADS1115_PGA_0_512V,       // ±0.512V
    ADS1115_PGA_0_256V        // ±0.256V
} ADS1115_PGA_t;

/* Data rate */
typedef enum
{
    ADS1115_DR_8SPS = 0,
    ADS1115_DR_16SPS,
    ADS1115_DR_32SPS,
    ADS1115_DR_64SPS,
    ADS1115_DR_128SPS,
    ADS1115_DR_250SPS,
    ADS1115_DR_475SPS,
    ADS1115_DR_860SPS
} ADS1115_DataRate_t;

typedef struct
{
    I2C_HandleTypeDef *hi2c;
    uint8_t address;
    ADS1115_PGA_t pga;
    ADS1115_DataRate_t dataRate;
} ADS1115_t;

/* Init */
HAL_StatusTypeDef ADS1115_Init(ADS1115_t *dev,
                               I2C_HandleTypeDef *hi2c,
                               uint8_t address,
                               ADS1115_PGA_t pga,
                               ADS1115_DataRate_t dataRate);

/* Check device */
HAL_StatusTypeDef ADS1115_IsReady(ADS1115_t *dev);

/* Raw register access */
HAL_StatusTypeDef ADS1115_WriteRegister(ADS1115_t *dev, uint8_t reg, uint16_t value);
HAL_StatusTypeDef ADS1115_ReadRegister(ADS1115_t *dev, uint8_t reg, uint16_t *value);

/* Single conversion */
HAL_StatusTypeDef ADS1115_StartSingleConversion(ADS1115_t *dev, ADS1115_Channel_t channel);
HAL_StatusTypeDef ADS1115_ReadRaw(ADS1115_t *dev, int16_t *raw);

/* High-level helpers */
HAL_StatusTypeDef ADS1115_ReadChannelRaw(ADS1115_t *dev, ADS1115_Channel_t channel, int16_t *raw);
HAL_StatusTypeDef ADS1115_ReadChannelVoltage(ADS1115_t *dev, ADS1115_Channel_t channel, float *voltage);

/* Utils */
float ADS1115_RawToVoltage(ADS1115_t *dev, int16_t raw);
uint32_t ADS1115_GetConversionDelayMs(ADS1115_t *dev);

#ifdef __cplusplus
}
#endif

#endif /* ADS1115_H */
