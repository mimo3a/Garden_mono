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
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */
extern ADS1115_t ads;
extern osMutexId_t i2c1MutexHandle;
extern UART_HandleTypeDef huart1;

int16_t  g_ads_raw    = 0;
uint32_t g_ads_count  = 0;  /* сколько раз задача прошла цикл */
uint8_t  g_ads_status = 0;  /* 0=HAL_OK, иначе код ошибки HAL */
uint8_t  g_mutex_ok   = 0;  /* 1=мьютекс захватился, 0=таймаут */
/* USER CODE END Variables */

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */

/* USER CODE END FunctionPrototypes */

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

void SensorTask(void *argument)
{
    char buf[256];
    typedef struct 
    {
         uint8_t status; 
         uint8_t ch; 
         int16_t raw;
    } ChReading;

    for (;;)
    {
        g_ads_count++;

        if (osMutexAcquire(i2c1MutexHandle, pdMS_TO_TICKS(100)) == osOK)
        {
            g_mutex_ok = 1;
            ChReading raw[4];

            for (int ch = 0; ch < 4; ch++) {
                HAL_StatusTypeDef status = ADS1115_ReadChannelRaw(&ads, (ADS1115_Channel_t)ch, &g_ads_raw);
                

                g_ads_status = (uint8_t)status;

                if (status == HAL_OK)
                    {
                        raw[ch].status = g_ads_status;
                        raw[ch].ch = ch;
                        raw[ch].raw = g_ads_raw;
                    }
                else
                {
                   raw[ch].status = g_ads_status;
                   raw[ch].ch = ch;
                   raw[ch].raw = -1;                    
                }
             }
        osMutexRelease(i2c1MutexHandle);
        /* make JSON*/
        int pos = 0;
        pos += snprintf(buf + pos, sizeof(buf) - pos, "[");
        for (int ch = 0; ch < 4; ch++) {
            
            pos += snprintf(buf + pos, sizeof(buf) - pos, "{\"status\":%d, \"ch\":%d, \"raw\":%d}", raw[ch].status, raw[ch].ch, raw[ch].raw);
            if (ch < 3) {
                pos += snprintf(buf + pos, sizeof(buf) - pos, ", ");
            }
        }
        pos += snprintf(buf + pos, sizeof(buf) - pos, "]\r\n");
       
        HAL_UART_Transmit(&huart1, (uint8_t *)buf, strlen(buf), HAL_MAX_DELAY);
    
        }
        else
        {
            g_mutex_ok = 0;
        }

        osDelay(1000);
    }
}

/* USER CODE END Application */

