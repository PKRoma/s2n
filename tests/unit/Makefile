#
# Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License").
# You may not use this file except in compliance with the License.
# A copy of the License is located at
#
#  http://aws.amazon.com/apache2.0
#
# or in the "license" file accompanying this file. This file is distributed
# on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
# express or implied. See the License for the specific language governing
# permissions and limitations under the License.
#

SRCS=$(wildcard *.c)
OBJS=$(SRCS:.c=.o)
TESTS=$(SRCS:.c=)
CRYPTO_LDFLAGS = -L$(LIBCRYPTO_ROOT)/lib

# Users can specify a subset of tests to be run, otherwise run all tests.
ifeq (,$(strip ${UNIT_TESTS}))
	UNIT_TESTS := ${TESTS}
endif

.PHONY : all
.PRECIOUS : $(TESTS)

all: $(UNIT_TESTS)

include ../../s2n.mk

CRUFT += $(wildcard *_test)
LIBS += ../testlib/libtests2n.a ../../lib/libs2n.a

# Suppress the unreachable code warning, because tests involve what should be
# unreachable code
CFLAGS += -Wno-unreachable-code -I../
LDFLAGS += ${CRYPTO_LDFLAGS} ${LIBS} ${CRYPTO_LIBS} -lm -ldl

ifdef S2N_ADDRESS_SANITIZER
$(UNIT_TESTS)::
	@${CC} ${CFLAGS} -o $@ $@.c ${LDFLAGS} 2>&1
	@DYLD_LIBRARY_PATH="$(LIBCRYPTO_ROOT)/lib:$$DYLD_LIBRARY_PATH" \
	LD_LIBRARY_PATH="$(LIBCRYPTO_ROOT)/lib:$$LD_LIBRARY_PATH" \
	./$@
else
$(UNIT_TESTS)::
	@${CC} ${CFLAGS} -o $@ $@.c ${LDFLAGS} 2>&1
	@DYLD_LIBRARY_PATH="$(LIBCRYPTO_ROOT)/lib:$$DYLD_LIBRARY_PATH" \
	LD_LIBRARY_PATH="$(LIBCRYPTO_ROOT)/lib:$$LD_LIBRARY_PATH" \
	LD_PRELOAD="../LD_PRELOAD/allocator_overrides.so" \
	./$@
endif

.PHONY : clean
clean: decruft
	@$(foreach test, $(TESTS), rm -f -- "${test}";)

