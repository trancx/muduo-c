################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../src/core/channel.c \
../src/core/eventloop.c \
../src/core/poller.c \
../src/core/timer_queue.c \
../src/core/vector.c 

OBJS += \
./src/core/channel.o \
./src/core/eventloop.o \
./src/core/poller.o \
./src/core/timer_queue.o \
./src/core/vector.o 

C_DEPS += \
./src/core/channel.d \
./src/core/eventloop.d \
./src/core/poller.d \
./src/core/timer_queue.d \
./src/core/vector.d 


# Each subdirectory must supply rules for building sources it contributes
src/core/%.o: ../src/core/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C Compiler'
	gcc -DCONFIG_DEBUG_TCB -DCONFIG_DEBUG_CHANNEL -DCONFIG_DEBUG_TIMERQUEUE -DCONFIG_DEBUG_POLLPRI -I"/home/trance/Documents/c-cpp/muduo-c/include" -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


