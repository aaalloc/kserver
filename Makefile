KDIR ?= /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

ccflags-y += -Wall -g

TARGET = kserver

kserver-y := main.o
kserver-y += ksocket_handler.o
kserver-y += operations.o

obj-m := kserver.o

all: default

default:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

bear:
	bear --output $(PWD)/.vscode/compile_commands.json -- $(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
