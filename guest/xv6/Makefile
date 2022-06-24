PREFIX = aarch64-linux-gnu-
CC = $(PREFIX)gcc
LD = $(PREFIX)ld
OBJCOPY = $(PREFIX)objcopy

QEMU = qemu-system-aarch64
CPUS = 1

TARGET = kernel

QEMUOPTS = -cpu cortex-a72 -machine virt,gic-version=3 -kernel $(TARGET) -m 128M -smp $(CPUS) -nographic
QEMUOPTS += -drive file=fs.img,if=none,format=raw,id=x0
QEMUOPTS += -device virtio-blk-device,drive=x0,bus=virtio-mmio-bus.0

kernel.img: $(TARGET)
	$(OBJCOPY) -O binary $^ $@

qemu: kernel fs.img
	$(QEMU) $(QEMUOPTS)

clean:
	$(RM) kernel.img
