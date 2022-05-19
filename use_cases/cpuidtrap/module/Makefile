obj-m += fixcpuid.o
ccflags-y += -Wno-unused-result
all: fixcpuid.c 
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules
clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
	

