# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright © 2022–2024 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH
#                       Matthias Kretz <m.kretz@gsi.de>

# Tests for vir-simd extensions to std::experimental::simd
ext_tests = for_each \
	    transform \
	    transform_reduce

# Tests for Parallelism TS 2 compliance
simd_tests = $(filter-out $(ext_tests),$(patsubst testsuite/tests/%.cc,%,$(wildcard testsuite/tests/*.cc)))

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
mandir=$(prefix)/share/man

srcdir=.
testdir=testsuite/$(build_dir)-O2
testdirext=testsuite/$(build_dir)-ext-O2
testdirextOs=testsuite/$(build_dir)-ext-Os
sim=

ifneq ($(findstring 86,$(triplet)),)
	testflags=-march=native
else ifneq ($(findstring wasm,$(triplet)),)
	testflags=
endif

std=$(shell env CXX=$(CXX) ./latest_std_flag.sh)

uses_stdx_simd=$(findstring define VIR_GLIBCXX_STDX_SIMD 1,$(shell $(CXX) $(CXXFLAGS) -x c++ -std=$(std) -o- -dM -E vir/detail.h))
stdlib_version=$(shell $(CXX) $(CXXFLAGS) -x c++ -std=$(std) -o- -E stdlibtest.cpp | tail -n1)

version:=$(shell grep '\<VIR_SIMD_VERSION\>' vir/simd_version.h | \
	sed -e 's/.*0x/0x/' -e 's/'\''//g' | \
	(read v; echo $$((v/0x10000)).$$((v%0x10000/0x100)).$$((v%0x100));))

version_patchlevel:=$(lastword $(subst ., ,$(version)))

version_info=$(shell test $$(($(version_patchlevel)&1)) -eq 1 && echo development || \
	     (test $(version_patchlevel) -lt 190 && echo release || \
	     (test $(version_patchlevel) -lt 200 && echo alpha $$(($(version_patchlevel)/2-94)) || \
	     echo beta $$(($(version_patchlevel)/2-99)))))

cxx_version := $(shell $(CXX) --version | head -n1)

print_compiler_info:
	@echo "-----------+-----------------------------------------------------------------"
	@echo "  Compiler | $(CXX) $(ICECC_CXX)"
	@echo "           | $(cxx_version)"
	@echo "    stdlib | $(stdlib_version)"
	@echo "  vir-simd | $(version) ($(version_info)) $(if $(uses_stdx_simd),using stdx::simd from libstdc++,using vir::stdx::simd fallback)"
	@echo "DRIVEROPTS | $(DRIVEROPTS)"
	@echo "  CXXFLAGS | -std=$(std) $(CXXFLAGS)"
	@echo "-----------+-----------------------------------------------------------------"

.PHONY: print_compiler_info

docs/man/man3vir/simdize.h.3vir docs/html/index.html: Doxyfile vir/*.h docs/cppreference.xml Makefile
	@sed -i 's/^PROJECT_NUMBER *= ".*"/PROJECT_NUMBER = "$(version)"/' $<
	@rm -rf docs/html docs/man docs/latex
	@doxygen
	cd docs/man/man3vir && \
		for i in *'_ T, N _.3vir'; do mv "$$i" "$${i%_ T, N _.3vir}.3vir"; done && \
		for i in vir_detail_*; do mv "$$i" "vir::detail::$${i#vir_detail_}"; done && \
		for i in vir_execution_*; do mv "$$i" "vir::execution::$${i#vir_execution_}"; done && \
		for i in vir_*; do mv "$$i" "vir::$${i#vir_}"; done

docs: docs/html/index.html

.PHONY: docs

# Always run the testsuite on extensions to stdx::simd.
check: print_compiler_info check-extensions check-constexpr_wrapper testsuite-ext-O2 testsuite-ext-Os

# If the default of vir-simd is to use the fallback implementation, then run the simd testsuite on it.
ifeq ($(uses_stdx_simd),)
check: testsuite-O2
endif

debug:
	@echo "vir::simd_version: $(version) ($(version_info))"
	@echo "build_dir: $(build_dir)"
	@echo "triplet: $(triplet)"
	@echo "testflags: $(testflags)"
	@echo "DRIVEROPTS: $(DRIVEROPTS)"
	@echo "CXX: $(CXX)"
	@echo "stdlib: $(stdlib_version)"
	@echo "CXXFLAGS: $(CXXFLAGS)"
	@echo "std: $(std)"
	@echo "uses_stdx_simd: $(uses_stdx_simd)"

getflag = -$(subst ext-,,$1)
disable_stdx_simd_flag = $(if $(findstring ext-,$1),,-DVIR_DISABLE_STDX_SIMD)

testsuite/$(build_dir)-%/:
	@mkdir -p $(dir $@)

testsuite/$(build_dir)-%/Makefile: testsuite/$(build_dir)-%/ $(srcdir)/testsuite/generate_makefile.sh Makefile
	$(file >$(dir $@)testsuite_files_simd)
	$(foreach t,$(if $(findstring ext-,$*),$(ext_tests),$(simd_tests)),$(file >>$(dir $@)testsuite_files_simd,$t.cc))
	@rm -f $@
	@echo "Generating simd testsuite ($*) subdirs and Makefiles ..."
	@cd $(dir $@) && ../generate_makefile.sh --destination=. --sim="$(sim)" --testflags="$(testflags)" $(CXX) -std=$(std) $(CXXFLAGS) $(call getflag,$*) $(call disable_stdx_simd_flag,$*) -DVIR_SIMD_TS_DROPIN
	@if test -e ../vc-testdata/reference-sincos-dp.dat; then \
		dir=$$PWD && \
		cd "testsuite/$(build_dir)-$*" && \
		ln -sf $$dir/../vc-testdata/*.dat .; \
	fi

testsuite-%: testsuite/$(build_dir)-%/Makefile
	@rm -f testsuite/$(build_dir)-$*/.simd.summary
	@$(MAKE) -C "testsuite/$(build_dir)-$*"
	@tail -n20 testsuite/$(build_dir)-$*/simd_testsuite.sum | grep -A20 -B1 'Summary ===' >> testsuite/$(build_dir)-$*/.simd.summary
	@cat testsuite/$(build_dir)-$*/.simd.summary && rm testsuite/$(build_dir)-$*/.simd.summary

install:
	@echo "Installing vir::stdx::simd $(version) ($(version_info)) to $(includedir)/vir"
	install -d $(includedir)/vir
	install -m 644 -t $(includedir)/vir vir/*.h

install-man: docs/man/man3vir/simdize.h.3vir
	@echo "Installing vir::stdx::simd $(version) ($(version_info)) man pages to $(mandir)"
	install -d $(mandir)/man3vir
	install -m 644 -t $(mandir)/man3vir docs/man/man3vir/vir::*.3vir
	install -m 644 -t $(mandir)/man3vir docs/man/man3vir/*.h.3vir

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

run-ext-O2-%: $(testdirext)/Makefile
	@$(MAKE) -C "$(testdirext)" run-$*

run-ext-Os-%: $(testdirext)/Makefile
	@$(MAKE) -C "$(testdirextOs)" run-$*

clean:
	@rm -rf testsuite/$(build_dir)-*

help: $(testdir)/Makefile $(testdirext)/Makefile $(testdirextOs)/Makefile
	@echo "... check"
	@echo "... testsuite-O2"
	@echo "... testsuite-ext-O2"
	@echo "... testsuite-ext-Os"
	@echo "... check-extensions"
	@echo "... check-extensions-stdlib"
	@echo "... check-extensions-fallback"
	@echo "... check-constexpr_wrapper"
	@echo "... docs"
	@echo "... clean"
	@echo "... install (using prefix=$(prefix))"
	@echo "... install-man (using prefix=$(prefix))"
	@$(MAKE) -C "$(testdir)" help
	@$(MAKE) -C "$(testdirext)" help|sed 's/run-/run-ext-O2-/g'
	@$(MAKE) -C "$(testdirextOs)" help|sed 's/run-/run-ext-Os-/g'

.PHONY: check install check-extensions check-extensions-stdlib check-extensions-fallback clean help testsuite
