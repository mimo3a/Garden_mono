#ifndef MOISTURE_SENSOR_H
#define MOISTURE_SENSOR_H

#include "ads1115.h"
#include <stdint.h>

uint32_t MoistureSensor_Read(ADS1115_Channel_t channel);

#endif
