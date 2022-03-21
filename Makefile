CFLAGS = -W -Wall -Wextra -g -pthread -lpthread
OBJECTS = bank.o string_parser.o

bank: $(OBJECTS)
	gcc $(CFLAGS)  -o bank $^

bank.o: bank.c string_parser.o
string_parser.o: string_parser.c

clean:
	rm -f $(OBJECTS) bank
