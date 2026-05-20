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
	$(CC) $(CFLAGS) -fPIC -shared ./nativelibs/files.c -I. -o ./nativelibs/files.so
	$(CC) $(CFLAGS) -fPIC -shared ./nativelibs/unix.c -I. -o ./nativelibs/unix.so
	$(CC) $(CFLAGS) -fPIC -shared ./nativelibs/syscall.c -I. -o ./nativelibs/syscall.so
	$(CC) $(CFLAGS) -fPIC -shared ./nativelibs/path.c -I. -o ./nativelibs/path.so
	$(CC) $(CFLAGS) -fPIC -shared ./nativelibs/process.c -I. -o ./nativelibs/process.so
	$(CC) $(CFLAGS) -fPIC -shared ./nativelibs/net.c -I. -o ./nativelibs/net.so
	$(CC) $(CFLAGS) -fPIC -shared ./nativelibs/json.c -I. -o ./nativelibs/json.so
	$(CC) $(CFLAGS) -fPIC -shared ./nativelibs/io.c -I. -o ./nativelibs/io.so

clean:
	rm -f $(OBJECTS) $(TARGET)

run: $(TARGET)
	./$(TARGET)

.PHONY: clean run
