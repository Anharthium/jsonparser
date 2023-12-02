CC=g++
CFLAGS=-I.
JSONPARSE_EXEC=jsonparser.out

jsonparser: json_main.cpp json_parse.cpp
	$(CC) -o $(JSONPARSE_EXEC) json_main.cpp json_parse.cpp $(CFLAGS)

tests: run_tests.cpp json_parse.cpp
	$(CC) -o runtests.out run_tests.cpp json_parse.cpp $(CFLAGS)
