CC = gcc
CFLAGS = -Wall -Wextra -Werror -ggdb -std=c11
LDFLAGS = -ldl -rdynamic
TARGET = main
SOURCES = main.c
OBJECTS = $(SOURCES:.c=.o)

$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) -O3 -o $(TARGET) $(LDFLAGS)

main.o: main.c include/rdn.h include/rdn_native.h src/rdn.c src/stack.h
	$(CC) $(CFLAGS) -O3 -c $< -o $@

lib: ./nativelibs/math.c
	$(CC) $(CFLAGS) -O3 -fPIC -shared ./nativelibs/math.c -I. -o ./nativelibs/math.so -lm 
	$(CC) $(CFLAGS) -O3 -fPIC -shared ./nativelibs/files.c -I. -o ./nativelibs/files.so
	$(CC) $(CFLAGS) -O3 -fPIC -shared ./nativelibs/unix.c -I. -o ./nativelibs/unix.so
	$(CC) $(CFLAGS) -O3 -fPIC -shared ./nativelibs/syscall.c -I. -o ./nativelibs/syscall.so
	$(CC) $(CFLAGS) -O3 -fPIC -shared ./nativelibs/path.c -I. -o ./nativelibs/path.so
	$(CC) $(CFLAGS) -O3 -fPIC -shared ./nativelibs/process.c -I. -o ./nativelibs/process.so
	$(CC) $(CFLAGS) -O3 -fPIC -shared ./nativelibs/net.c -I. -o ./nativelibs/net.so
	$(CC) $(CFLAGS) -O3 -fPIC -shared ./nativelibs/strings.c -I. -o ./nativelibs/strings.so
	$(CC) $(CFLAGS) -O3 -fPIC -shared ./nativelibs/strconv.c -I. -o ./nativelibs/strconv.so
	$(CC) $(CFLAGS) -O3 -fPIC -shared ./nativelibs/json.c -I. -o ./nativelibs/json.so
	$(CC) $(CFLAGS) -O3 -fPIC -shared ./nativelibs/io.c -I. -o ./nativelibs/io.so

clean:
	rm -f $(OBJECTS) $(TARGET)

run: $(TARGET)
	./$(TARGET)

.PHONY: clean run
