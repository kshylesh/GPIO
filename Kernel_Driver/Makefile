CC = i586-poky-linux-gcc
ARCH = x86
CROSS_COMPILE = i586-poky-linux-


IOT_HOME = /opt/iot-devkit/1.7.2/sysroots


PATH := $(PATH):$(IOT_HOME)/x86_64-pokysdk-linux/usr/bin/i586-poky-linux

SROOT=$(IOT_HOME)/i586-poky-linux

obj-m = rgb_driver.o

all:
	make ARCH=x86 CROSS_COMPILE=i586-poky-linux- -Wall -C $(SROOT)/usr/src/kernel M=$(PWD) modules
	i586-poky-linux-gcc -Wall -o rgb_driver_tester_single.o rgb_driver_tester_single.c --sysroot=$(SROOT) -lpthread -lm
	i586-poky-linux-gcc -Wall -o rgb_driver_tester.o rgb_driver_tester.c --sysroot=$(SROOT) -lpthread -lm

clean:
	make ARCH=x86 CROSS_COMPILE=i586-poky-linux- -C $(SROOT)/usr/src/kernel M=$(PWD) clean
	rm -f *.o
