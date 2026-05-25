# MyLib - Custom Libraries

Эта папка содержит пользовательские библиотеки для проекта F103First.

## Файлы

### liquidcrystal_i2c.h / liquidcrystal_i2c.c
Драйвер для LCD дисплея 1602/2004 с I2C интерфейсом (PCF8574).

**Основные функции:**
- `HD44780_Init(uint8_t rows)` - инициализация дисплея
- `HD44780_Clear()` - очистка дисплея
- `HD44780_SetCursor(uint8_t col, uint8_t row)` - установка курсора
- `HD44780_PrintStr(char *str)` - вывод строки
- `HD44780_Backlight()` - включение подсветки
- `HD44780_NoBacklight()` - выключение подсветки

### ds18b20.h / ds18b20.c
Драйвер для цифрового датчика температуры DS18B20 с интерфейсом 1-Wire.

**Конфигурация:**
- Порт: `GPIOB`
- Пин: `GPIO_PIN_1`
- Требуется внешний pull-up резистор 4.7kΩ

**Основные функции:**
- `DS18B20_DWT_Init()` - инициализация DWT для микросекундных задержек
- `DS18B20_ReadTemperature(float *temperature)` - чтение температуры (float)
- `DS18B20_ReadTemperatureInt(int16_t *temp_integer, int16_t *temp_fraction)` - чтение температуры (целочисленный формат)

**Коды возврата:**
- `DS18B20_OK` (0) - успешное чтение
- `DS18B20_ERROR_NO_DEVICE` (1) - датчик не найден
- `DS18B20_ERROR_CRC` (2) - ошибка контрольной суммы
- `DS18B20_ERROR_INVALID_DATA` (3) - неверные данные

**Внутренние функции:**
- `DS18B20_Reset()` - сброс шины 1-Wire
- `DS18B20_WriteBit()` / `DS18B20_ReadBit()` - чтение/запись бита
- `DS18B20_WriteByte()` / `DS18B20_ReadByte()` - чтение/запись байта
- `DS18B20_CRC8()` - вычисление контрольной суммы

## Подключение в проект

В файле `main.c` добавьте:
```c
#include "liquidcrystal_i2c.h"
#include "ds18b20.h"
```

## Использование DS18B20

```c
// Инициализация DWT для микросекундных задержек
DS18B20_DWT_Init();

// Чтение температуры (целочисленный формат)
int16_t temp_integer, temp_fraction;
uint8_t result = DS18B20_ReadTemperatureInt(&temp_integer, &temp_fraction);

if (result == DS18B20_OK) {
    // Температура прочитана успешно
    printf("T=%d.%02d C\n", temp_integer, temp_fraction);
} else if (result == DS18B20_ERROR_NO_DEVICE) {
    printf("Sensor not found!\n");
} else if (result == DS18B20_ERROR_CRC) {
    printf("CRC error!\n");
}
```
