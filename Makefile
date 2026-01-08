.PHONY: help debug release dist clean deps coconut

help:
	@echo "Targets:"
	@echo "  debug  : Generate build artifacts for a debug build in build-debug"
	@echo "  release: Generate build artifacts for a release build in build-release"
	@echo "  dist   : Pack civicc and coconut into a tar.gz file. Use this for creating a submission"
	@echo "  clean  : Remove all build directories and created dist files"

coconut:
	make -C coconut

# Change to @cmake -DDISABLE_ASAN=true -DCMAKE_BUILD_TYPE=... to disable address
# sanitizer
debug: coconut
	@cmake -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -S ./ -B build-$@/

release: coconut
	@cmake -DCMAKE_BUILD_TYPE=Release -S ./ -B build-$@/


dist:
	bash scripts/dist.sh

clean:
	rm -f *.tar*
	rm -rf build*/

###############################################################################
#
# Testing
#

check:
	@cd test; \
	CIVAS=../$(TEST_CIVAS) \
	CIVVM=../$(TEST_CIVVM) \
	CIVCC=../$(TEST_CIVCC) \
	RUN_FUNCTIONAL=$(TEST_RUN_FUNCTIONAL) \
	bash run.bash $(TEST_DIRS)

###############################################################################
#
# Testing
#
#  Uncomment the necessary parts when adding extensions.

# Subdirectories of "test/" to traverse into during testing. Enable
# "nested_funs" and "arrays" when you start implementing those extensions.
# "preprocess" is optional, for those who implemented the C preprocessor.
TEST_DIRS := basic # nested_funs arrays preprocess

# Flags to pass to your compiler when running tests (e.g., a verbosity level).
TEST_CFLAGS := ""

# Whether to run tests that compare program output. Set to 1 after you have
# implemented your assembly phase.
TEST_RUN_FUNCTIONAL := 0

# Path to toolchain files, relative to the root directory (used for functional
# test).
TEST_CIVAS := ../toolchain/civas
TEST_CIVVM := ../toolchain/civvm

# Path to your compiler binary, relative to the root directory.
TEST_CIVCC := build-debug/civicc
