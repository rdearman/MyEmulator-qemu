.PHONY: all clean

all: hello

hello.s: hello.c
	/root/native/bin/chibicc -S -o $@ $<

hello.o: hello.s
	/root/native/bin/as -o $@ $<

start.o: start.S
	/root/native/bin/as -o $@ $<

hello: hello.o start.o
	/root/native/bin/ld -T /root/native/lib/myemulator2-user.ld -o $@ hello.o start.o

clean:
	rm -f hello.s hello.o start.o hello
