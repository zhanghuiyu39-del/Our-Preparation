################################################################################
# Automatically-generated file. Do not edit!
################################################################################

SHELL = cmd.exe

CG_TOOL_ROOT := C:/ti/ccs1281/ccs/tools/compiler/ti-cgt-c2000_22.6.1.LTS

GEN_OPTS__FLAG := 
GEN_CMDS__FLAG := 

ORDERED_OBJS += \
"./USER/DITASY.obj" \
"./USER/DITASY_ADC.obj" \
"./USER/PR.obj" \
"./USER/main.obj" \
"./source/F2837xD_Adc.obj" \
"./source/F2837xD_CodeStartBranch.obj" \
"./source/F2837xD_CpuTimers.obj" \
"./source/F2837xD_DBGIER.obj" \
"./source/F2837xD_DefaultISR.obj" \
"./source/F2837xD_Dma.obj" \
"./source/F2837xD_ECap.obj" \
"./source/F2837xD_EPwm.obj" \
"./source/F2837xD_EQep.obj" \
"./source/F2837xD_Emif.obj" \
"./source/F2837xD_GlobalVariableDefs.obj" \
"./source/F2837xD_Gpio.obj" \
"./source/F2837xD_I2C.obj" \
"./source/F2837xD_Ipc.obj" \
"./source/F2837xD_Ipc_Driver.obj" \
"./source/F2837xD_Ipc_Driver_Lite.obj" \
"./source/F2837xD_Ipc_Driver_Util.obj" \
"./source/F2837xD_Mcbsp.obj" \
"./source/F2837xD_PieCtrl.obj" \
"./source/F2837xD_PieVect.obj" \
"./source/F2837xD_Sci.obj" \
"./source/F2837xD_Spi.obj" \
"./source/F2837xD_SysCtrl.obj" \
"./source/F2837xD_TempSensorConv.obj" \
"./source/F2837xD_Upp.obj" \
"./source/F2837xD_can.obj" \
"./source/F2837xD_sci_io.obj" \
"./source/F2837xD_sdfm_drivers.obj" \
"./source/F2837xD_struct.obj" \
"./source/F2837xD_usDelay.obj" \
"../HRPWM_LIB/SFO_v8_fpu_lib_build_c28_eabi.lib" \
"../cmd/2837xD_FLASH_lnk_cpu1.cmd" \
"../cmd/F2837xD_Headers_nonBIOS_cpu1.cmd" \
$(GEN_CMDS__FLAG) \
-llibc.a \

-include ../makefile.init

RM := DEL /F
RMDIR := RMDIR /S/Q

# All of the sources participating in the build are defined here
-include sources.mk
-include HRPWM_LIB/subdir_vars.mk
-include USER/subdir_vars.mk
-include cmd/subdir_vars.mk
-include source/subdir_vars.mk
-include HRPWM_LIB/subdir_rules.mk
-include USER/subdir_rules.mk
-include cmd/subdir_rules.mk
-include source/subdir_rules.mk
-include objects.mk

ifneq ($(MAKECMDGOALS),clean)
ifneq ($(strip $(C55_DEPS)),)
-include $(C55_DEPS)
endif
ifneq ($(strip $(C_UPPER_DEPS)),)
-include $(C_UPPER_DEPS)
endif
ifneq ($(strip $(S67_DEPS)),)
-include $(S67_DEPS)
endif
ifneq ($(strip $(S62_DEPS)),)
-include $(S62_DEPS)
endif
ifneq ($(strip $(S_DEPS)),)
-include $(S_DEPS)
endif
ifneq ($(strip $(OPT_DEPS)),)
-include $(OPT_DEPS)
endif
ifneq ($(strip $(C??_DEPS)),)
-include $(C??_DEPS)
endif
ifneq ($(strip $(ASM_UPPER_DEPS)),)
-include $(ASM_UPPER_DEPS)
endif
ifneq ($(strip $(S??_DEPS)),)
-include $(S??_DEPS)
endif
ifneq ($(strip $(C64_DEPS)),)
-include $(C64_DEPS)
endif
ifneq ($(strip $(CXX_DEPS)),)
-include $(CXX_DEPS)
endif
ifneq ($(strip $(S64_DEPS)),)
-include $(S64_DEPS)
endif
ifneq ($(strip $(INO_DEPS)),)
-include $(INO_DEPS)
endif
ifneq ($(strip $(CLA_DEPS)),)
-include $(CLA_DEPS)
endif
ifneq ($(strip $(S55_DEPS)),)
-include $(S55_DEPS)
endif
ifneq ($(strip $(SV7A_DEPS)),)
-include $(SV7A_DEPS)
endif
ifneq ($(strip $(C62_DEPS)),)
-include $(C62_DEPS)
endif
ifneq ($(strip $(C67_DEPS)),)
-include $(C67_DEPS)
endif
ifneq ($(strip $(PDE_DEPS)),)
-include $(PDE_DEPS)
endif
ifneq ($(strip $(K_DEPS)),)
-include $(K_DEPS)
endif
ifneq ($(strip $(C_DEPS)),)
-include $(C_DEPS)
endif
ifneq ($(strip $(CC_DEPS)),)
-include $(CC_DEPS)
endif
ifneq ($(strip $(C++_DEPS)),)
-include $(C++_DEPS)
endif
ifneq ($(strip $(C43_DEPS)),)
-include $(C43_DEPS)
endif
ifneq ($(strip $(S43_DEPS)),)
-include $(S43_DEPS)
endif
ifneq ($(strip $(ASM_DEPS)),)
-include $(ASM_DEPS)
endif
ifneq ($(strip $(S_UPPER_DEPS)),)
-include $(S_UPPER_DEPS)
endif
ifneq ($(strip $(CPP_DEPS)),)
-include $(CPP_DEPS)
endif
ifneq ($(strip $(SA_DEPS)),)
-include $(SA_DEPS)
endif
endif

-include ../makefile.defs

# Add inputs and outputs from these tool invocations to the build variables 
EXE_OUTPUTS += \
REG_LED_BLINK.out 

EXE_OUTPUTS__QUOTED += \
"REG_LED_BLINK.out" 


# All Target
all: $(OBJS) $(CMD_SRCS) $(LIB_SRCS) $(GEN_CMDS)
	@$(MAKE) --no-print-directory -Onone "REG_LED_BLINK.out"

# Tool invocations
REG_LED_BLINK.out: $(OBJS) $(CMD_SRCS) $(LIB_SRCS) $(GEN_CMDS)
	@echo 'Building target: "$@"'
	@echo 'Invoking: C2000 Linker'
	"C:/ti/ccs1281/ccs/tools/compiler/ti-cgt-c2000_22.6.1.LTS/bin/cl2000" -v28 -ml -mt --cla_support=cla1 --float_support=fpu32 --tmu_support=tmu0 --vcu_support=vcu2 -O2 --advice:performance=all --define=_INLINE --define=_FLASH --define=CPU1 --diag_warning=225 --diag_wrap=off --display_error_number --abi=eabi -z -m"REG_LED_BLINK.map" --stack_size=0x200 --warn_sections -i"C:/ti/ccs1281/ccs/tools/compiler/ti-cgt-c2000_22.6.1.LTS/lib" -i"C:/ti/ccs1281/ccs/tools/compiler/ti-cgt-c2000_22.6.1.LTS/include" --reread_libs --diag_wrap=off --display_error_number --xml_link_info="REG_LED_BLINK_linkInfo.xml" --rom_model -o "REG_LED_BLINK.out" $(ORDERED_OBJS)
	@echo 'Finished building target: "$@"'
	@echo ' '

# Other Targets
clean:
	-$(RM) $(EXE_OUTPUTS__QUOTED)
	-$(RM) "USER\DITASY.obj" "USER\DITASY_ADC.obj" "USER\PR.obj" "USER\main.obj" "source\F2837xD_Adc.obj" "source\F2837xD_CodeStartBranch.obj" "source\F2837xD_CpuTimers.obj" "source\F2837xD_DBGIER.obj" "source\F2837xD_DefaultISR.obj" "source\F2837xD_Dma.obj" "source\F2837xD_ECap.obj" "source\F2837xD_EPwm.obj" "source\F2837xD_EQep.obj" "source\F2837xD_Emif.obj" "source\F2837xD_GlobalVariableDefs.obj" "source\F2837xD_Gpio.obj" "source\F2837xD_I2C.obj" "source\F2837xD_Ipc.obj" "source\F2837xD_Ipc_Driver.obj" "source\F2837xD_Ipc_Driver_Lite.obj" "source\F2837xD_Ipc_Driver_Util.obj" "source\F2837xD_Mcbsp.obj" "source\F2837xD_PieCtrl.obj" "source\F2837xD_PieVect.obj" "source\F2837xD_Sci.obj" "source\F2837xD_Spi.obj" "source\F2837xD_SysCtrl.obj" "source\F2837xD_TempSensorConv.obj" "source\F2837xD_Upp.obj" "source\F2837xD_can.obj" "source\F2837xD_sci_io.obj" "source\F2837xD_sdfm_drivers.obj" "source\F2837xD_struct.obj" "source\F2837xD_usDelay.obj" 
	-$(RM) "USER\DITASY.d" "USER\DITASY_ADC.d" "USER\PR.d" "USER\main.d" "source\F2837xD_Adc.d" "source\F2837xD_CpuTimers.d" "source\F2837xD_DefaultISR.d" "source\F2837xD_Dma.d" "source\F2837xD_ECap.d" "source\F2837xD_EPwm.d" "source\F2837xD_EQep.d" "source\F2837xD_Emif.d" "source\F2837xD_GlobalVariableDefs.d" "source\F2837xD_Gpio.d" "source\F2837xD_I2C.d" "source\F2837xD_Ipc.d" "source\F2837xD_Ipc_Driver.d" "source\F2837xD_Ipc_Driver_Lite.d" "source\F2837xD_Ipc_Driver_Util.d" "source\F2837xD_Mcbsp.d" "source\F2837xD_PieCtrl.d" "source\F2837xD_PieVect.d" "source\F2837xD_Sci.d" "source\F2837xD_Spi.d" "source\F2837xD_SysCtrl.d" "source\F2837xD_TempSensorConv.d" "source\F2837xD_Upp.d" "source\F2837xD_can.d" "source\F2837xD_sci_io.d" "source\F2837xD_sdfm_drivers.d" "source\F2837xD_struct.d" 
	-$(RM) "source\F2837xD_CodeStartBranch.d" "source\F2837xD_DBGIER.d" "source\F2837xD_usDelay.d" 
	-@echo 'Finished clean'
	-@echo ' '

.PHONY: all clean dependents
.SECONDARY:

-include ../makefile.targets

