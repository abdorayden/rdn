#include "./include/src.h"

// TODO: create native cross platform libs for rdn with bindings
// TODO: introduce big ints 
// TODO: better error report
// TODO: finish std native and nonative libs
// TODO: optimize the code
// TODO: introduce apply keyword that accept body just like defun (functions) but when we push the name into the stack it will executed directly
// EXAMPLE:
//  step1 apply
//      "step1"
//  end
//  step2 apply
//      "step2"
//  end
//  step3 apply
//      "step3"
//  end
//  step1
//  step2
//  step3
//  print
//  print
//  print

int main(int argc, char **argv) {
    return rdn_main(argc, argv);
}

#include "./src/src.c"
