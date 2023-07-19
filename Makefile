CXXFLAGS+=-Wno-attributes -Wno-unknown-pragmas
ifneq ($(findstring GCC,$(shell $(CXX) --version)),)
	CXXFLAGS+=-static-libstdc++
endif
CXXFLAGS+=-I$(PWD)
prefix=/usr/local
includedir=$(prefix)/include

srcdir=.
testdir=testsuite/build
sim=
testflags=-march=native -std=c++2a

check: check-extensions testsuite

$(testdir)/Makefile: $(srcdir)/testsuite/generate_makefile.sh
	@echo "Generating simd testsuite subdirs and Makefiles ..."
	@$(srcdir)/testsuite/generate_makefile.sh --destination="$(testdir)" --sim="$(sim)" --testflags="$(testflags)" $(CXX) $(CXXFLAGS) -DVIR_DISABLE_STDX_SIMD -DVIR_SIMD_TS_DROPIN
       
testsuite: $(testdir)/Makefile
	@rm -f .simd.summary
	$(CXX) --version
	@$(MAKE) -C "$(testdir)"
	@tail -n20 $(testdir)/simd_testsuite.sum | grep -A20 -B1 'Summary ===' >> .simd.summary
	@cat .simd.summary && rm .simd.summary

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

clean:
	@rm -r "$(testdir)"

help: $(testdir)/Makefile
	@echo "... check"
	@echo "... testsuite"
	@echo "... check-extensions"
	@echo "... check-extensions-stdlib"
	@echo "... check-extensions-fallback"
	@echo "... clean"
	@echo "... install (using prefix=$(prefix))"
	@$(MAKE) -C "$(testdir)" help

.PHONY: check install check-extensions check-extensions-stdlib check-extensions-fallback clean help testsuite
