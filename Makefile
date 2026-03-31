main : main.c libraylib.so
	gcc \
		main.c \
		./src/state.c \
		-o main \
		-L./third_party/ \
		-Wl,-rpath,./third_party \
		-fPIC \
		-lraylist

libraylib.so : ./third_party/raylist.h
	gcc -DLIST_C -shared -fPIC -o ./third_party/libraylist.so ./third_party/raylist.h
