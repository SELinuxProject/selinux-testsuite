TARGETS=atsecure good evil.so

all: $(TARGETS)

evil.so: evil.c
	$(CC) -shared -o $@ -fPIC $<

clean:
	rm -f $(TARGETS)
