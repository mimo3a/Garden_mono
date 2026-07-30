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
ADC_HandleTypeDef hadc1;

I2C_HandleTypeDef hi2c1;

UART_HandleTypeDef huart1;

/* USER CODE BEGIN PV */
volatile uint8_t measure_flag = 0;
uint32_t adc_value = 0;
ADS1115_t ads;

uint8_t rx_data;
char rx_buffer[32];
uint8_t rx_index = 0;

/* Батарея замеряется через ADC1_IN3 (PA3), делитель 100k+100k между BAT+ и GND.
   hadc1 объявлен CubeMX'ом выше, ReadBatteryMillivolts() читает через него. */

/* Последнее измеренное напряжение батареи, мВ.
   Начальное значение 4200 = полный заряд — чтобы первый цикл после
   power-on не решил сразу что «критично» и не ушёл в STANDBY. */
static uint32_t last_battery_mV = 4200;

/* Пороги для одной ячейки 18650 Li-Ion. */
#define BATTERY_LOW_MV        3400U   /* ниже — флажок в JSON  */
#define BATTERY_CRITICAL_MV   3100U   /* ниже — прощальный blink и STANDBY */
#define BATTERY_VREF_MV       3300U   /* VDD/VREF+ STM32 после LDO */
#define BATTERY_ADC_MAX       4095U   /* 12-bit ADC */
#define BATTERY_DIVIDER       2U      /* R1=R2=100k → ×2 */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_I2C1_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_ADC1_Init(void);
/* USER CODE BEGIN PFP */

uint8_t Moisture_ToPercent(uint32_t raw, uint32_t dry, uint32_t wet);
static void EnterStopMode(void);
static uint8_t Debugger_IsAttached(void);
static void LED_Blink(uint8_t count);
HAL_StatusTypeDef MeasureAndDisplay(void);
static uint32_t ReadBatteryMillivolts(void);
static void CriticalBatteryShutdown(void);

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin) {

	if(GPIO_Pin == GPIO_PIN_10){
		/* PA10 = UART1 RX переконфигурирован как EXTI перед STOP.
		   Стартовый бит первого байта от ESP будит MCU без провода. */
		measure_flag = 1;

	}
}

/* Диагностика через встроенный LED на PC13 (инверсная: LOW = ON)
   и внешний параллельный LED на PB12 (прямая: HIGH = ON).
   Оба зажигаются/гасятся синхронно.
   1 короткая вспышка = OK, 2 = ошибка, 3 = отдельная ошибка (см. main loop). */
static void LED_Blink(uint8_t count)
{
    for (uint8_t i = 0; i < count; i++)
    {
        HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET); /* PC13 ON  (active LOW)  */
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_12, GPIO_PIN_SET);   /* PB12 ON  (active HIGH) */
        HAL_Delay(120);
        HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET);   /* PC13 OFF */
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_12, GPIO_PIN_RESET); /* PB12 OFF */
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

    last_battery_mV = ReadBatteryMillivolts();

    uint8_t result = DS18B20_ReadTemperatureInt(&temp_integer, &temp_fraction);

    char uart_buffer[256];

    if(result == DS18B20_OK)
    {
        snprintf(uart_buffer,
            sizeof(uart_buffer),
            "{\"deviceId\":1,"
            "\"temperature\":%d.%02d,"
            "\"soil\":[%lu,%lu,%lu,%lu],"
            "\"battery\":%lu}\r\n",
            temp_integer,
            temp_fraction,
            soil1, soil2, soil3, soil4,
            last_battery_mV);
    }
    else
    {
        snprintf(uart_buffer,
            sizeof(uart_buffer),
            "{\"deviceId\":1,"
            "\"temperature\":null,"
            "\"soil\":[%lu,%lu,%lu,%lu],"
            "\"battery\":%lu}\r\n",
            soil1, soil2, soil3, soil4,
            last_battery_mV);
    }



    return HAL_UART_Transmit(&huart1,
                             (uint8_t*)uart_buffer,
                             strlen(uart_buffer),
                             2000);
}

/* Читает батарею: 8 сэмплов, усреднение, учёт делителя ×2. */
static uint32_t ReadBatteryMillivolts(void)
{
    uint32_t sum = 0;
    for (int i = 0; i < 8; i++)
    {
        HAL_ADC_Start(&hadc1);
        HAL_ADC_PollForConversion(&hadc1, 10);
        sum += HAL_ADC_GetValue(&hadc1);
        HAL_ADC_Stop(&hadc1);
    }
    uint32_t raw = sum / 8U;
    return (raw * BATTERY_VREF_MV * BATTERY_DIVIDER) / BATTERY_ADC_MAX;
}

/* Критический разряд: 5 медленных морганий обоих LED (по секунде на такт),
   потом STANDBY. Выход из STANDBY — только через NRST/power-on. */
static void CriticalBatteryShutdown(void)
{
    for (int i = 0; i < 5; i++)
    {
        HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET); /* PC13 ON  */
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_12, GPIO_PIN_SET);   /* PB12 ON  */
        HAL_Delay(500);
        HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET);   /* PC13 OFF */
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_12, GPIO_PIN_RESET); /* PB12 OFF */
        HAL_Delay(500);
    }
    HAL_PWR_EnterSTANDBYMode();
    /* Сюда не возвращаемся — STANDBY = reset при пробуждении. */
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

    /* Оба LED выключить перед сном, чтобы не жрать ток в STOP. */
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET);   /* PC13 OFF (active LOW)  */
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_12, GPIO_PIN_RESET); /* PB12 OFF (active HIGH) */

    /* DeInit UART — освобождает PA10 (RX pin) для переконфигурации в EXTI. */
    HAL_UART_AbortReceive_IT(&huart1);
    HAL_UART_DeInit(&huart1);

    /* PA10 (UART1 RX) → EXTI falling edge. UART idle = HIGH; стартовый
       бит первого байта от ESP тянет линию LOW — этого достаточно чтобы
       разбудить MCU из STOP без отдельного провода GPIO25→PA1. */
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin  = GPIO_PIN_10;
    GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
    GPIO_InitStruct.Pull = GPIO_PULLUP; /* удерживает HIGH пока ESP в sleep (TX флоатит) */
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
    HAL_NVIC_SetPriority(EXTI15_10_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);

    HAL_SuspendTick();
    HAL_PWR_EnterSTOPMode(PWR_LOWPOWERREGULATOR_ON, PWR_STOPENTRY_WFI);

    SystemClock_Config();
    HAL_ResumeTick();

    HAL_NVIC_DisableIRQ(EXTI15_10_IRQn);

    /* Полный ре-init UART после STOP. Без этого первая TX после пробуждения
       выходит мусором (␀␀␀…) — peripheral после STOP в неопределённом состоянии. */
    MX_USART1_UART_Init();
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
  MX_ADC1_Init();
  /* USER CODE BEGIN 2 */

  /* Держать SWD живым в low-power режимах — полезно для отладки. */
  __HAL_RCC_PWR_CLK_ENABLE();
  HAL_DBGMCU_EnableDBGSleepMode();
  HAL_DBGMCU_EnableDBGStopMode();
  HAL_DBGMCU_EnableDBGStandbyMode();

  /* Калибровка ADC1 — обязательна на F103 для точности >±5 LSB.
     CubeMX её не генерит, делаем сами один раз при старте. */
  HAL_ADCEx_Calibration_Start(&hadc1);

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

        /* Критическая батарея — прощай, до замены/зарядки.
           Нижний порог 2400 мВ соответствует срабатыванию DW01A: ниже этого
           реальная батарея физически не может быть (модуль защиты отсекает
           нагрузку). Всё что ниже — «делитель не подключён / PA3 болтается».
           Требуются 3 замера подряд в диапазоне 2400..CRITICAL мВ. */
        static uint8_t critical_streak = 0;
        if (last_battery_mV > 2400U && last_battery_mV < BATTERY_CRITICAL_MV)
        {
            critical_streak++;
            if (critical_streak >= 3)
            {
                CriticalBatteryShutdown();
            }
        }
        else
        {
            critical_streak = 0;
        }
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
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

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
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_ADC;
  PeriphClkInit.AdcClockSelection = RCC_ADCPCLK2_DIV6;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief ADC1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_ADC1_Init(void)
{

  /* USER CODE BEGIN ADC1_Init 0 */

  /* USER CODE END ADC1_Init 0 */

  ADC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN ADC1_Init 1 */

  /* USER CODE END ADC1_Init 1 */

  /** Common config
  */
  hadc1.Instance = ADC1;
  hadc1.Init.ScanConvMode = ADC_SCAN_DISABLE;
  hadc1.Init.ContinuousConvMode = DISABLE;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc1.Init.NbrOfConversion = 1;
  if (HAL_ADC_Init(&hadc1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_3;
  sConfig.Rank = ADC_REGULAR_RANK_1;
  sConfig.SamplingTime = ADC_SAMPLETIME_71CYCLES_5;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC1_Init 2 */
  /* Override sampling time: 7.5 циклов (625 нс) слишком коротко для источника с
     Thevenin ~50 кΩ (наш делитель 100k+100k). ADC не успевает зарядить sample cap →
     показания занижены. 71.5 циклов ≈ 6 мкс — с большим запасом. Лучше это же
     значение выставить и в CubeMX (Parameter Settings → Rank 1 → Sampling Time),
     чтобы override не был нужен. */
  sConfig.SamplingTime = ADC_SAMPLETIME_71CYCLES_5;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE END ADC1_Init 2 */

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
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1|GPIO_PIN_12, GPIO_PIN_RESET);

  /*Configure GPIO pin : PC13 */
  GPIO_InitStruct.Pin = GPIO_PIN_13;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pins : PB1 PB12 */
  GPIO_InitStruct.Pin = GPIO_PIN_1|GPIO_PIN_12;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

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
