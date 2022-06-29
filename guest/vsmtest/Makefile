PREFIX = aarch64-linux-gnu-
CC = $(PREFIX)gcc
LD = $(PREFIX)ld
OBJCOPY = $(PREFIX)objcopy

TARGET = vsmtest

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
NCPU = 2
endif

QEMUOPTS = -cpu $(QCPU) -machine $(MACHINE) -smp $(NCPU) -m 256
QEMUOPTS += -global virtio-mmio.force-legacy=false
QEMUOPTS += -nographic -kernel $(TARGET)

OBJS = boot.o main.o

all: vsmtest.img

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.S
	$(CC) $(CFLAGS) -c $< -o $@

-include: *.d

$(TARGET): $(OBJS) memory.ld
	$(LD) $(LDFLAGS) -T memory.ld -o $@ $(OBJS)

vsmtest.img: $(TARGET)
	$(OBJCOPY) -O binary $^ $@
	cp $@ ../

qemu: $(TARGET)
	$(QEMU) $(QEMUOPTS)

clean:
	$(RM) $(OBJS) $(TARGET) vsmtest.img *.d

.PHONY: qemu clean all
