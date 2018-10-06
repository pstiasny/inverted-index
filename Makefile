inverted_index: inverted_index.cc
	clang++ --std=c++11 -o inverted_index inverted_index.cc

clean:
	rm -f inverted_index
