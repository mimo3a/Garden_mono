#include "ads1115.h"

/* Internal helpers */

static uint16_t ADS1115_BuildConfig(ADS1115_t *dev, ADS1115_Channel_t channel)
{
    uint16_t config = 0;

    /* OS = 1 -> start single conversion */
    config |= (1 << 15);

    /* MUX: single-ended */
    switch (channel)
    {
        case ADS1115_CHANNEL_0:
            config |= (4 << 12);   // AIN0 vs GND
            break;
        case ADS1115_CHANNEL_1:
            config |= (5 << 12);   // AIN1 vs GND
            break;
        case ADS1115_CHANNEL_2:
            config |= (6 << 12);   // AIN2 vs GND
            break;
        case ADS1115_CHANNEL_3:
            config |= (7 << 12);   // AIN3 vs GND
            break;
        default:
            config |= (4 << 12);
            break;
    }

    /* PGA */
    switch (dev->pga)
    {
        case ADS1115_PGA_6_144V:
            config |= (0 << 9);
            break;
        case ADS1115_PGA_4_096V:
            config |= (1 << 9);
            break;
        case ADS1115_PGA_2_048V:
            config |= (2 << 9);
            break;
        case ADS1115_PGA_1_024V:
            config |= (3 << 9);
            break;
        case ADS1115_PGA_0_512V:
            config |= (4 << 9);
            break;
        case ADS1115_PGA_0_256V:
        default:
            config |= (5 << 9);
            break;
    }

    /* MODE = 1 -> single-shot mode */
    config |= (1 << 8);

    /* Data rate */
    switch (dev->dataRate)
    {
        case ADS1115_DR_8SPS:
            config |= (0 << 5);
            break;
        case ADS1115_DR_16SPS:
            config |= (1 << 5);
            break;
        case ADS1115_DR_32SPS:
            config |= (2 << 5);
            break;
        case ADS1115_DR_64SPS:
            config |= (3 << 5);
            break;
        case ADS1115_DR_128SPS:
            config |= (4 << 5);
            break;
        case ADS1115_DR_250SPS:
            config |= (5 << 5);
            break;
        case ADS1115_DR_475SPS:
            config |= (6 << 5);
            break;
        case ADS1115_DR_860SPS:
            config |= (7 << 5);
            break;
        default:
            config |= (4 << 5);    // 128 SPS
            break;
    }

    /* Comparator disabled */
    config |= (3 << 0);

    return config;
}

static float ADS1115_GetFSR(ADS1115_t *dev)
{
    switch (dev->pga)
    {
        case ADS1115_PGA_6_144V: return 6.144f;
        case ADS1115_PGA_4_096V: return 4.096f;
        case ADS1115_PGA_2_048V: return 2.048f;
        case ADS1115_PGA_1_024V: return 1.024f;
        case ADS1115_PGA_0_512V: return 0.512f;
        case ADS1115_PGA_0_256V:
        default:                 return 0.256f;
    }
}

HAL_StatusTypeDef ADS1115_Init(ADS1115_t *dev,
                               I2C_HandleTypeDef *hi2c,
                               uint8_t address,
                               ADS1115_PGA_t pga,
                               ADS1115_DataRate_t dataRate)
{
    if (dev == NULL || hi2c == NULL)
        return HAL_ERROR;

    dev->hi2c = hi2c;
    dev->address = address;
    dev->pga = pga;
    dev->dataRate = dataRate;

    return HAL_OK;
}

HAL_StatusTypeDef ADS1115_IsReady(ADS1115_t *dev)
{
    if (dev == NULL || dev->hi2c == NULL)
        return HAL_ERROR;

    return HAL_I2C_IsDeviceReady(dev->hi2c, dev->address, 3, 100);
}

HAL_StatusTypeDef ADS1115_WriteRegister(ADS1115_t *dev, uint8_t reg, uint16_t value)
{
    if (dev == NULL || dev->hi2c == NULL)
        return HAL_ERROR;

    uint8_t data[3];
    data[0] = reg;
    data[1] = (uint8_t)((value >> 8) & 0xFF);
    data[2] = (uint8_t)(value & 0xFF);

    return HAL_I2C_Master_Transmit(dev->hi2c, dev->address, data, 3, HAL_MAX_DELAY);
}

HAL_StatusTypeDef ADS1115_ReadRegister(ADS1115_t *dev, uint8_t reg, uint16_t *value)
{
    if (dev == NULL || dev->hi2c == NULL || value == NULL)
        return HAL_ERROR;

    HAL_StatusTypeDef status;
    uint8_t data[2];

    status = HAL_I2C_Master_Transmit(dev->hi2c, dev->address, &reg, 1, HAL_MAX_DELAY);
    if (status != HAL_OK)
        return status;

    status = HAL_I2C_Master_Receive(dev->hi2c, dev->address, data, 2, HAL_MAX_DELAY);
    if (status != HAL_OK)
        return status;

    *value = ((uint16_t)data[0] << 8) | data[1];
    return HAL_OK;
}

HAL_StatusTypeDef ADS1115_StartSingleConversion(ADS1115_t *dev, ADS1115_Channel_t channel)
{
    uint16_t config;

    if (dev == NULL)
        return HAL_ERROR;

    config = ADS1115_BuildConfig(dev, channel);
    return ADS1115_WriteRegister(dev, ADS1115_REG_CONFIG, config);
}

HAL_StatusTypeDef ADS1115_ReadRaw(ADS1115_t *dev, int16_t *raw)
{
    uint16_t reg;
    HAL_StatusTypeDef status;

    if (dev == NULL || raw == NULL)
        return HAL_ERROR;

    status = ADS1115_ReadRegister(dev, ADS1115_REG_CONVERSION, &reg);
    if (status != HAL_OK)
        return status;

    *raw = (int16_t)reg;
    return HAL_OK;
}

uint32_t ADS1115_GetConversionDelayMs(ADS1115_t *dev)
{
    if (dev == NULL)
        return 10;

    switch (dev->dataRate)
    {
        case ADS1115_DR_8SPS:   return 130;
        case ADS1115_DR_16SPS:  return 70;
        case ADS1115_DR_32SPS:  return 40;
        case ADS1115_DR_64SPS:  return 20;
        case ADS1115_DR_128SPS: return 10;
        case ADS1115_DR_250SPS: return 5;
        case ADS1115_DR_475SPS: return 3;
        case ADS1115_DR_860SPS: return 2;
        default:                return 10;
    }
}

HAL_StatusTypeDef ADS1115_ReadChannelRaw(ADS1115_t *dev, ADS1115_Channel_t channel, int16_t *raw)
{
    HAL_StatusTypeDef status;

    if (dev == NULL || raw == NULL)
        return HAL_ERROR;

    status = ADS1115_StartSingleConversion(dev, channel);
    if (status != HAL_OK)
        return status;

    HAL_Delay(ADS1115_GetConversionDelayMs(dev));

    return ADS1115_ReadRaw(dev, raw);
}

float ADS1115_RawToVoltage(ADS1115_t *dev, int16_t raw)
{
    float fsr;

    if (dev == NULL)
        return 0.0f;

    fsr = ADS1115_GetFSR(dev);

    return ((float)raw * fsr) / 32768.0f;
}

HAL_StatusTypeDef ADS1115_ReadChannelVoltage(ADS1115_t *dev, ADS1115_Channel_t channel, float *voltage)
{
    HAL_StatusTypeDef status;
    int16_t raw;

    if (dev == NULL || voltage == NULL)
        return HAL_ERROR;

    status = ADS1115_ReadChannelRaw(dev, channel, &raw);
    if (status != HAL_OK)
        return status;

    *voltage = ADS1115_RawToVoltage(dev, raw);
    return HAL_OK;
}
