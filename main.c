#include "./include/src.h"
#include <string.h>
#include <stdio.h>

// TODO: create native cross platform libs for rdn with bindings
// TODO: introduce big ints 
// TODO: finish std native and nonative libs
// TODO: optimize the code
// TODO: introduce protected call
// TODO: introduce deftype keyword to add a new type 
// example:
// (
// 0  [*age*]
// "" [*name*]
// ) Person deftype

int main(int argc, char **argv) {
    for(int i = 0 ; i < argc ; ++i) {
        if(
                strcmp(argv[i], "-v") == 0 ||
                strcmp(argv[i], "--version") == 0
                ){
            fprintf(stderr , "raden interpreter version %s , check or report any bug in 'https://github.com/abdorayden/rdn'\n" , RADEN_VERSION);
            return 0;

        }
    }
    return rdn_main(argc, argv);
}

#include "./src/src.c"
