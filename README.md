Toy persistent in-memory inverted index

# Building

Pull submodules:

    git submodule update --init 

Build with make:

    make

To run tests:

    make test


# Querying the index

Starting the console:

    ./inverted_index

Adding documents:

    ii> (add "document 1" "foo bar")
    ii> (add "document 2" "bar baz")
    ii> (add "document 3" "foo bar baz")

Querying by word:

    ii> (query "bar")
    document 1
    document 2
    document 3

Querying for documents that contain all specified words:

    ii> (query "foo" "bar" "baz")
    document 3

Fetching a particular document:

    ii> (get "document 3")
    foo bar baz
