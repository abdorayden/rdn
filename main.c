#include "./include/src.h"

// TODO: create native cross platform libs for rdn with bindings
// TODO: introduce big ints 
// TODO: introduce null type
// TODO: better error report
// TODO: finish std native and nonative libs

int main(int argc, char **argv) {
    return rdn_main(argc, argv);
}

#include "./src/src.c"
