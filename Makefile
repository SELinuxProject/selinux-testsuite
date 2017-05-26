SUBDIRS = policy tests 

all:
	@set -e; for i in $(SUBDIRS); do $(MAKE) -C $$i all ; done

test:
	make -C policy load
	make -C tests test
	make -C policy unload

check-syntax:
	@./tools/check-syntax

clean:
	@set -e; for i in $(SUBDIRS); do $(MAKE) -C $$i clean ; done


