TARGETS=create_modify_qp

LDLIBS+= -libverbs

RESULT=$(shell grep "SELINUX_INFINIBAND_PKEY_TEST=0" ./ibpkey_test.conf)

ifeq  ($(RESULT), )
    all: $(TARGETS)
else
    all:
	@echo "Infiniband test disabled"
endif

clean:
	rm -f $(TARGETS)
