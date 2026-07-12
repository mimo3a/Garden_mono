/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body (FIXED)
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include <string.h>
#include "ds18b20.h"
#include "ads1115.h"
#include "moisture_sensor.h"

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define STARTUP_DEBUG_WINDOW_MS 5000

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */


/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
I2C_HandleTypeDef hi2c1;

UART_HandleTypeDef huart1;

/* USER CODE BEGIN PV */
volatile uint8_t measure_flag = 0;
uint32_t adc_value = 0;
ADS1115_t ads;

uint8_t rx_data;
char rx_buffer[32];
uint8_t rx_index = 0;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_I2C1_Init(void);
static void MX_USART1_UART_Init(void);
/* USER CODE BEGIN PFP */

uint8_t Moisture_ToPercent(uint32_t raw, uint32_t dry, uint32_t wet);
static void EnterStopMode(void);
static uint8_t Debugger_IsAttached(void);
static void LED_Blink(uint8_t count);
HAL_StatusTypeDef MeasureAndDisplay(void);

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin) {

	if(GPIO_Pin == GPIO_PIN_1){

		measure_flag = 1;

	}
}

/* Диагностика через встроенный LED на PC13 (инверсная логика: LOW = ON).
   1 короткая вспышка = событие OK, 2 = ошибка, 3 = отдельная ошибка (см. main loop). */
static void LED_Blink(uint8_t count)
{
    for (uint8_t i = 0; i < count; i++)
    {
        HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET); /* ON  */
        HAL_Delay(120);
        HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET);   /* OFF */
        HAL_Delay(200);
    }
    HAL_Delay(400); /* пауза между группами вспышек */
}


HAL_StatusTypeDef MeasureAndDisplay(void) {

    int16_t temp_integer = 0;
    int16_t temp_fraction = 0;

    uint32_t raw1, raw2, raw3, raw4;
    uint32_t soil1, soil2, soil3, soil4;

    raw1 = MoistureSensor_Read(ADS1115_CHANNEL_0);
    raw2 = MoistureSensor_Read(ADS1115_CHANNEL_1);
    raw3 = MoistureSensor_Read(ADS1115_CHANNEL_2);
    raw4 = MoistureSensor_Read(ADS1115_CHANNEL_3);

    soil1 = Moisture_ToPercent(raw1, 17300, 7100);
    soil2 = Moisture_ToPercent(raw2, 17300, 7100);
    soil3 = Moisture_ToPercent(raw3, 17300, 7100);
    soil4 = Moisture_ToPercent(raw4, 17300, 7100);

    uint8_t result = DS18B20_ReadTemperatureInt(&temp_integer, &temp_fraction);

    char uart_buffer[256];

    if(result == DS18B20_OK)
    {
        snprintf(uart_buffer,
            sizeof(uart_buffer),
            "{\"deviceId\":1,"
            "\"temperature\":%d.%02d,"
            "\"soil\":[%lu,%lu,%lu,%lu]}\r\n",
            temp_integer,
            temp_fraction,
            soil1, soil2, soil3, soil4);
    }
    else
    {
        snprintf(uart_buffer,
            sizeof(uart_buffer),
            "{\"deviceId\":1,"
            "\"temperature\":null,"
            "\"soil\":[%lu,%lu,%lu,%lu]}\r\n",
            soil1, soil2, soil3, soil4);
    }



    return HAL_UART_Transmit(&huart1,
                             (uint8_t*)uart_buffer,
                             strlen(uart_buffer),
                             2000);
}

uint8_t Moisture_ToPercent(uint32_t raw, uint32_t dry, uint32_t wet)
{
    if (raw >= dry) return 0;
    if (raw <= wet) return 100;

    return (uint8_t)(((dry - raw) * 100U) / (dry - wet));
}

static void EnterStopMode(void)
{
    if (Debugger_IsAttached())
    {
        HAL_Delay(100);
        return;
    }

    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET);

    HAL_SuspendTick();
    HAL_PWR_EnterSTOPMode(PWR_LOWPOWERREGULATOR_ON, PWR_STOPENTRY_WFI);

    SystemClock_Config();
    HAL_ResumeTick();

    HAL_UART_Receive_IT(&huart1, &rx_data, 1);
}

static uint8_t Debugger_IsAttached(void)
{
    return (CoreDebug->DHCSR & CoreDebug_DHCSR_C_DEBUGEN_Msk) != 0U;
}
	

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */


  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_I2C1_Init();
  MX_USART1_UART_Init();
  /* USER CODE BEGIN 2 */

  /* Держать SWD живым в low-power режимах — иначе CubeProgrammer не подцепится,
     когда MCU в STOP. Должно быть выставлено ДО первого захода в сон. */
  __HAL_RCC_PWR_CLK_ENABLE();
  HAL_DBGMCU_EnableDBGSleepMode();
  HAL_DBGMCU_EnableDBGStopMode();
  HAL_DBGMCU_EnableDBGStandbyMode();

  /* FLASH-SAFE BOOT: если при reset удерживать кнопку PA3, MCU навсегда
     остаётся в этом цикле (быстрое мигание LED) и никогда не уходит в STOP.
     Это гарантирует, что STM32CubeProgrammer всегда сможет прошить чип
     после любой сломанной прошивки. Сценарий:
       1. Зажать кнопку PA3.
       2. Нажать RESET (или передёрнуть питание).
       3. Отпустить RESET, кнопку продолжать держать.
       4. Прошить в CubeProgrammer.
       5. Отпустить кнопку → нажать RESET → новая прошивка запустится нормально. */
  HAL_Delay(20);  /* дать pull-up устаканиться */
  if (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_3) == GPIO_PIN_RESET)
  {
      while (1)
      {
          HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
          HAL_Delay(80);
      }
  }

  HAL_UART_Receive_IT(&huart1, &rx_data, 1);

  ADS1115_Init(&ads, &hi2c1, ADS1115_ADDR_GND,
               ADS1115_PGA_4_096V,
               ADS1115_DR_128SPS);

  if (ADS1115_IsReady(&ads) != HAL_OK)
  {
      char msg[] = "ADS1115 not found\r\n";
      HAL_UART_Transmit(&huart1, (uint8_t*)msg, strlen(msg), HAL_MAX_DELAY);
  }
  else
  {
      char msg[] = "ADS1115 OK\r\n";
      HAL_UART_Transmit(&huart1, (uint8_t*)msg, strlen(msg), HAL_MAX_DELAY);
  }

  // !!! ВАЖНО: Включаем счетчик циклов для delay_us ДО инициализации дисплея !!!
  DS18B20_DWT_Init();

  HAL_Delay(10);
  
  // Тест LED - мигнем 3 раза для проверки работы
  // Blue Pill: LED активен когда PC13 = LOW (инверсная логика)
  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET); // LED выключен
  HAL_Delay(500);
  
  for(int i = 0; i < 3; i++) {
      HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET);   // LED включен
      HAL_Delay(200);
      HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET); // LED выключен
      HAL_Delay(200);
  }
  
  HAL_Delay(500); // Пауза для проверки
  HAL_Delay(STARTUP_DEBUG_WINDOW_MS);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1){


    if (measure_flag)
    {
        measure_flag = 0;
        LED_Blink(1);                                 /* проснулись */
        HAL_StatusTypeDef tx = MeasureAndDisplay();
        LED_Blink(tx == HAL_OK ? 1 : 2);              /* 1 = отправлено, 2 = ошибка UART */
    }

    EnterStopMode();
  
    
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief I2C1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C1_Init(void)
{

  /* USER CODE BEGIN I2C1_Init 0 */

  /* USER CODE END I2C1_Init 0 */

  /* USER CODE BEGIN I2C1_Init 1 */

  /* USER CODE END I2C1_Init 1 */
  hi2c1.Instance = I2C1;
  hi2c1.Init.ClockSpeed = 100000;
  hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C1_Init 2 */

  /* USER CODE END I2C1_Init 2 */

}

/**
  * @brief USART1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART1_UART_Init(void)
{

  /* USER CODE BEGIN USART1_Init 0 */

  /* USER CODE END USART1_Init 0 */

  /* USER CODE BEGIN USART1_Init 1 */

  /* USER CODE END USART1_Init 1 */
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 115200;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART1_Init 2 */

  /* USER CODE END USART1_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1, GPIO_PIN_RESET);

  /*Configure GPIO pin : PC13 */
  GPIO_InitStruct.Pin = GPIO_PIN_13;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pin : PA1 */
  GPIO_InitStruct.Pin = GPIO_PIN_1;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
  GPIO_InitStruct.Pull = GPIO_PULLDOWN;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pin : PB1 */
  GPIO_InitStruct.Pin = GPIO_PIN_1;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* EXTI interrupt init*/
  HAL_NVIC_SetPriority(EXTI1_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI1_IRQn);

  /* USER CODE BEGIN MX_GPIO_Init_2 */
  GPIO_InitStruct.Pin = GPIO_PIN_3;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/**
  * @brief  EXTI line detection callback.
  * @param  GPIO_Pin: Specifies the pins connected EXTI line
  * @retval None
  */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1)
    {
        if (rx_data == '\r')
        {
            /* Ignore CR so both "MEASURE\n" and "MEASURE\r\n" work. */
        }
        else if (rx_data == '\n')
        {
            rx_buffer[rx_index] = '\0';

            if (strcmp(rx_buffer, "MEASURE") == 0)
            {
                measure_flag = 1;
            }

            rx_index = 0;
        }
        else
        {
            if (rx_index < sizeof(rx_buffer) - 1)
            {
                rx_buffer[rx_index++] = rx_data;
            }
        }

        HAL_UART_Receive_IT(&huart1, &rx_data, 1);
    }
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1)
    {
        HAL_UART_Receive_IT(&huart1, &rx_data, 1);
    }
}
/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
