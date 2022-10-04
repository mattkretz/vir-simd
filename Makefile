CXXFLAGS+=-DVIR_DISABLE_STDX_SIMD
CXXFLAGS+=-DVIR_SIMD_TS_DROPIN
CXXFLAGS+=-Wno-attributes
CXXFLAGS+=-I$(PWD)

srcdir=.
testdir=testsuite/build
sim=
testflags=-march=native -std=c++2a

check: $(srcdir)/testsuite/generate_makefile.sh
	@rm -f .simd.summary
	$(CXX) --version
	@echo "Generating simd testsuite subdirs and Makefiles ..."
	@$(srcdir)/testsuite/generate_makefile.sh --destination="$(testdir)" --sim="$(sim)" --testflags="$(testflags)" $(CXX) $(CXXFLAGS)
	@$(MAKE) -C "$(testdir)"
	@tail -n20 $(testdir)/simd_testsuite.sum | grep -A20 -B1 'Summary ===' >> .simd.summary
	@cat .simd.summary && rm .simd.summary

clean:
	@rm -r "$(testdir)"
