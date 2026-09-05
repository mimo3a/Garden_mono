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
    char buf[64];

    for (;;)
    {
        g_ads_count++;

        if (osMutexAcquire(i2c1MutexHandle, pdMS_TO_TICKS(100)) == osOK)
        {
            g_mutex_ok = 1;
            HAL_StatusTypeDef status = ADS1115_ReadChannelRaw(&ads, ADS1115_CHANNEL_0, &g_ads_raw);
            osMutexRelease(i2c1MutexHandle);

            g_ads_status = (uint8_t)status;

            if (status == HAL_OK)
            {
                snprintf(buf, sizeof(buf), "ADS CH0: %d\r\n", g_ads_raw);
                HAL_UART_Transmit(&huart1, (uint8_t *)buf, strlen(buf), HAL_MAX_DELAY);
            }
        }
        else
        {
            g_mutex_ok = 0;
        }

        osDelay(1000);
    }
}

/* USER CODE END Application */

