GTEST_DIR="vendor/googletest/googletest"


inverted_index: inverted_index.o main.cc
	clang++ --std=c++11 main.cc inverted_index.o -o inverted_index

inverted_index.o: inverted_index.cc inverted_index.h
	clang++ -c --std=c++11 -o inverted_index.o inverted_index.cc

test: test_inverted_index
	./test_inverted_index

test_inverted_index: test_inverted_index.cc inverted_index.o inverted_index.h libgtest.a
	clang++ -g --std=c++11 -o test_inverted_index \
		-pthread \
		-isystem ${GTEST_DIR}/include \
		libgtest.a inverted_index.o test_inverted_index.cc

libgtest.a:
	clang++ --std=c++11 \
		-isystem ${GTEST_DIR}/include \
		-I${GTEST_DIR} \
		-pthread \
		-c ${GTEST_DIR}/src/gtest-all.cc
	ar -rv libgtest.a gtest-all.o

clean:
	rm -f inverted_index *.a *.o test_inverted_index
