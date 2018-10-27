CXX = clang++
CXX_FLAGS = --std=c++11 -Iinclude
GTEST_DIR = "vendor/googletest/googletest"
SOURCES = $(shell find src -name '*.cc')
OBJECTS = $(SOURCES:src/%.cc=%.o)


inverted_index: $(OBJECTS) main.cc
	clang++ --std=c++11 -Iinclude main.cc $(OBJECTS) -o inverted_index

inverted_index.o: src/*.cc include/inverted_index.h

test: test_inverted_index
	./test_inverted_index

test_inverted_index: tests/test_inverted_index.cc $(OBJECTS) include/inverted_index.h libgtest.a
	clang++ -g --std=c++11 -o test_inverted_index \
		-pthread \
		-isystem ${GTEST_DIR}/include \
		-Iinclude \
		libgtest.a $(OBJECTS) tests/test_inverted_index.cc

libgtest.a:
	clang++ --std=c++11 \
		-isystem ${GTEST_DIR}/include \
		-I${GTEST_DIR} \
		-pthread \
		-c ${GTEST_DIR}/src/gtest-all.cc
	ar -rv libgtest.a gtest-all.o

clean:
	rm -f inverted_index test_inverted_index *.a *.o


%.o: src/%.cc
	$(CXX) $(CXX_FLAGS) -Iinclude -c $< -o $@
