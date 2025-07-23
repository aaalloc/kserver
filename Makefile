KDIR ?= /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

# operation.h size buf
SIZE_BUF_IO := 1024

# Include paths for the src structure
ccflags-y += -Wall -g -O3 -DSIZE_BUF_IO=$(SIZE_BUF_IO)
ccflags-y += -I$(PWD)/src

ifeq ($(RDTSC_ENABLED), y)
ccflags-y += -DRDTSC_ENABLED
endif

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

wq_insert_exec-y := src/wq_insert_exec.o

wq_exec_time_pred-y := src/wq_exec_time_pred.o
wq_exec_time_pred-y += src/eat_time.o

wq_new_worker-y := src/wq_new_worker.o
wq_new_worker-y += src/eat_time.o

matrix_time_measurement-y := src/matrix_time_measurement.o

obj-m := kserver.o wq_insert_exec.o wq_exec_time_pred.o wq_new_worker.o matrix_time_measurement.o
all: default

default:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

kserver-only:
	$(MAKE) -C $(KDIR) M=$(PWD) kserver.ko

wq-insert-exec:
	$(MAKE) -C $(KDIR) M=$(PWD) wq_insert_exec.ko

wq-exec-time-pred:
	$(MAKE) -C $(KDIR) M=$(PWD) wq_exec_time_pred.ko


wq-new-worker:
	$(MAKE) -C $(KDIR) M=$(PWD) wq_new_worker.ko

matrix-time-measurement:
	$(MAKE) -C $(KDIR) M=$(PWD) matrix_time_measurement.ko

bear:
	bear --output $(PWD)/.vscode/compile_commands.json -- $(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean


stresstest: stresstest.c
	gcc -Wall -g -O3 -o stresstest stresstest.c -lpthread