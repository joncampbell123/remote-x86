all: ../e_lib.a ../s_lib.a cpu_tests
	make -C cpu

../e_lib.a:
	make -C ..

../s_lib.a:
	make -C ..

cpu_tests: cpu_tests.c ../s_lib.a
	gcc -o $@ $^

clean:
	make -C cpu clean
	rm -f cpu_tests

