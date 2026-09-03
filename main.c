#include "./include/rdn.h"
#include <string.h>
#include <stdio.h>

// TODO: finish std native and nonative libs
// TODO: optimize the code
// TODO: macros doesn't allow nested blocks
// TODO: next work i need to fix bugs in this language and add more tests
//
//
//
//
// TODO: add comment doc like python but uses <* Doc *> inside functions, modules and then make it global in file
// TODO: add single line comment
// TODO: check functions hanling for the API

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

#include "./src/rdn.c"
