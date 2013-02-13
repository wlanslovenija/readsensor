all: readsensor

readsensor: main.o
	$(CC) $(LDFLAGS) -o $@ -lrt $^

%.o: %.c
	$(CC) $(CFLAGS) $(EXTRA_CFLAGS) -c -I. -o $@ $<

clean:
	rm -rf *.o readsensor

