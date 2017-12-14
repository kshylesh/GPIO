CC = i586-poky-linux-gcc
ARCH = x86
CROSS_COMPILE = i586-poky-linux-gcc


IOT_HOME = /opt/iot-devkit/1.7.2/sysroots


PATH := $(PATH):$(IOT_HOME)/x86_64-pokysdk-linux/usr/bin/i586-poky-linux

SROOT=$(IOT_HOME)/i586-poky-linux

#obj-m = rbt530_drv.o
app=led
all: rgb_tester.o rgb_function.o
	
	$(CROSS_COMPILE) rgb_tester.o rgb_function.o -o $(app) --sysroot=$(SROOT) -pthread
rgb_function.o: rgb_function.c
	$(CROSS_COMPILE) -Wall -c -o rgb_function.o --sysroot=$(SROOT) rgb_function.c
rgb_tester.o: rgb_tester.c
	$(CROSS_COMPILE) -Wall -pthread -c -o rgb_tester.o --sysroot=$(SROOT) rgb_tester.c



clean:
	rm -f *.o
	rm -f $(app)
