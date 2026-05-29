#include "./include/src.h"

// TODO: create native cross platform libs for rdn with bindings
// TODO: introduce big ints 
// TODO: finish std native and nonative libs
// TODO: optimize the code
// TODO: introduce protected call

int main(int argc, char **argv) {
    return rdn_main(argc, argv);
}

#include "./src/src.c"
