KDIR ?= /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

# operation.h size buf
SIZE_BUF_IO := 1024

# Include paths for the src structure
ccflags-y += -Wall -g -O3 -DSIZE_BUF_IO=$(SIZE_BUF_IO)
ccflags-y += -I$(PWD)/src/lib -I$(PWD)/src/scenario

TARGET = kserver

# Main source file
kserver-y := src/main.o

# Library files
kserver-y += src/lib/ksocket_handler.o
kserver-y += src/lib/operations.o
kserver-y += src/lib/task.o

# Scenario files
kserver-y += src/scenario/mom.o

obj-m := kserver.o

all: default

default:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

bear:
	bear --output $(PWD)/.vscode/compile_commands.json -- $(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean


stresstest: stresstest.c
	gcc -Wall -g -O3 -o stresstest stresstest.c -lpthread