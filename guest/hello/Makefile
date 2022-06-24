PREFIX = aarch64-linux-gnu-
CC = $(PREFIX)gcc
LD = $(PREFIX)ld
OBJCOPY = $(PREFIX)objcopy

TARGET = hello

CPU = cortex-a72
QCPU = cortex-a72

CFLAGS = -Wall -Og -g -MD -ffreestanding -nostdinc -nostdlib -nostartfiles -mcpu=$(CPU)
CFLAGS += -I ./include/
LDFLAGS = -nostdlib -nostartfiles

QEMUPREFIX =
QEMU = $(QEMUPREFIX)qemu-system-aarch64
GIC_VERSION = 3
MACHINE = virt,gic-version=$(GIC_VERSION)
ifndef NCPU
NCPU = 1
endif

QEMUOPTS = -cpu $(QCPU) -machine $(MACHINE) -smp $(NCPU) -m 256
QEMUOPTS += -global virtio-mmio.force-legacy=false
QEMUOPTS += -nographic -kernel $(TARGET)

OBJS = hello.o boot.o uart.o gicv3.o vector.o timer.o

all: hello.img

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.S
	$(CC) $(CFLAGS) -c $< -o $@

-include: *.d

$(TARGET): $(OBJS) memory.ld
	$(LD) $(LDFLAGS) -T memory.ld -o $@ $(OBJS)

hello.img: $(TARGET)
	$(OBJCOPY) -O binary $^ $@

qemu: $(TARGET)
	$(QEMU) $(QEMUOPTS)

clean:
	$(RM) $(OBJS) $(TARGET) hello.img *.d

.PHONY: qemu clean all
