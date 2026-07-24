################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (13.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Core/user/oled/oled.c \
../Core/user/oled/soft_i2c.c 

OBJS += \
./Core/user/oled/oled.o \
./Core/user/oled/soft_i2c.o 

C_DEPS += \
./Core/user/oled/oled.d \
./Core/user/oled/soft_i2c.d 


# Each subdirectory must supply rules for building sources it contributes
Core/user/oled/%.o Core/user/oled/%.su Core/user/oled/%.cyclo: ../Core/user/oled/%.c Core/user/oled/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32F407xx -c -I../Core/Inc -I../Drivers/STM32F4xx_HAL_Driver/Inc -I../Drivers/STM32F4xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32F4xx/Include -I../Drivers/CMSIS/Include -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-Core-2f-user-2f-oled

clean-Core-2f-user-2f-oled:
	-$(RM) ./Core/user/oled/oled.cyclo ./Core/user/oled/oled.d ./Core/user/oled/oled.o ./Core/user/oled/oled.su ./Core/user/oled/soft_i2c.cyclo ./Core/user/oled/soft_i2c.d ./Core/user/oled/soft_i2c.o ./Core/user/oled/soft_i2c.su

.PHONY: clean-Core-2f-user-2f-oled

