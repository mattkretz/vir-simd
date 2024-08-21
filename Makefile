build_dir := $(shell which $(CXX))
tmp := "case $$(readlink -f $(build_dir)) in *icecc) which $${ICECC_CXX:-g++};; *) echo $(build_dir);; esac"
build_dir := $(shell sh -c $(tmp))
build_dir := $(realpath $(build_dir))
build_dir := build-$(subst /,-,$(build_dir:/%=%))

triplet := $(shell $(CXX) -dumpmachine)

CXXFLAGS+=-Wall -Wextra -Wpedantic -Wno-attributes -Wno-unknown-pragmas -Wno-psabi
ifneq ($(findstring GCC,$(shell $(CXX) --version)),)
	CXXFLAGS+=-static-libstdc++
else ifneq ($(findstring clang,$(shell $(CXX) --version)),)
	# Avoid warning on self-assignment tests
	CXXFLAGS+=-Wno-self-assign-overloaded
	# Avoid warning about PCH inclusion method
	CXXFLAGS+=-Wno-deprecated
endif
CXXFLAGS+=-I$(PWD)
prefix=/usr/local
includedir=$(prefix)/include

srcdir=.
testdir=testsuite/$(build_dir)-O2
testdirOs=testsuite/$(build_dir)-Os
testdirext=testsuite/$(build_dir)-ext
sim=

ifneq ($(findstring 86,$(triplet)),)
	testflags=-march=native
else ifneq ($(findstring wasm,$(triplet)),)
	testflags=
endif

std=$(shell env CXX=$(CXX) ./latest_std_flag.sh)

check: check-extensions check-constexpr_wrapper testsuite-O2 testsuite-Os

debug:
	@echo "build_dir: $(build_dir)"
	@echo "triplet: $(triplet)"
	@echo "testflags: $(testflags)"
	@echo "DRIVEROPTS: $(DRIVEROPTS)"
	@echo "CXX: $(CXX)"
	@echo "CXXFLAGS: $(CXXFLAGS)"
	@echo "std: $(std)"

testsuite/$(build_dir)-%/Makefile: $(srcdir)/testsuite/generate_makefile.sh Makefile
	@rm -f $@
	@echo "Generating simd testsuite ($*) subdirs and Makefiles ..."
	@$(srcdir)/testsuite/generate_makefile.sh --destination="testsuite/$(build_dir)-$*" --sim="$(sim)" --testflags="$(testflags)" $(CXX) -$* -std=$(std) $(CXXFLAGS) -DVIR_DISABLE_STDX_SIMD -DVIR_SIMD_TS_DROPIN
	@test -e ../vc-testdata/reference-sincos-dp.dat && dir=$$PWD && cd "testsuite/$(build_dir)-$*" && ln -s $$dir/../vc-testdata/*.dat .

$(testdirext)/Makefile: $(srcdir)/testsuite/generate_makefile.sh Makefile
	@rm -f $@
	@echo "Generating simd testsuite subdirs and Makefiles ..."
	@mkdir -p $(testdirext)
	@echo for_each.cc > $(testdirext)/testsuite_files_simd
	@echo transform.cc >> $(testdirext)/testsuite_files_simd
	@echo transform_reduce.cc >> $(testdirext)/testsuite_files_simd
	@cd $(testdirext) && ../generate_makefile.sh --destination="." --sim="$(sim)" --testflags="-O2 $(testflags)" $(CXX) -std=$(std) $(CXXFLAGS) -DVIR_SIMD_TS_DROPIN
	@test -e ../vc-testdata/reference-sincos-dp.dat && dir=$$PWD && cd $(testdirext) && ln -s $$dir/../vc-testdata/*.dat .

testsuite-%: testsuite/$(build_dir)-%/Makefile
	@rm -f testsuite/$(build_dir)-$*/.simd.summary
	@$(MAKE) -C "testsuite/$(build_dir)-$*"
	@tail -n20 testsuite/$(build_dir)-$*/simd_testsuite.sum | grep -A20 -B1 'Summary ===' >> testsuite/$(build_dir)-$*/.simd.summary
	@cat testsuite/$(build_dir)-$*/.simd.summary && rm testsuite/$(build_dir)-$*/.simd.summary

install:
	install -d $(includedir)/vir
	install -m 644 -t $(includedir)/vir vir/*.h

check-extensions: check-extensions-stdlib check-extensions-fallback

check-extensions-stdlib:
	@echo "$(std): test extensions ($(CXXFLAGS))"
	@$(CXX) -O2 -std=$(std) -Wall -Wextra $(CXXFLAGS) -S vir/test.cpp -o test.S

check-extensions-fallback:
	@echo "$(std): test extensions ($(CXXFLAGS) -DVIR_DISABLE_STDX_SIMD)"
	@$(CXX) -O2 -std=$(std) -Wall -Wextra $(CXXFLAGS) -DVIR_DISABLE_STDX_SIMD -S vir/test.cpp -o test.S

check-constexpr_wrapper:
	@echo "gnu++2a: test constexpr_wrapper ($(CXXFLAGS))"
	@$(CXX) -O2 -std=gnu++2a -Wall -Wextra $(CXXFLAGS) -S vir/test_constexpr_wrapper.cpp -o test.S
	@if $(CXX) -std=gnu++2b -c /dev/null 2>/dev/null; then \
		echo "gnu++2b: test constexpr_wrapper ($(CXXFLAGS))"; \
		$(CXX) -O2 -std=gnu++2b -Wall -Wextra $(CXXFLAGS) -S vir/test_constexpr_wrapper.cpp -o test.S; \
	fi

run-%: $(testdir)/Makefile
	@$(MAKE) -C "$(testdir)" run-$*

run-Os-%: $(testdirOs)/Makefile
	@$(MAKE) -C "$(testdirOs)" run-$*

run-ext-%: $(testdirext)/Makefile
	@$(MAKE) -C "$(testdirext)" run-$*

clean:
	@rm -rf testsuite/$(build_dir)-*

help: $(testdir)/Makefile $(testdirOs)/Makefile $(testdirext)/Makefile
	@echo "... check"
	@echo "... testsuite-O2"
	@echo "... testsuite-Os"
	@echo "... testsuite-ext"
	@echo "... check-extensions"
	@echo "... check-extensions-stdlib"
	@echo "... check-extensions-fallback"
	@echo "... check-constexpr_wrapper"
	@echo "... clean"
	@echo "... install (using prefix=$(prefix))"
	@$(MAKE) -C "$(testdir)" help
	@$(MAKE) -C "$(testdirOs)" help|sed 's/run-/run-Os-/g'
	@$(MAKE) -C "$(testdirext)" help|sed 's/run-/run-ext-/g'

.PHONY: check install check-extensions check-extensions-stdlib check-extensions-fallback clean help testsuite
