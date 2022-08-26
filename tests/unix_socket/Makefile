TARGETS=client server socketpair

LDLIBS+= -lselinux

all: $(TARGETS)
clean:
	rm -f $(TARGETS) flag test_client_sock test_server_sock
