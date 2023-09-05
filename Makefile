CXXFLAGS+=-Wno-attributes -Wno-unknown-pragmas
ifneq ($(findstring GCC,$(shell $(CXX) --version)),)
	CXXFLAGS+=-static-libstdc++
endif
CXXFLAGS+=-I$(PWD)
prefix=/usr/local
includedir=$(prefix)/include

srcdir=.
testdir=testsuite/build-O2
sim=
testflags=-march=native -std=c++2a

check: check-extensions testsuite-O2 testsuite-Os

testsuite/build-%/Makefile: $(srcdir)/testsuite/generate_makefile.sh
	@echo "Generating simd testsuite ($*) subdirs and Makefiles ..."
	@$(srcdir)/testsuite/generate_makefile.sh --destination="testsuite/build-$*" --sim="$(sim)" --testflags="$(testflags)" $(CXX) -O2 -$* $(CXXFLAGS) -DVIR_DISABLE_STDX_SIMD -DVIR_SIMD_TS_DROPIN
       
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

run-%:
	@$(MAKE) -C "$(testdir)" run-$*

run-Os-%:
	@$(MAKE) -C "testsuite/build-Os" run-$*

clean:
	@rm -rf testsuite/build-*

help: $(testdir)/Makefile testsuite/build-Os/Makefile
	@echo "... check"
	@echo "... testsuite-O2"
	@echo "... testsuite-Os"
	@echo "... check-extensions"
	@echo "... check-extensions-stdlib"
	@echo "... check-extensions-fallback"
	@echo "... clean"
	@echo "... install (using prefix=$(prefix))"
	@$(MAKE) -C "$(testdir)" help
	@$(MAKE) -C "$(testdir)" help|sed 's/run-/run-Os-/g'

.PHONY: check install check-extensions check-extensions-stdlib check-extensions-fallback clean help testsuite
