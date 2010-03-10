#
# Amazon SimpleDB C bindings
#
# Created by Peter Macko
#
# Copyright 2009
#      The President and Fellows of Harvard College.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
# 3. Neither the name of the University nor the names of its contributors
#    may be used to endorse or promote products derived from this software
#    without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE UNIVERSITY AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE UNIVERSITY OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
#

#include mk/header.mk
include mk/defs.mk

#
# Commands
#

DEPEND := $(CC) -MM $(COMPILER_FLAGS) $(INCLUDE_FLAGS) $(DEBUG_FLAGS)
COMPILE := $(CC) -c $(COMPILER_FLAGS) $(INCLUDE_FLAGS) $(DEBUG_FLAGS)
LINK := $(CC) $(LINKER_FLAGS) $(DEBUG_FLAGS)

COMPILE := $(strip $(COMPILE))
LIBRARIES := $(strip $(LIBRARIES))
LINK := $(strip $(LINK))


#
# Preprocess the input data
#

SOURCES := $(sort $(SOURCES)) 
OBJECTS := $(patsubst %.c,%.o,$(SOURCES))
OBJECTS := $(patsubst %,$(BUILD_DIR)/%,$(sort $(OBJECTS)))

PROG_SOURCES := $(sort $(PROG_SOURCES)) 
PROG_OBJECTS := $(patsubst %.c,%.o,$(PROG_SOURCES))
PROG_OBJECTS := $(patsubst %,$(BUILD_DIR)/%,$(sort $(PROG_OBJECTS)))

ALL_SOURCES := $(sort $(PROG_SOURCES) $(SOURCES))
ALL_OBJECTS := $(patsubst %.c,%.o,$(ALL_SOURCES))
ALL_OBJECTS := $(patsubst %,$(BUILD_DIR)/%,$(sort $(ALL_OBJECTS)))


#
# Phony and preliminary targets
#

.PHONY: all clean distclean messclean lines depend

ifdef TEST_TARGET
BUILD_DIR_TEST_TARGET := $(BUILD_DIR)/$(TEST_TARGET)
endif

all: $(BUILD_DIR)/$(TARGET) $(BUILD_DIR_TEST_TARGET)

clean distclean: messclean
	@rm -rf project.mk $(BUILD_DIR)

messclean:
	@find . -name '*~' -delete 2> /dev/null
	@rm -rf *.db 2> /dev/null
	@rm -rf __db.* log.* 2> /dev/null
	
$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

lines:
	@echo Total number of lines:
	@cat Makefile *.c *.h | wc -l


#
# Compiler targets & dependencies
#

depend: $(BUILD_DIR)/project.mk

$(BUILD_DIR)/project.mk: $(ALL_SOURCES) *.h
	@echo "Remaking project.mk"
	@rm -f $@ 2> /dev/null
	@echo '.PHONY: all' >> $@
	@echo 'all: $(ALL_OBJECTS)' >> $@
	@echo >> $@
	@for S in $(ALL_SOURCES); do \
		$(DEPEND) $$S | sed 's|^[a-zA-Z0-9]*\.o|$(BUILD_DIR)\/&|' >> $@; \
		if [[ $${PIPESTATUS[0]} -ne 0 ]]; then exit 1; fi; \
		echo '	$(COMPILE) -o $$@ $$<' >> $@; \
		echo >> $@; \
	done

$(ALL_OBJECTS): $(BUILD_DIR)/project.mk $(ALL_SOURCES)
	@$(MAKE) --no-print-directory -f $(BUILD_DIR)/project.mk $@ \
		| grep -v 'is up to date' \
		| grep -v 'Nothing to be done for' \
		; exit $${PIPESTATUS[0]}
