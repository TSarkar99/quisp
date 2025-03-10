.PHONY: all tidy format ci makefile-exe makefile-lib checkmakefile googletest clean test

all: makefile-exe
	$(MAKE) -C quisp -j$(nproc)

run-module-test: lib
	cd module_tests && ./runtest

run-unit-test: lib googletest
	$(MAKE) -C quisp run-unit-test

test: run-module-test run-unit-test

exe: makefile-exe
	$(MAKE) -C quisp -j$(nproc)

lib: makefile-lib
	$(MAKE) -C quisp -j$(nproc)

ci: eigen googletest
	cd quisp && opp_makemake -f --deep -O out -i ./makefrag -M release --make-so
	$(MAKE) -C quisp ci -j$(nproc)

format: quisp/Makefile
	$(MAKE) -C quisp format

tidy: quisp/Makefile
	$(MAKE) -C quisp tidy

googletest/CMakeLists.txt:
	git submodule update --init

googletest/build: googletest/CMakeLists.txt
	mkdir -p googletest/build && cd googletest/build && cmake ..

googletest/build/lib: googletest/build
	make -C googletest/build

googletest: googletest/build/lib

eigen/CMakeLists.txt:
	git submodule update --init

eigen: eigen/CMakeLists.txt

makefile-exe: eigen
	cd quisp && opp_makemake -f --deep -O out -i ./makefrag

makefile-lib: eigen
	cd quisp && opp_makemake -f --deep -O out -i ./makefrag -M debug  --make-so

clean:
	$(RM) quisp/Makefile quisp/quisp quisp/quisp_dbg quisp/run_unit_test quisp/libquisp*
	$(RM) -r quisp/out

distclean:
	git submodule deinit --all -f
	make clean


checkmakefile:
	@if [ ! -f $(QUISP_MAKEFILE) ]; then \
	echo; \
	echo '===================================================================================================='; \
	echo 'quisp/Makefile does not exist. Please use "make makefile-exe" or "make makefile-lib" to generate it!'; \
	echo '===================================================================================================='; \
	echo; \
	exit 1; \
	fi

