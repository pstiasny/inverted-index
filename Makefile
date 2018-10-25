GTEST_DIR="vendor/googletest/googletest"


inverted_index: inverted_index.cc main.cc
	clang++ --std=c++11 -o inverted_index main.cc

test: test_inverted_index
	./test_inverted_index

test_inverted_index: test_inverted_index.cc libgtest.a
	clang++ --std=c++11 -o test_inverted_index -pthread -isystem ${GTEST_DIR}/include libgtest.a test_inverted_index.cc

libgtest.a:
	clang++ --std=c++11 \
		-isystem ${GTEST_DIR}/include \
		-I${GTEST_DIR} \
		-pthread \
		-c ${GTEST_DIR}/src/gtest-all.cc
	ar -rv libgtest.a gtest-all.o

clean:
	rm -f inverted_index *.a *.o
