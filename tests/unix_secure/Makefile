ctargets = server client checksyscalls

selinuxhome = /usr/local/selinux
LDFLAGS = -L$(selinuxhome)/lib
LDLIBS = -lsecure
CFLAGS = -I$(selinuxhome)/include
CFLAGS+=-Wall

all: $(ctargets)
clean:
	rm -f $(ctargets) *.o *~
