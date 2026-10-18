/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : freertos.c
  * Description        : Code for freertos applications
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "cmsis_os.h"
#include "ads1115.h"
#include <stdio.h>
#include <string.h>
#include <stdarg.h>   
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */


/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define CH_PER_ADS 4
#define ADS_COUNT 2
#define CH_TOTAL (CH_PER_ADS * ADS_COUNT)

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */
extern ADS1115_t ads[ADS_COUNT];
extern uint8_t ads_ok[ADS_COUNT];
extern osMutexId_t i2c1MutexHandle;
extern UART_HandleTypeDef huart1;

uint32_t g_ads_count  = 0;  /* сколько раз задача прошла цикл */
uint8_t  g_mutex_ok   = 0;  /* 1=мьютекс захватился, 0=таймаут */
/* USER CODE END Variables */

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */

/* USER CODE END FunctionPrototypes */

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */
typedef struct 
    {
         uint8_t dev;
         uint8_t status; 
         uint8_t ch; 
         int16_t raw;
    } ChReading;

static int appendf(char *buf, size_t size, int pos, const char *fmt, ...)
{
    if (pos < 0 || (size_t)pos >= size)
    {
        return -1;
    }

    va_list args;
    va_start(args, fmt);
    int n = vsnprintf(buf + pos, size - (size_t)pos, fmt, args);
    va_end(args);

    if (n < 0 || (size_t)n >= size - (size_t)pos)
    {
        return -1;   /* строка обрезана бы — считаем ошибкой */
    }
    return pos + n;
}


void SensorTask(void *argument)
{
    static char buf[512];
    
    ChReading channels[CH_TOTAL];
    for(;;)
    {
        g_ads_count++;
        if(osMutexAcquire(i2c1MutexHandle, pdMS_TO_TICKS(100)) != osOK )
        {
            g_mutex_ok = 0;
            osDelay(100);
            continue;
        }
        g_mutex_ok = 1;
        for(int dev =0; dev < ADS_COUNT; dev++)
        {
            for(int ch = 0; ch < CH_PER_ADS; ch++)
            {
                int idx = dev * CH_PER_ADS + ch;      /* 0..7 */
                int16_t raw = -1;
                HAL_StatusTypeDef status = HAL_ERROR;
                if (ads_ok[dev])
                {
                    status = ADS1115_ReadChannelRaw(&ads[dev], (ADS1115_Channel_t)ch, &raw);
                }
                channels[idx].dev    = (uint8_t)dev;
                channels[idx].ch     = (uint8_t)ch;
                channels[idx].status = (uint8_t)status;
                channels[idx].raw    = (status == HAL_OK) ? raw : -1;
            }
        }
        osMutexRelease(i2c1MutexHandle);

    /* Сборка JSON: {"channels":[{...},{...},...]} */
        int pos = 0;
        pos = appendf(buf, sizeof(buf), pos, "{\"channels\":[");

        for (int idx = 0; idx < CH_TOTAL; idx++)
        {
            pos = appendf(buf, sizeof(buf), pos,
                          "%s{\"dev\":%d,\"ch\":%d,\"status\":%d,\"raw\":%d}",
                          (idx > 0) ? "," : "",
                          channels[idx].dev,
                          channels[idx].ch,
                          channels[idx].status,
                          channels[idx].raw);
        }

        pos = appendf(buf, sizeof(buf), pos, "]}\r\n");

        if (pos > 0)
        {
            HAL_UART_Transmit(&huart1, (uint8_t *)buf, (uint16_t)pos, HAL_MAX_DELAY);
        }

        osDelay(1000);
    }
}


/* USER CODE END Application */

