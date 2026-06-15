################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (13.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Core/MyLib/ads1115.c \
../Core/MyLib/ds18b20.c \
../Core/MyLib/liquidcrystal_i2c.c 

OBJS += \
./Core/MyLib/ads1115.o \
./Core/MyLib/ds18b20.o \
./Core/MyLib/liquidcrystal_i2c.o 

C_DEPS += \
./Core/MyLib/ads1115.d \
./Core/MyLib/ds18b20.d \
./Core/MyLib/liquidcrystal_i2c.d 


# Each subdirectory must supply rules for building sources it contributes
Core/MyLib/%.o Core/MyLib/%.su Core/MyLib/%.cyclo: ../Core/MyLib/%.c Core/MyLib/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m3 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32F103xB -c -I../Core/Inc -I../Drivers/STM32F1xx_HAL_Driver/Inc/Legacy -I../Drivers/STM32F1xx_HAL_Driver/Inc -I../Drivers/CMSIS/Device/ST/STM32F1xx/Include -I../Drivers/CMSIS/Include -I../Core/MyLib -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfloat-abi=soft -mthumb -o "$@"

clean: clean-Core-2f-MyLib

clean-Core-2f-MyLib:
	-$(RM) ./Core/MyLib/ads1115.cyclo ./Core/MyLib/ads1115.d ./Core/MyLib/ads1115.o ./Core/MyLib/ads1115.su ./Core/MyLib/ds18b20.cyclo ./Core/MyLib/ds18b20.d ./Core/MyLib/ds18b20.o ./Core/MyLib/ds18b20.su ./Core/MyLib/liquidcrystal_i2c.cyclo ./Core/MyLib/liquidcrystal_i2c.d ./Core/MyLib/liquidcrystal_i2c.o ./Core/MyLib/liquidcrystal_i2c.su

.PHONY: clean-Core-2f-MyLib

