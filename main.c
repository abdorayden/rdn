#include "./include/src.h"

// TODO: make a good api in C so i can implement native functions or create bindings

// TODO: introduce lists (make those builtin functions works with string)
// example :
// (1 2 3 4 5 "hello") [* push this list on the stack *]
// 6 append [* accept a value *]
// 0 index [* accept index from list and push it in the stack *]
// 0 remove [* accept index and remove the value from it *]
// [* other example *]
// ("rayden" , "abdo") names let
// names "other_name" append

// TODO: introduce string manipulations (to make it easy just convert it to a list)
// example :
// "hello" slice 0 index print

// TODO: introduce functions 
// example :
// foo defun
//  "hello foo" print
// end
// [* call function *]
// foo call

// TODO: introduce load , loadnative
// example:
// "std" load [* .rdn file *]
// "raylib" loadnative [* .so/.dll file *]

// TODO: introduce big ints 


int main(int argc, char **argv) {
    return rdn_main(argc, argv);
}

#include "./src/src.c"
