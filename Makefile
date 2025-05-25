SRC_DIR := tests
BIN_DIR := bin
ifneq ($(KERNELRELEASE),)
	obj-m := my_buffer.o
else
	KERNELDIR ?= /lib/modules/$(shell uname -r)/build
	PWD := $(shell pwd)
default:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules
clean:
	$(MAKE) -C $(KERNEL_PATH) M=$(PWD) clean
$(BIN_DIR)/%: $(SRC_DIR)/%.c
	gcc $< -o $@
build_tests:$(BIN_DIR)/test1 $(BIN_DIR)/test2 $(BIN_DIR)/test3 $(BIN_DIR)/test4 $(BIN_DIR)/test5
	
tests_clean:
	rm -rf bin/*

endif
