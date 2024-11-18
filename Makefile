SUBDIRS = policy tests 

all:
	@set -e; for i in $(SUBDIRS); do $(MAKE) -C $$i all ; done

test:
	$(MAKE) -C policy load
	$(MAKE) -C tests test
	$(MAKE) -C policy unload

check-syntax:
	@./tools/check-syntax

clean:
	@set -e; for i in $(SUBDIRS); do $(MAKE) -C $$i clean ; done
