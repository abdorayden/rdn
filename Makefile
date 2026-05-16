CC = gcc
CFLAGS = -Wall -Wextra -Werror -ggdb -std=c11
LDFLAGS = -ldl -rdynamic
TARGET = main
SOURCES = main.c
OBJECTS = $(SOURCES:.c=.o)

$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) -o $(TARGET) $(LDFLAGS)

main.o: main.c include/src.h include/rdn_native.h src/src.c src/stack.h
	$(CC) $(CFLAGS) -c $< -o $@

lib: ./nativelibs/math.c
	$(CC) $(CFLAGS) -fPIC -shared ./nativelibs/math.c -I. -o ./nativelibs/math.so -lm 

clean:
	rm -f $(OBJECTS) $(TARGET)

run: $(TARGET)
	./$(TARGET)

.PHONY: clean run
