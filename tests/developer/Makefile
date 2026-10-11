TOOLBIN ?= /root/native/bin
CRT_DIR ?= /usr/lib
LIB_DIR ?= /root/native/lib
LD_SCRIPT ?= /lib/myemulator2-user.ld

CC := $(TOOLBIN)/chibicc
AS := $(TOOLBIN)/as
LD := $(TOOLBIN)/ld

.PHONY: all clean run
all: hello

hello.s: hello.c
	$(CC) -S -o $@ $<


hello.o: hello.s
	$(AS) -o $@ $<

hello: hello.o
	$(LD) -T $(LD_SCRIPT) -o $@ $(CRT_DIR)/crt1.o $(CRT_DIR)/crti.o $< \
		-L$(CRT_DIR) -L$(LIB_DIR) --start-group -lc $(LIB_DIR)/libgcc.a --end-group \
		$(CRT_DIR)/crtn.o

run: hello
	./hello

clean:
	rm -f hello.s hello.o hello
