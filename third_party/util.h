#ifndef UTIL_H_
#define UTIL_H_

typedef enum{
    NONE = -1,
    LPAREN,
    ADDITION_TYPE,
    MULTIPLICATION_TYPE
} OPType;

bool is_integer(char c) ;
OPType pre(char ch);
int cal(int num1 , int num2 , char op);
int isOP(char ch);
int to_i(char* num);
#endif /* UTIL_H_ */

#ifdef UTIL_C_
#include <stdbool.h>
#include <string.h>
#include <stdio.h>


bool is_integer(char c) {
    if (c >= '0' && c <= '9') {
        return true;
    } else {
        return false;
    }
}
OPType pre(char ch){
	if(ch == '(')	return LPAREN;
	else if(ch == '+' || ch == '-')	return ADDITION_TYPE;
	else if(ch == '*' || ch == '/')	return MULTIPLICATION_TYPE;
	else return NONE;
}
int cal(int num1 , int num2 , char op){
	switch(op){
		case '*':
			return num1*num2;
		case '+':
			return num1+num2;
		case '-':
			return num1-num2;
		case '/':
			return num1/num2;
		default :
			return -1;
	}
}
int isOP(char ch){
	return (ch == '+' || ch == '-' || ch == '/' || ch == '*');
}
int to_i(char* num){
	if(strlen(num) < 1)	return -1;
	int outnum[strlen(num)];
	for(size_t i = 0 ; i < strlen(num) ; i++){
		if(((int)num[i] > 57) && ((int)num[i] < 48))
			return -1;
		outnum[i] = num[i] - 48;
	}
	int res = 0;
	for(int j = 1 ; j <= (strlen(num) - 1) ; j++){
		res += *(outnum + (j - 1)) * _pow(10, (strlen(num) - j));
	}
	res += outnum[strlen(num) - 1];
	return res;
}
#endif /* UTIL_C_ */
