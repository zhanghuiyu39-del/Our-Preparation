################################################################################
# Automatically-generated file. Do not edit!
################################################################################

SHELL = cmd.exe

# Each subdirectory must supply rules for building sources it contributes
Init_examples/%.obj: ../Init_examples/%.cpp $(GEN_OPTS) | $(GEN_FILES) $(GEN_MISC_FILES)
	@echo 'Building file: "$<"'
	@echo 'Invoking: C2000 Compiler'
	"C:/ti/ccs1281/ccs/tools/compiler/ti-cgt-c2000_22.6.1.LTS/bin/cl2000" -v28 -ml -mt --cla_support=cla1 --float_support=fpu32 --tmu_support=tmu0 --vcu_support=vcu2 -O2 --include_path="C:/F28379WorkSpace/REG_F28379D" --include_path="C:/F28379WorkSpace/REG_F28379D/HRPWM_LIB" --include_path="C:/F28379WorkSpace/REG_F28379D/include" --include_path="C:/F28379WorkSpace/REG_F28379D/source" --include_path="C:/F28379WorkSpace/REG_F28379D/USER" --include_path="C:/F28379WorkSpace/REG_F28379D/cmd" --include_path="C:/ti/ccs1281/ccs/tools/compiler/ti-cgt-c2000_22.6.1.LTS/include" --advice:performance=all --define=_INLINE --define=_FLASH --define=CPU1 --diag_warning=225 --diag_wrap=off --display_error_number --abi=eabi --preproc_with_compile --preproc_dependency="Init_examples/$(basename $(<F)).d_raw" --obj_directory="Init_examples" $(GEN_OPTS__FLAG) "$<"
	@echo 'Finished building: "$<"'
	@echo ' '

Init_examples/%.obj: ../Init_examples/%.c $(GEN_OPTS) | $(GEN_FILES) $(GEN_MISC_FILES)
	@echo 'Building file: "$<"'
	@echo 'Invoking: C2000 Compiler'
	"C:/ti/ccs1281/ccs/tools/compiler/ti-cgt-c2000_22.6.1.LTS/bin/cl2000" -v28 -ml -mt --cla_support=cla1 --float_support=fpu32 --tmu_support=tmu0 --vcu_support=vcu2 -O2 --include_path="C:/F28379WorkSpace/REG_F28379D" --include_path="C:/F28379WorkSpace/REG_F28379D/HRPWM_LIB" --include_path="C:/F28379WorkSpace/REG_F28379D/include" --include_path="C:/F28379WorkSpace/REG_F28379D/source" --include_path="C:/F28379WorkSpace/REG_F28379D/USER" --include_path="C:/F28379WorkSpace/REG_F28379D/cmd" --include_path="C:/ti/ccs1281/ccs/tools/compiler/ti-cgt-c2000_22.6.1.LTS/include" --advice:performance=all --define=_INLINE --define=_FLASH --define=CPU1 --diag_warning=225 --diag_wrap=off --display_error_number --abi=eabi --preproc_with_compile --preproc_dependency="Init_examples/$(basename $(<F)).d_raw" --obj_directory="Init_examples" $(GEN_OPTS__FLAG) "$<"
	@echo 'Finished building: "$<"'
	@echo ' '

Init_examples/%.obj: ../Init_examples/%.C $(GEN_OPTS) | $(GEN_FILES) $(GEN_MISC_FILES)
	@echo 'Building file: "$<"'
	@echo 'Invoking: C2000 Compiler'
	"C:/ti/ccs1281/ccs/tools/compiler/ti-cgt-c2000_22.6.1.LTS/bin/cl2000" -v28 -ml -mt --cla_support=cla1 --float_support=fpu32 --tmu_support=tmu0 --vcu_support=vcu2 -O2 --include_path="C:/F28379WorkSpace/REG_F28379D" --include_path="C:/F28379WorkSpace/REG_F28379D/HRPWM_LIB" --include_path="C:/F28379WorkSpace/REG_F28379D/include" --include_path="C:/F28379WorkSpace/REG_F28379D/source" --include_path="C:/F28379WorkSpace/REG_F28379D/USER" --include_path="C:/F28379WorkSpace/REG_F28379D/cmd" --include_path="C:/ti/ccs1281/ccs/tools/compiler/ti-cgt-c2000_22.6.1.LTS/include" --advice:performance=all --define=_INLINE --define=_FLASH --define=CPU1 --diag_warning=225 --diag_wrap=off --display_error_number --abi=eabi --preproc_with_compile --preproc_dependency="Init_examples/$(basename $(<F)).d_raw" --obj_directory="Init_examples" $(GEN_OPTS__FLAG) "$<"
	@echo 'Finished building: "$<"'
	@echo ' '


