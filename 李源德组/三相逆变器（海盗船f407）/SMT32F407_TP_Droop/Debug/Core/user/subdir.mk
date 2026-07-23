################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (13.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Core/user/adc_convert.c \
../Core/user/control.c \
../Core/user/vofa.c 

OBJS += \
./Core/user/adc_convert.o \
./Core/user/control.o \
./Core/user/vofa.o 

C_DEPS += \
./Core/user/adc_convert.d \
./Core/user/control.d \
./Core/user/vofa.d 


# Each subdirectory must supply rules for building sources it contributes
Core/user/%.o Core/user/%.su Core/user/%.cyclo: ../Core/user/%.c Core/user/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32F407xx -c -I../Core/Inc -I../Drivers/STM32F4xx_HAL_Driver/Inc -I../Drivers/STM32F4xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32F4xx/Include -I../Drivers/CMSIS/Include -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-Core-2f-user

clean-Core-2f-user:
	-$(RM) ./Core/user/adc_convert.cyclo ./Core/user/adc_convert.d ./Core/user/adc_convert.o ./Core/user/adc_convert.su ./Core/user/control.cyclo ./Core/user/control.d ./Core/user/control.o ./Core/user/control.su ./Core/user/vofa.cyclo ./Core/user/vofa.d ./Core/user/vofa.o ./Core/user/vofa.su

.PHONY: clean-Core-2f-user

