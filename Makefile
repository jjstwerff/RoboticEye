gcc:
	cd src && make

cmake:
	mkdir build -p
	cd build && cmake -DLLVM_ENABLE_PROJECTS=clang -G "Unix Makefiles" ../src && make

