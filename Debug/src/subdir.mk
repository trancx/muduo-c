################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../src/buffer.c \
../src/log.c \
../src/main.c \
../src/server.c 

OBJS += \
./src/buffer.o \
./src/log.o \
./src/main.o \
./src/server.o 

C_DEPS += \
./src/buffer.d \
./src/log.d \
./src/main.d \
./src/server.d 


# Each subdirectory must supply rules for building sources it contributes
src/%.o: ../src/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C Compiler'
	gcc -DCONFIG_DEBUG_TCB -DCONFIG_DEBUG_CHANNEL -DCONFIG_DEBUG_TIMERQUEUE -DCONFIG_DEBUG_POLLPRI -I"/home/trance/Documents/c-cpp/muduo-c/include" -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


