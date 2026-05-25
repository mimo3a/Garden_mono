#include "moisture_sensor.h"

extern ADS1115_t ads;

uint32_t MoistureSensor_Read(ADS1115_Channel_t channel)
{
    int16_t raw = 0;

    if (ADS1115_ReadChannelRaw(&ads, channel, &raw) != HAL_OK)
    {
        return 0;
    }

    if (raw < 0)
    {
        raw = 0;
    }

    return (uint32_t)raw;
}
