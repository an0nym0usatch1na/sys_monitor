#Tell architecture
ARCH = arm

#Tell cross compiler
CROSS_COMPILE = arm-eabi-

ifeq ($(KERNELRELEASE),)
	#Normal Makefile
	# develop pc for goldfish
	KERNELDIR ?= /home/anonymous/android/sources/goldfish
	
	# develop pc for msm
	#KERNELDIR ?= /home/anonymous/android/msm
	
	# office pc for goldfish
	#KERNELDIR ?= /android/sources/goldfish
	PWD := $(shell pwd)

default:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE)

clean:
	rm -rf *.o *~ core .depend .*.cmd *.ko *.mod.c .tmp_versions ./sys_calls/*.o

else
	#kbuild part of makefile	
	obj-m := sys_monitor.o 
	
	sys_monitor-y := main.o log.o trace_dog.o ./sys_calls/open.o ./sys_calls/close.o ./sys_calls/read.o ./sys_calls/write.o ./sys_calls/ioctl.o ./sys_calls/fork.o ./sys_calls/exit.o ./sys_calls/exec.o interface.o ./../share/debug.o process.o fd_cache.o
endif
