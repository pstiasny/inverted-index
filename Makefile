CXX = clang++
CXX_FLAGS = --std=c++11 -g
GTEST_DIR = "vendor/googletest/googletest"
SOURCES = $(shell find src -name '*.cc')
OBJECTS = $(SOURCES:src/%.cc=build/%.o)

$(warning objects $(OBJECTS))

build/inverted_index: $(OBJECTS) main.cc
	clang++ -g --std=c++11 -Iinclude main.cc $(OBJECTS) -o build/inverted_index

test: build/test_inverted_index
	build/test_inverted_index


build/test_inverted_index: tests/test_inverted_index.cc $(OBJECTS) include/inverted_index.h build/libgtest.a
	$(CXX) $(CXX_FLAGS) -g -o build/test_inverted_index \
		-pthread \
		-isystem ${GTEST_DIR}/include \
		-Iinclude \
		build/libgtest.a $(OBJECTS) tests/test_inverted_index.cc

build/libgtest.a: | build
	clang++ -g --std=c++11 \
		-isystem ${GTEST_DIR}/include \
		-I${GTEST_DIR} \
		-pthread \
		-c ${GTEST_DIR}/src/gtest-all.cc \
		-o build/gtest-all.o
	ar -rv build/libgtest.a build/gtest-all.o

clean:
	rm -rf build


build/phy/%.o: src/phy/%.cc include/inverted_index.h src/phy/phy.h | build
	$(CXX) $(CXX_FLAGS) -Iinclude -c $< -o $@


build/%.o: src/%.cc include/inverted_index.h | build
	$(CXX) $(CXX_FLAGS) -Iinclude -c $< -o $@


build:
	mkdir build
	mkdir build/phy
