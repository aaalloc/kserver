KDIR ?= /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

# operation.h size buf
SIZE_BUF_IO := 1024

# Include paths for the src structure
ccflags-y += -Wall -g -O3 -DSIZE_BUF_IO=$(SIZE_BUF_IO)
ccflags-y += -I$(PWD)/src

TARGET = kserver

# Main source file
kserver-y := src/main.o

# Library files
kserver-y += src/ksocket_handler.o
kserver-y += src/operations.o
kserver-y += src/task.o

# Scenario files
kserver-y += src/mom.o
kserver-y += src/only_cpu.o

obj-m := kserver.o

all: default

default:
	$(MAKE) -C $(KDIR) M=$(PWD) modules


kserver-only:
	$(MAKE) -C $(KDIR) M=$(PWD) kserver.ko

wq-only:
	$(MAKE) -C $(KDIR) M=$(PWD) wq_insert_exec.ko

bear:
	bear --output $(PWD)/.vscode/compile_commands.json -- $(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean


stresstest: stresstest.c
	gcc -Wall -g -O3 -o stresstest stresstest.c -lpthread