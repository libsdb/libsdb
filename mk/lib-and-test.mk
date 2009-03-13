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

PROG_SOURCES := $(PROG_SOURCES) $(TEST_SOURCES)

include mk/compile.mk


#
# Phony targets
#

.PHONY: run-dev run vg valgrind gdb vg-dev vg-all install uninstall

run-dev: all
	@$(BUILD_DIR)/$(TEST_TARGET) $(RUN_DEV_ARGS)

run: all
	@$(BUILD_DIR)/$(TEST_TARGET)

vg valgrind: all
	@valgrind --tool=memcheck --leak-check=yes --num-callers=24 $(BUILD_DIR)/$(TEST_TARGET)

vg-dev: all
	@valgrind --tool=memcheck --leak-check=yes --num-callers=24 $(BUILD_DIR)/$(TEST_TARGET) $(RUN_DEV_ARGS)

gdb: all
	@gdb @$(BUILD_DIR)/$(TEST_TARGET)

vg-all: all
	@valgrind --tool=memcheck --leak-check=yes --num-callers=24 --show-reachable=yes $(BUILD_DIR)/$(TEST_TARGET)

install: all
	@find include -name '*~' -delete 2> /dev/null
	@for S in $(shell ls -1 include/*.h*); do		\
		echo install -m 644 $$S /usr/include;		\
		install -m 644 $$S /usr/include;			\
	done
	install -m 644 $(BUILD_DIR)/$(TARGET) /usr/lib
	install -m 755 $(CONFIG_SCRIPT) /usr/bin

uninstall:
	@for S in $(shell ls -1 include/*.h*); do		\
		echo rm -f /usr/$$S;						\
		rm -f /usr/$$S;								\
	done
	rm -f /usr/lib/$(TARGET)
	rm -f /usr/bin/$(CONFIG_SCRIPT)


#
# Linker targets
#

$(BUILD_DIR)/$(TARGET): $(BUILD_DIR) $(OBJECTS)
	@rm -f $@ 2> /dev/null
	ar cq $@ $(OBJECTS)
	ranlib $@

$(BUILD_DIR)/$(TEST_TARGET): $(BUILD_DIR) $(OBJECTS) $(PROG_OBJECTS)
	$(LINK) -o $@ $(OBJECTS) $(PROG_OBJECTS) $(LIBRARIES)
