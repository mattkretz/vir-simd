CXXFLAGS+=-Wno-attributes -Wno-unknown-pragmas
ifneq ($(findstring GCC,$(shell $(CXX) --version)),)
	CXXFLAGS+=-static-libstdc++
endif
CXXFLAGS+=-I$(PWD)
prefix=/usr/local
includedir=$(prefix)/include

srcdir=.
testdir=testsuite/build-O2
testdirOs=testsuite/build-Os
testdirext=testsuite/build-ext
sim=
testflags=-march=native

check: check-extensions testsuite-O2 testsuite-Os

testsuite/build-%/Makefile: $(srcdir)/testsuite/generate_makefile.sh Makefile
	@rm -f $@
	@echo "Generating simd testsuite ($*) subdirs and Makefiles ..."
	@$(srcdir)/testsuite/generate_makefile.sh --destination="testsuite/build-$*" --sim="$(sim)" --testflags="$(testflags)" $(CXX) -$* -std=c++2a $(CXXFLAGS) -DVIR_DISABLE_STDX_SIMD -DVIR_SIMD_TS_DROPIN

$(testdirext)/Makefile: $(srcdir)/testsuite/generate_makefile.sh Makefile
	@rm -f $@
	@echo "Generating simd testsuite subdirs and Makefiles ..."
	@mkdir -p $(testdirext)
	@echo for_each.cc > $(testdirext)/testsuite_files_simd
	@echo transform.cc >> $(testdirext)/testsuite_files_simd
	@cd $(testdirext) && ../generate_makefile.sh --destination="." --sim="$(sim)" --testflags="-O2 $(testflags)" $(CXX) -std=gnu++2a $(CXXFLAGS) -DVIR_SIMD_TS_DROPIN

testsuite-%: testsuite/build-%/Makefile
	@rm -f testsuite/build-$*/.simd.summary
	@$(MAKE) -C "testsuite/build-$*"
	@tail -n20 testsuite/build-$*/simd_testsuite.sum | grep -A20 -B1 'Summary ===' >> testsuite/build-$*/.simd.summary
	@cat testsuite/build-$*/.simd.summary && rm testsuite/build-$*/.simd.summary

install:
	install -d $(includedir)/vir
	install -m 644 -t $(includedir)/vir vir/*.h

check-extensions: check-extensions-stdlib check-extensions-fallback

check-extensions-stdlib:
	$(CXX) -O2 -std=gnu++2a -Wall -Wextra $(CXXFLAGS) -S vir/test.cpp -o test.S

check-extensions-fallback:
	$(CXX) -O2 -std=gnu++2a -Wall -Wextra $(CXXFLAGS) -DVIR_DISABLE_STDX_SIMD -S vir/test.cpp -o test.S

run-%: $(testdir)/Makefile
	@$(MAKE) -C "$(testdir)" run-$*

run-Os-%: $(testdirOs)/Makefile
	@$(MAKE) -C "$(testdirOs)" run-$*

run-ext-%: $(testdirext)/Makefile
	@$(MAKE) -C "$(testdirext)" run-$*

clean:
	@rm -rf testsuite/build-*

help: $(testdir)/Makefile $(testdirOs)/Makefile $(testdirext)/Makefile
	@echo "... check"
	@echo "... testsuite-O2"
	@echo "... testsuite-Os"
	@echo "... testsuite-ext"
	@echo "... check-extensions"
	@echo "... check-extensions-stdlib"
	@echo "... check-extensions-fallback"
	@echo "... clean"
	@echo "... install (using prefix=$(prefix))"
	@$(MAKE) -C "$(testdir)" help
	@$(MAKE) -C "$(testdirOs)" help|sed 's/run-/run-Os-/g'
	@$(MAKE) -C "$(testdirext)" help|sed 's/run-/run-ext-/g'

.PHONY: check install check-extensions check-extensions-stdlib check-extensions-fallback clean help testsuite
