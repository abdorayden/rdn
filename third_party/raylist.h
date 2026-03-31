/************************************************************************************************
*			Copyright (c) 2024-2026 Ray Den	raylist v2.5.0				*
*												*
*	Permission is hereby granted, free of charge, to any person obtaining a copy		*
*	of this software and associated documentation files (the "Software"), to deal		*
*	in the Software without restriction, including without limitation the rights		*
*	to use, copy, modify, merge, publish, distribute, sublicense, and/or sell		*
*	copies of the Software, and to permit persons to whom the Software is			*
*	furnished to do so, subject to the following conditions:				*
*												*
*	The above copyright notice and this permission notice shall be included in		*
*	all copies or substantial portions of the Software.					*
*												*
*	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR		*
*	IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,		*	
*	FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE		*
*	AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER			*
*	LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,		*	
*	OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN		*
*	THE SOFTWARE.										*
*												*
 ************************************************************************************************/

#ifndef LIST_H
#define LIST_H

#ifdef __cplusplus
extern "C" {
#endif

#define LIBRAYLIST_VERSION_MAJOR 2
#define LIBRAYLIST_VERSION_MINOR 5
#define LIBRAYLIST_VERSION_PATCH 0

#define LIBRAYLIST_VERSION (LIBRAYLIST_VERSION_MAJOR  * 10000 + \
                      LIBRAYLIST_VERSION_MINOR  * 100 + \
                      LIBRAYLIST_VERSION_PATCH)

#define LIBRAYLIST_VERSION_STRING "2.5.0"

#ifdef _WIN32
#define RLLAPI  __declspec(dllexport)
#else
#define RLLAPI
#endif

// 	typeof pragma returns string contained type of variable
// 	Example :
// 		int var = 4;
// 		printf("typeof(test) : %s" , typeof(var));
// 	Output :
// 		typeof(test) : int

#define typeof(x) _Generic((x),   \
    int		: "int"		, \
    int   *	: "int*"	, \
    float *	: "float*"	, \
    float	: "float"	, \
    double	: "double"	, \
    double*	: "double*"	, \
    char	: "char"	, \
    char  *	: "char*"	, \
    void  *	: "void*"	, \
    short 	: "short"	, \
    short *	: "short*"	, \
    default	: "unknown"	  \
    )

// lambda call back function in C
// i see this macro is usefull because sometimes you need to implement function directly 
// if you got problem or warnings use -Wa,--noexecstack flag when you compile your program
// NOTE: this is gcc extension , that's mean this lambda function will not work if you are using other compiler


#if defined(__GNUC__) && !defined(__clang__) && !defined(_MSC_VER)

#define RLAmbda(RLAmbda$_ret, RLAmbda$_args, RLAmbda$_body)		\
	({								\
	RLAmbda$_ret RLAmbda$__anon$ RLAmbda$_args			\
	RLAmbda$_body							\
	&RLAmbda$__anon$;						\
	})

#else
    #warning "This is not gcc, RLAmbda macro is not allowed"
#define RLAmbda(RLAmbda$_ret, RLAmbda$_args, RLAmbda$_body)		\
	NULL
#endif

/*
 *	local type
 * */
#define RLLOCAL	static

#define RLINLINE	inline

#include <stdio.h>
#include <stdint.h>

#ifndef RLBOOL
#include <stdbool.h>
#define RLBOOL bool
#endif // !RLBOOL

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#ifndef _WIN32

#ifndef _SIGNAL_H
#include <signal.h>
RLLOCAL inline void ctrl_c_raylist_handler(int sig);
#endif

#else
#include <windows.h>
RLLOCAL BOOL WINAPI ctrl_c_raylist_handler(DWORD sig);
#endif

RLLOCAL RLBOOL revesed = false;

/*
 *	if not defined RLALLOC RLSIZEOF RLREALLOC RLFREE RLUNUSED RLMEMCPY the developer not using other allocation memory or other api style
 *	NOTE: signature of functions api must be the same GNU api
 *	so we used the defualt 
 * */

#ifndef RLSIZEOF
#define RLSIZEOF	sizeof
#endif

#ifndef RLALLOC
#define RLALLOC	malloc
#endif

#ifndef RLREALLOC
#define RLREALLOC	realloc
#endif

#ifndef RLFREE
#define RLFREE	free
#endif

#ifndef RLUNUSED
#define RLUNUSED(x) (void)x
#endif

#ifndef RLMEMCPY
#define RLMEMCPY	memcpy
#endif

/*
 *	API
 *	this api is used to create thread for multiple operating systems
 *	this api used for running Asynchronous functions
 * */
#ifdef _WIN32

#include <windows.h>
#define RLAPIThread HANDLE
#define RLAPICallBackFunction	LPTHREAD_START_ROUTINE
#define RLAPIParam		LPVOID

#define RLTestCancel()

#else

#ifndef _PTHREAD_H
#include <pthread.h>
#endif

#define RLAPIThread pthread_t
#define RLAPICallBackFunction	void*
#define RLAPIParam		void*

#define RLTestCancel		pthread_testcancel

#endif


typedef char* string;


// Type enum contained bunch of types helps functions any variables type will work for
// RL_STR for strings RL_BOOL for boolean ... 
typedef enum{
	RL_CHR = 0 , 	// char type
	RL_STR     , 	// string type
	RL_BOOL    ,	// bool type
	RL_INT     , 	// int type
	RL_FLT     , 	// float type
	RL_VOIDPTR ,	// void pointer type

	RL_VOIDPTRFUNC,
	RL_VOIDFUNC,
	RL_INTFUNC ,
	RL_CHARFUNC,
	RL_STRFUNC
}Type;

typedef enum{
	LIST_INDEX_OUT_OF_RANGE = 0,
	LIST_MEMALLOC,
	NULL_VALUE,
	DATA_NOT_IN_PLACE,
	RLCOLLENCTIONS_MAX__BUFFER,
	ERROR_WAIT_THREAD,
	ERROR_KILL_THREAD,
	ERROR_CREATE_THREAD,
	LIST_EMPTY,
	LIST_SEARCH_DATA_NOTFOUND,
	FINE 
}ListError;

#ifdef LIST_C
string __errors__[FINE] = {
	[LIST_INDEX_OUT_OF_RANGE] = "[ERROR] list index out of range",
	[LIST_MEMALLOC] = "[ERROR] list allocating memory",
	[RLCOLLENCTIONS_MAX__BUFFER] = "[ERROR] you can't push more data in stack or queue because the buffer is limited",
	[NULL_VALUE] = "[ERROR] NULL value detected",
	[DATA_NOT_IN_PLACE] = "[ERROR] data in wrong place {function need data function type but found other data type}",
	[ERROR_WAIT_THREAD] = "[ERROR] wait thread , use GetLastError() or check errorno code ",
	[ERROR_KILL_THREAD] = "[ERROR] kill thread , use GetLastError() or check errorno code ",
	[ERROR_CREATE_THREAD] = "[ERROR] create thread , use GetLastError() or check errorno code ",
	[LIST_SEARCH_DATA_NOTFOUND] = "[ERROR] data is not in the list",
	[LIST_EMPTY] = "[ERROR] list empty"
};
#else
extern string __errors__[FINE];
#endif

// global __raylist___self__status__ to handle raylist errors
RLLOCAL ListError __raylist___self__status__ = FINE;

/*
 *	the call back function type in list
 * */

typedef void 	(*VOIDFUNCTION		)(void*);
typedef void* 	(*VOIDPTRFUNCTION	)(void*);
typedef int  	(*INTEGERFUNCTION  	)(void*);
typedef RLBOOL 	(*BOOLEANFUNCTION  	)(void*);
typedef char 	(*CHARACTERFUNCTION	)(void*);
typedef string 	(*STRINGFUNCTION 	)(void*);

/*
 *	Linked List struct
 *		index : index of single list
 *		type  : type of the data memory saved in void* data pointer
 *		data  : the place we store the address of our variables 
 *		next  : pointing to next list
 * */

// TODO: fix complexity problem in list structure
typedef struct _____list__ __List;

struct _____list__ {
	int 		index;
	Type 		type ;
	void* 		data ;
	RLBOOL		owned;
	struct _____list__* 	next ;
};

#ifndef LIST_MAX
#define LIST_MAX 10
#endif
static int __raylist_self_index = 0;

// NOTE: RLDefer macro detect return  main function only to clear allocated memory by raylist
// this API detect ctrl-c so if program crashed by ctrl-c signal memory will be freed
// will be called in main List or Stack or Queue function sooooo yeah

// RLDefer is defer function call atexit from stdlib to call function before quit the program
#define RLDefer		atexit	

/*
 * 	SetObject macro used to change a list object level
 *	IfaceList l = list(0);
 *	IfaceList p = stack(Buf_Disable);
 *	SetObject(1);
 *	start from 1
 *	if the number was 2 it will access p object
 * */
#define RLSetObject(x)								\
	do{									\
		if(!strcmp(typeof(x) , "int") && x < LIST_MAX && x > 0)		\
			__raylist_self_index = x - 1;					\
	}while(0)								



//    RLCopyObject(size) macro enabled copy value without point to address of the variable
//    this macro is usefull when you try to append struct to your list 
//    and use type RL_VOIDPTR or repeated variable
//    RLDisableCopyObject() do disable it
//
//    Example : 
//    	... // list is already __raylist__init_interfaces__
//    	RLCopyObject(RLSIZEOF(struct MyStruct));
//    	my_list.Append(RL_VOIDPTR , (void*)&my_struct_var);	// append more data with same size
//    	my_list.Append(RL_VOIDPTR , (void*)&my_struct_var);
//    	my_list.Append(RL_VOIDPTR , (void*)&my_struct_var);
//    	RLDisableCopyObject();

RLLOCAL RLBOOL enable_copy = false;
RLLOCAL size_t copy_size = 0;

#define RLCopyObject(size)					\
	enable_copy = true;					\
	if((size) < 0)	copy_size = (size) * -1;		\
	else if((size) > 0)		copy_size = (size);	

#define RLDisableCopyObject()					\
	enable_copy = false;					\
	copy_size = 0;						


// Global list variable 
RLLOCAL __List* __raylist_self_list__[LIST_MAX];

// global count for index
RLLOCAL int __raylist_self_global_count__[LIST_MAX];

// filter callback function 
typedef enum{
	RL_ALL = 0xFF,  	// delete all types 
	RL_ONLY		// only type
}Filter_Flag;

typedef RLBOOL (*FILTERCALLBACK)(void*);

// RLFilter macro is callback function for Filter method
// this callback function always return RLBOOL and accept void* rlfilterdata
// it's so simple that you don''t have to define another function and name it 
// you can just call Filter method and give it RLFilter callback directly

#if defined(__GNUC__) && !defined(__clang__) && !defined(_MSC_VER)

#define RLFilter(body) 	\
	RLAmbda(RLBOOL , (void* rlfilterdata) , body)

#else
    #warning "This is not gcc, RLFilter macro is not allowed"
#define RLFilter(body) 	\
	NULL
#endif

// map call back function
typedef void* (*MAPCALLBACK)(void*);

// RLMap macro is callback function for map method
// this callback function always return void* and accept void* rlmapdata
// it's so simple that you don''t have to define another function and name it 
// you can just call Map method and give it RLMap callback directly

#if defined(__GNUC__) && !defined(__clang__) && !defined(_MSC_VER)

#define RLMap(body) 	\
	RLAmbda(void* , (void* rlmapdata) , body)

#else
    #warning "This is not gcc, RLMap macro is not allowed"
#define RLMap(body) 	\
	NULL
#endif

typedef void (*MAPERRORCALLBACK)(string);

#if defined(__GNUC__) && !defined(__clang__) && !defined(_MSC_VER)

#define RLMapError(body) 	\
	RLAmbda(void , (string error) , body)

#else
    #warning "This is not gcc, RLMapError macro is not allowed"
#define RLMapError(body) 	\
	NULL
#endif

typedef void (*MAPDATACALLBACK)(void*);

#if defined(__GNUC__) && !defined(__clang__) && !defined(_MSC_VER)

#define RLMapData(body) 	\
	RLAmbda(void , (void* data) , body)

#else
    #warning "This is not gcc, RLMapData macro is not allowed"
#define RLMapData(body) 	\
	NULL
#endif

// Exec function Flag
typedef enum {
	/* return value from Exec function
	 * RL_OUT flag is not enabled if index parameter in Exec_Sync was out of range
	 * */ 
	RL_OUT, 
	/* the return value will replace function callback index ,
	* the function Exec will run all function 
	* the void function is not replaced 
	* */
	RL_INPLACE,  
	/* the function return value will added to the list */
	RL_ADDED
}Exec_Flag;

/*	definitions 
 *		for more readable code
 *	*/
#define interface 		struct

typedef interface RLList 	RLList;
typedef interface IfaceThread 	IfaceThread;
typedef interface RLResult 	RLResult;
typedef interface RLCollections RLCollections ;
typedef interface Iterator 	Iterator;

/*
 *	interface for manipulate thread handle 
 * */
interface IfaceThread{
	// the thread in it self
	RLAPIThread thread;
	/*
	 *	Wait function 
	 *	abstarct function waits for the thread specified by thread to terminate
	 * */
	void (*Wait)(RLAPIThread thread);
	/*
	 *	Kill function
	 *	abstarct function for killing the thread
	 * */
	void (*Kill)(RLAPIThread thread);
};

/*
 *	this for limit size of stack or queue
 * */
#define Buf_Disable  -1	// NOTE : if this flag is enabled stack or queue will add data dynamic
RLLOCAL int __raylist__buf__ = 0;
RLLOCAL RLBOOL __raylist_limit_buf__	= false;

/*
 *	IfaceList struct act like class all thouse functions point to other functions 
 *
 *	Example :
 *		void foo(void){
 *			printf("bar");
 *		}
 *		IfaceList test;
 *		test.List_Append = foo;
 *		
 * */

RLLOCAL void* __raylist__returned_data__ = NULL;

interface RLResult{
	/*
	 *  MapError method used for handling error
	 * */
	void (*MapError)(
			MAPERRORCALLBACK
	);

	/*
	 *  MapData method used for handling data
	 * */
	void (*MapData)(
			MAPDATACALLBACK
	);

	RLBOOL (*IsError)(
			void
	);

	/*
	 *	Unwrup method used for ignore the error
	 * */
	void (*Unwrap)(void);

	/*
	 *	GetData method for getting the data
	 * */
	void* (*GetData)(void);

};

RLLOCAL int __raylist__index__iterator__ = 0;
interface Iterator {
	/* Returns the next element in the iteration. */
	RLResult (*Next)(void);
	/* Returns true if the iteration has more elements. */
	RLBOOL (*HasNext)(void);
	/* Reset the index */
	void (*Reset)(void);
};

#define RLForEach(result , list)		\
	for(RLResult (result) = (list).Iter().Next(); (list).Iter().HasNext() ; (result) = (list).Iter().Next())

typedef void (*FOREACHDATACALLBACK)(RLResult);

#if defined(__GNUC__) && !defined(__clang__) && !defined(_MSC_VER)
#define RLForEachLambda(body) 	\
	RLAmbda(void , (RLResult result) , body)

#else
    #warning "This is not gcc, RLForEachLambda macro is not allowed"
#define RLForEachLambda(body) 	\
	NULL
#endif

// NOTE: this interface can anyone implements thiere functions
// NOTE: raylist use List and Stack and Queue function to implement this functions
interface RLList{

	/* ForEach method is java style */ 
	/* 	this method process the result for each result in list */
	void (*ForEach)(FOREACHDATACALLBACK);

	/* return implementation of the Iterator interface 						*/ 
	/* how to use it										*/
	/* for(Iterator it = my_list.Iter() , void* data = NULL ; it.HasNext() ; data = it.Next()){	*/
	/* 	...		 									*/
	/* }		 										*/
	/* 	for easier usage use RLForEach macro 							*/ 
	/* 	Example:		 								*/
	/* 		RLForEach(value , my_list){		 					*/
	/* 			...		 							*/
	/*	 	}		 								*/
	Iterator (*Iter)(void);
	/*
	 *	Any
	 * */
	RLBOOL (*Any)(void);
	/*
	 *	All
	 * */
	RLBOOL (*All)(void);
	/*
	 *	Insert function will take the data and insert at the idx variable it in __raylist_self_list__ global linked list variable
	 *
	 *	Insert(
	 *		int idx		: 		index where we insert the data
	 *		Type type	: 		data type
	 *		void* data the  :		data
	 *	)
	 *	Exemple :
	 *		my_list.Insert(1 , RL_BOOL , true);
	 * */
	RLResult (*Insert)(
			int idx,
			Type type,
			void* data
	);
	/*
	 *	Append function will take the data and stored to the __raylist_self_list__ global variable 
	 *
	 * 	Append(
	 *		Type : the data type
	 *		void*: data pointer point address of stored data
	 * 	)
	 *
	 *	Example :
	 *		my_list.Append(RL_STR , "Hello World");
	 *
	 * */
	void (*Append)(
			Type type,
			void* data
	);
	/*
	 *	List_Filter function takes function callback that returns boolean and filter all elements from the list
	 *
	 *	Example : 
	 *	int check_mod_of_num(void* tch){
	 *		return (*(int*)tch % 2);
	 *	}
	 *	
	 *					before : 	[
	 *								3,2,7,5,1
	 *							]
	 *	my_list.List_Filter(check_mod_of_num , RL_INT);
	 *					after : 	[
	 *								2
	 *							]
	 * */
	RLResult (*Filter)(
			FILTERCALLBACK,
			Type , 
			Filter_Flag flag
	);
	/*
	 * 	Search function return true if the data has been found and will store index of the data in int* parameter else will return false
	 *
	 *	Search(
	 *		int* : integer pointer variable to store index of the data we found
	 *		Type : the data type
	 *		void*: data pointer point address of stored data
	 *	)
	 *
	 *	Example :
	 *		int var;
	 *		if(my_list.Search(&var, RL_STR , "Hello World"))	// handle (var contained the index of the data)
	 *		else 							// handle
	 *
	 * */
	RLResult (*Search)(
			Type ,
			void* 
	);
	/*
	 *	Del_Index function takes index and delete it from the linked list
	 *
	 *	Del_Index(
	 *		int : the index that we want to delete
	 *	)
	 *
	 *	Example :
	 *		my_list.Del_Index(3);
	 *
	 * */
	RLResult (*Del_Index)(
			int idx 
	);
	/*
	 *	Get function takes index parameter and will return void* data  
	 *
	 *	Del_Index(
	 *		int : the index of the data we want to get
	 *	)
	 *
	 *	Example :
	 *		void* d = my_list.Get(2);
	 *		the data pointer in d variable now 
	 *
	 * */
	RLResult (*Get)(
			int idx
	);

	/*
	 *	Reverse function will reverse the list 
	 *
	 * */
	RLResult (*Reverse)(
			void
	);
#ifndef RL_NO_IO
	/*
	 *	Print function will print all our data in standerd output 
	 * */
	void (*Print)(
			void
	);
#endif // RL_NO_IO
	/*
	 * void bar{
	 * 	printf("Hello World");
	 * }
	 * IfaceList my_list = list(0);
	 * my_list.Append(RL_VOIDFUNC , bar);
	 * 	the list now contains a function 
	 *  zero for execute function of index 0 , and NULL parameter mean the funtion doesm't accept a parameter
	 * my_list.Exec_Sync(0 , NULL);
	 * */

	RLResult (*Exec_Sync)(
			int idx,
			void* data,
			Exec_Flag flag
	);
	// clear list
	void (*Clear)(
			void
	);
	/*
	 *  return String if the error is set true (__raylist___self__status__ > 0)
	 * */ 
	string (*Get_Error)(
			void
	);
	/*
	 *	map is a higher-order function that applies a given function to each element of a collection
	 *	Example : 
	 *
	 *		void* callback(void* d)
	 *		{
	 *			if(*(int*)d == 2)
	 *			{
	 *				*(int*)d = 10;
	 *			}
	 *			return d;
	 *		}
	 *		IfaceList my_list = list(0);
	 *		for(int i = 0 ; i < 4 ; i++){
	 *			RLAppend(i , RL_INT);
	 *		}
	 *		list : [0,1,2,3]
	 *		my_list.Map(callback , RL_INT);
	 *		list : [0,1,10,3]
	 * */ 
	void (*Map)(
			MAPCALLBACK	func,
			Type		type
	);
	/*
	 *	__raylist_self_exec__ asynchronous functiona
	 *	IfaceList my_list = list(0);
	 *	my_list.Append(RL_STRFUNC , func);
	 *	my_list.List_Exec_Async(0, NULL).Wait();
	 *
	 * */
	RLResult (*Exec_Async)(
			int idx,
			RLAPIParam data
	);

	/*
	 *	return length of list
	 * */
	int (*Len)(void);
};

// RLCollections interface 
// contains all method , push pop and more for stack and queue data structure
interface RLCollections{
	/*
	 *	Max_Buffer() return true if __raylist__buf__ equal to fixed __raylist__buf__ else false
	 * */

	RLBOOL (*Max_Buffer)(void);

	/*
	 *	return true if stack is empty else false
	 * */
	RLBOOL (*Is_Empty)(
			void
	);
	/*
	 *  return String if the error is set true (__raylist___self__status__ > 0)
	 * */ 
	string (*Get_Error)(
			void
	);
	/*
	 *	Push function will take the data and stored to the __raylist_self_list__ global variable 
	 *
	 * 	Push(
	 *		Type : the data type
	 *		void*: data pointer point address of stored data
	 * 	)
	 *	
	 *	NOTE : the append method works like push in stack algorithm
	 *
	 *	Example :
	 *		my_list.Push(RL_STR , "Hello World");
	 *
	 * */
	RLResult (*Push)(
			Type type,
			void* data
	);
	/*
	 *	Peek function return last value of the Stack (Stack)
	 *	Example : 
	 *		void* value = my_list.Stack_Peek();
	 * */
	RLResult (*Peek)(void);
	/*
	 *	Stack_Pop function will pop the last value from the list
	 *
	 *	Example : 
	 *		void* value = my_list.Stack_Pop();
	 * */
	RLResult (*Pop)(void);
	// clear Stack
	void (*Clear)(
			void
	);

};

/*
 *	it's like List_Append but use generic so you can add value directly without reference
 * */
#define RLAppend(t , d) _Generic((d) , 		\
			int : add_int,		\
			char : add_char,	\
			float : add_float,	\
			default : add_voidptr		\
		)(&__raylist_self_list__[__raylist_self_index] , d , t  , __raylist_self_global_count__[__raylist_self_index]++);

#define RLPush(t , d) 												\
	do{													\
		if(__raylist_limit_buf__){											\
			if(__raylist_self_global_count__[__raylist_self_index] < __raylist__buf__){							\
				_Generic((d) ,	 								\
					int : add_int,								\
					char : add_char,							\
					float : add_float,							\
					default : add_voidptr							\
				)(&__raylist_self_list__[__raylist_self_index] , d , t  , __raylist_self_global_count__[__raylist_self_index]++);			\
			}else{											\
				__raylist___self__status__ = RLCOLLENCTIONS_MAX__BUFFER;			\
			}											\
		}else{												\
				_Generic((d) ,	 								\
					int : add_int,								\
					char : add_char,							\
					float : add_float,							\
					default : add_voidptr							\
				)(&__raylist_self_list__[__raylist_self_index] , d , t  , __raylist_self_global_count__[__raylist_self_index]++);			\
		}												\
	}while(0)

RLLAPI RLList List(
		int count , 	// number of data you
		... 		// data : <TYPE> , <VALUE>
);

RLLAPI RLCollections Stack(int buffer_size);

RLLAPI RLCollections Queue(int buffer_size);

#define ___CAPACITY_MAX 	32

#ifdef LIST_C
void* __raylist__table__stock__memory__[___CAPACITY_MAX] = {0};
int  _____raylist_index = 0;
#else
extern void* __raylist__table__stock__memory__[___CAPACITY_MAX];
extern int  _____raylist_index;
#endif

void* __raylist__local__temp__int(int value);
void* __raylist__local__temp__float(float value);
void* __raylist__local__temp__short(short value);
void* __raylist__local__temp__long(long value);
void* __raylist__local__temp__char(char value);
void* __raylist__local__temp__double(double value);
void* __raylist__local__temp__bool(bool value);

#define RLTempalloc(v) _Generic((v) , 			\
	int : __raylist__local__temp__int,		\
	double : __raylist__local__temp__double,	\
	short : __raylist__local__temp__short,		\
	long : __raylist__local__temp__long,		\
	char : __raylist__local__temp__char,		\
	bool : __raylist__local__temp__bool,		\
	float : __raylist__local__temp__float		\
		)(v)

#define RLTempallocClear()							\
	do{									\
		for(int i = 0 ; i < ___CAPACITY_MAX ; i++)			\
		{								\
			if(__raylist__table__stock__memory__[i] != NULL){	\
				RLFREE(__raylist__table__stock__memory__[i]);	\
				__raylist__table__stock__memory__[i] = NULL;	\
			}							\
		}								\
		_____raylist_index = 0;						\
	}while(0)

// LIST_C implementation of interfaces 
#ifdef LIST_C

void* __raylist__local__temp__int(int value){
	if(_____raylist_index < ___CAPACITY_MAX){
		__raylist__table__stock__memory__[_____raylist_index] = RLALLOC(RLSIZEOF(int));
		*(int*)__raylist__table__stock__memory__[_____raylist_index] = value;
		return __raylist__table__stock__memory__[_____raylist_index++];
	}
	return NULL;
}

void* __raylist__local__temp__float(float value){
	if(_____raylist_index < ___CAPACITY_MAX){
		__raylist__table__stock__memory__[_____raylist_index] = RLALLOC(RLSIZEOF(float));
		*(float*)__raylist__table__stock__memory__[_____raylist_index] = value;
		return __raylist__table__stock__memory__[_____raylist_index++];
	}
	return NULL;
}

void* __raylist__local__temp__short(short value){
	if(_____raylist_index < ___CAPACITY_MAX){
		__raylist__table__stock__memory__[_____raylist_index] = RLALLOC(RLSIZEOF(short));
		*(short*)__raylist__table__stock__memory__[_____raylist_index] = value;
		return __raylist__table__stock__memory__[_____raylist_index++];
	}
	return NULL;
}

void* __raylist__local__temp__long(long value){
	if(_____raylist_index < ___CAPACITY_MAX){
		__raylist__table__stock__memory__[_____raylist_index] = RLALLOC(RLSIZEOF(long));
		*(long*)__raylist__table__stock__memory__[_____raylist_index] = value;
		return __raylist__table__stock__memory__[_____raylist_index++];
	}
	return NULL;
}

void* __raylist__local__temp__char(char value){
	if(_____raylist_index < ___CAPACITY_MAX){
		__raylist__table__stock__memory__[_____raylist_index] = RLALLOC(RLSIZEOF(char));
		*(char*)__raylist__table__stock__memory__[_____raylist_index] = value;
		return __raylist__table__stock__memory__[_____raylist_index++];
	}
	return NULL;
}

void* __raylist__local__temp__double(double value){
	if(_____raylist_index < ___CAPACITY_MAX){
		__raylist__table__stock__memory__[_____raylist_index] = RLALLOC(RLSIZEOF(double));
		*(double*)__raylist__table__stock__memory__[_____raylist_index] = value;
		return __raylist__table__stock__memory__[_____raylist_index++];
	}
	return NULL;
}

void* __raylist__local__temp__bool(bool value){
	if(_____raylist_index < ___CAPACITY_MAX){
		__raylist__table__stock__memory__[_____raylist_index] = RLALLOC(RLSIZEOF(bool));
		*(bool*)__raylist__table__stock__memory__[_____raylist_index] = value;
		return __raylist__table__stock__memory__[_____raylist_index++];
	}
	return NULL;
}

void __raylist__temp_default_clear(void){
	for(int i = 0 ; i < ___CAPACITY_MAX ; i++)
	{
		if(__raylist__table__stock__memory__[i] != NULL){
			RLFREE(__raylist__table__stock__memory__[i]);
		}
	}
}


RLLOCAL inline void __raylist__result__maperror__(MAPERRORCALLBACK func)
{
	if(func == NULL || __raylist___self__status__ == FINE)
		return;
	func(__errors__[__raylist___self__status__]);
}

RLLOCAL inline void __raylist__result__mapdata__(MAPDATACALLBACK func)
{
	if(func == NULL || __raylist__returned_data__ == NULL){
		return;
	}
	func(__raylist__returned_data__);
}

RLLOCAL inline RLBOOL __raylist__result__iserror__(void)
{
	return __raylist___self__status__ != FINE;
}

RLLOCAL inline void __raylist__result__unwrap__(void)
{
	__raylist___self__status__ = FINE;
}

RLLOCAL inline void* __raylist__result__getdata__(void)
{
	return __raylist__returned_data__;
}

RLLOCAL RLResult __raylist__impl__result(void* data){
	RLResult result = {0};
	__raylist__returned_data__ = data;
	result.MapError = __raylist__result__maperror__;
	result.MapData = __raylist__result__mapdata__;
	result.IsError = __raylist__result__iserror__;
	result.Unwrap = __raylist__result__unwrap__;
	result.GetData = __raylist__result__getdata__;
	return result;
}

/*
 *	api function 
 * */
RLLOCAL RLAPIThread RLCreateThread(RLAPICallBackFunction func , RLAPIParam parameter)
{
#ifndef _WIN32
	RLAPIThread thread;
	if(pthread_create(&thread, NULL, func, parameter) == 0)
	{
		__raylist___self__status__ = ERROR_CREATE_THREAD;
		return thread;
	}
#else
	RLAPIThread thread = CreateThread(NULL , 0 , func , parameter , 0 , NULL);
	if(thread == NULL){
		__raylist___self__status__ = ERROR_CREATE_THREAD;
		return NULL ;
	}
#endif
	return thread;
}

RLLOCAL void RLWait(RLAPIThread thread)
{
#ifdef _WIN32
	if(WaitForSingleObject(thread , INFINITE) == WAIT_FAILED)
	{
		__raylist___self__status__ = ERROR_WAIT_THREAD;
		return;
	}
#else
	if(pthread_join(thread , NULL) != 0)
	{
		__raylist___self__status__ = ERROR_WAIT_THREAD;
		return;
	}
#endif
}

/*
 * NOTE : in Callback function don't forget to call RLTestCancel() function 
 * to enable kill thread
 * */

RLLOCAL void RLKill(RLAPIThread thread)
{
#ifdef _WIN32
	if(!TerminateThread(thread , 0))
	{
		__raylist___self__status__ = ERROR_KILL_THREAD;
		return;
	}
	CloseHandle(thread);
#else
	if(pthread_cancel(thread) != 0)
	{
		__raylist___self__status__ = ERROR_WAIT_THREAD;
		return;
	}
#endif
}

RLLOCAL void add_voidptr(
		__List** __raylist_self_list__ , 
		void* val , 
		Type t, 
		int idx
){
	__List* temp = RLALLOC(RLSIZEOF(__List));
	if(temp == NULL)
	{
		__raylist___self__status__ = LIST_MEMALLOC;
		return;
	}
	temp->owned = enable_copy;
	if(!enable_copy)
		temp->data = val;  
	else {
		// temp->data = RLALLOC(copy_size + 1);
		// RLMEMCPY(temp->data , val , copy_size + 1);
		temp->data = RLALLOC(copy_size);
		RLMEMCPY(temp->data , val , copy_size);
	}
	temp->type = t;                 
	temp->index = idx;              
	temp->next = *__raylist_self_list__;
	*__raylist_self_list__ = temp;                      
}
RLLOCAL void add_int(
		__List** __raylist_self_list__ , 
		int val , 
		Type t, 
		int idx
){
	__List* temp = RLALLOC(RLSIZEOF(__List));
	if(temp == NULL)
	{
		__raylist___self__status__ = LIST_MEMALLOC;
		return;
	}
	temp->data = RLALLOC(RLSIZEOF(int));
	*(int*)temp->data = val;
	temp->type = t;                 
	temp->index = idx;              
	temp->owned = true;
	temp->next = *__raylist_self_list__;                
	*__raylist_self_list__ = temp;                      
}

RLLOCAL void add_char(
		__List** __raylist_self_list__ , 
		char val , 
		Type t, 
		int idx
){
	__List* temp = RLALLOC(RLSIZEOF(__List));
	if(temp == NULL)
	{
		__raylist___self__status__ = LIST_MEMALLOC;
		return;
	}
	temp->data = RLALLOC(RLSIZEOF(char));
	*(char*)temp->data = val;
	temp->type = t;                 
	temp->index = idx;              
	temp->owned = true;
	temp->next = *__raylist_self_list__;                
	*__raylist_self_list__ = temp;                      
}

RLLOCAL void add_float(
		__List** __raylist_self_list__ , 
		float val , 
		Type t, 
		int idx
){
	__List* temp = RLALLOC(RLSIZEOF(__List));
	if(temp == NULL)
	{
		__raylist___self__status__ = LIST_MEMALLOC;
		return;
	}
	temp->data = RLALLOC(RLSIZEOF(float));
	*(float*)(temp->data) = val;
	temp->type = t;                 
	temp->index = idx;              
	temp->owned = true;
	temp->next = *__raylist_self_list__;                
	*__raylist_self_list__ = temp;                      
}

#define __raylist_self_local_list_add__(l , v , t , i) _Generic((v) ,	 	\
			int : add_int,			\
			char : add_char,		\
			float : add_float,		\
			default : add_voidptr		\
		)(l , v , t , i);

RLLOCAL void __raylist__init_interfaces__(void){
	for(int x = 0 ; x < LIST_MAX ; x++){
		__raylist_self_list__[x] = NULL;
		__raylist_self_global_count__[x] = 0;
	}
}

RLLOCAL RLBOOL __raylist_self_is_empty__(void)
{
	return (__raylist_self_global_count__[__raylist_self_index] == 0);
}

RLBOOL __raylist_self_list_all__(void){
	__List* local_list = __raylist_self_list__[__raylist_self_index];
	while(local_list != NULL)
	{
		if(local_list->data == NULL)	return false;
		local_list = local_list->next;
	}
	return true;
} 
RLBOOL __raylist_self_list_any__(void){
	__List* local_list = __raylist_self_list__[__raylist_self_index];
	while(local_list != NULL)
	{
		if(local_list->data != NULL)	return true;
		local_list = local_list->next;
	}
	return false;
}

RLLOCAL RLResult __raylist_self_local_list_insert__(__List** __raylist_self_list__ , int idx , Type type , void* data){
	__List* local_list = *(__raylist_self_list__);

	if(idx < __raylist_self_global_count__[__raylist_self_index] && idx >= 0){
		while(local_list != NULL){
			if(local_list->index > idx){	
				local_list->index++;
			}
			if(idx == local_list->index){
				__List* node = RLALLOC(RLSIZEOF(__List));
				if(node == NULL)
				{
					__raylist___self__status__ = LIST_MEMALLOC;
					return __raylist__impl__result(NULL);
				}
				node->data = data;
				node->type = type;
				node->index = idx;

				local_list->index++;
				__List* temp = local_list->next;
				local_list->next = node;
				node->next = temp;
				if(local_list->next != NULL)
					local_list = local_list->next;
			}

			local_list = local_list->next;
		}
		__raylist_self_global_count__[__raylist_self_index]++;
	}else{
		__raylist___self__status__ = LIST_INDEX_OUT_OF_RANGE;
		return __raylist__impl__result(NULL);
	}
	__raylist___self__status__ = FINE;
	return __raylist__impl__result(NULL);
}
RLLOCAL RLResult __raylist_self_stack_peek__(void)
{
	if(__raylist_self_list__[__raylist_self_index] == NULL){
		__raylist___self__status__ = LIST_EMPTY;
		return __raylist__impl__result(NULL);
	}
	__raylist___self__status__ = FINE;
	return __raylist__impl__result(__raylist_self_list__[__raylist_self_index]->data);
}

RLLOCAL RLBOOL __raylist_self_max_buf__(){
	return (__raylist_limit_buf__ && __raylist__buf__ == __raylist_self_global_count__[__raylist_self_index]);
}

RLLOCAL int __raylist_self_list_len__(){
	return __raylist_self_global_count__[__raylist_self_index];
}

RLLOCAL RLResult __raylist_self_queue_peek__(void)
{
	__List* temp = __raylist_self_list__[__raylist_self_index];
	if(temp == NULL)
	{
		__raylist___self__status__ = LIST_EMPTY;
		return __raylist__impl__result(NULL);
	}
	while(temp->next != NULL)	temp = temp->next;
	__raylist___self__status__ = FINE;
	return __raylist__impl__result(temp->data);
}

RLLOCAL RLResult __raylist_self_local_queue_pop__(__List** __raylist_self_list__){
	if(*__raylist_self_list__ == NULL){
		__raylist___self__status__ = LIST_EMPTY;
		return __raylist__impl__result(NULL);
	}
	__raylist___self__status__ = FINE;
	void* d;
	__List* ___temp = *__raylist_self_list__;
	if((___temp)->next == NULL){
		d = ___temp->data;
		RLFREE(___temp);
		*__raylist_self_list__ = NULL;
		__raylist_self_global_count__[__raylist_self_index]--;
		return __raylist__impl__result(d);
	}
	while((___temp)->next->next != NULL)	___temp = (___temp)->next;
	d = ___temp->next->data;
	RLFREE(___temp->next);
	___temp->next = NULL;
	__raylist_self_global_count__[__raylist_self_index]--;
	return __raylist__impl__result(d);
}

RLLOCAL RLResult __raylist_self_queue_pop__(){
	return __raylist_self_local_queue_pop__(&__raylist_self_list__[__raylist_self_index]);
}

RLLOCAL RLResult __raylist_self_local_stack_pop__(__List** __raylist_self_list__){
	__List* temp = *__raylist_self_list__;
	if(temp == NULL){
		__raylist___self__status__ = LIST_EMPTY;
		return __raylist__impl__result(NULL);
	}
	__raylist___self__status__ = FINE;
	void* ret;
	ret = temp->data;
	*__raylist_self_list__ = (*__raylist_self_list__)->next;
	RLFREE(temp);
	__raylist_self_global_count__[__raylist_self_index]--;
	return __raylist__impl__result(ret);
}

RLLOCAL RLResult __raylist_self_stack_pop__(){
	return __raylist_self_local_stack_pop__(&__raylist_self_list__[__raylist_self_index]);
}

RLLOCAL RLResult  __raylist_self_list_insert__(int idx , Type type , void* data){
	return __raylist_self_local_list_insert__(&__raylist_self_list__[__raylist_self_index] , idx , type , data);
}



RLLOCAL void __raylist_self_list_append__(
		Type type,
		void* data
)
{
	if(__raylist___self__status__ != FINE) __raylist___self__status__ = FINE;
	__raylist_self_local_list_add__(&__raylist_self_list__[__raylist_self_index] , data, type , __raylist_self_global_count__[__raylist_self_index]++);
}

RLLOCAL RLResult __raylist_self_push__(
		Type type,
		void* data
)
{
	if(__raylist___self__status__ != FINE) __raylist___self__status__ = FINE;
	if(__raylist_limit_buf__){
		if(__raylist_self_global_count__[__raylist_self_index] < __raylist__buf__){
			__raylist_self_local_list_add__(&__raylist_self_list__[__raylist_self_index] , data, type , __raylist_self_global_count__[__raylist_self_index]++);
			__raylist___self__status__ = FINE;
			return __raylist__impl__result(NULL);
		}else{
			__raylist___self__status__ = RLCOLLENCTIONS_MAX__BUFFER;
			return __raylist__impl__result(NULL);
		}
	}else{
		__raylist_self_local_list_add__(&__raylist_self_list__[__raylist_self_index] , data, type , __raylist_self_global_count__[__raylist_self_index]++);
	}
	__raylist___self__status__ = FINE;
	return __raylist__impl__result(NULL);
}

RLLOCAL void __raylist_self_clear__(void){
	for(int x = 0 ; x < LIST_MAX ; x++){
		__List* current = __raylist_self_list__[x];
		__List* temp;
		while (current) {
			temp = current; 
			current = current->next;
			if(temp->owned)
				RLFREE(temp->data);        
			RLFREE(temp);        
		}
		__raylist_self_list__[x] = NULL;
		__raylist_self_global_count__[x] = 0;
	}
	__raylist__init_interfaces__();
	revesed = false;
}

#ifdef _WIN32

RLLOCAL BOOL WINAPI ctrl_c_raylist_handler(DWORD sig)
{
	if(sig == CTRL_C_EVENT)
	{
		__raylist_self_clear__();
		__raylist__temp_default_clear();
		exit(0);
	}
	return 0;
}
#else

RLLOCAL inline void ctrl_c_raylist_handler(int sig)
{
	RLUNUSED(sig);
	__raylist_self_clear__();
	__raylist__temp_default_clear();
	exit(0);
}
#endif

#define ExecuteAll	-1

RLLOCAL RLResult __raylist_self_local_exec__(__List* __raylist_self_list__ ,int idx , void* data , Exec_Flag flag)
{
	if(idx > __raylist_self_global_count__[__raylist_self_index]){	
		__raylist___self__status__ = LIST_INDEX_OUT_OF_RANGE;
		return __raylist__impl__result(NULL);
	
	}else if(__raylist_self_list__ == NULL){
		__raylist___self__status__ = LIST_EMPTY;
		return __raylist__impl__result(NULL);
	}else{
		__raylist___self__status__ = FINE;
	}
	if(idx < 0){
		while(__raylist_self_list__ != NULL){
			switch((__raylist_self_list__)->type){
				// NOTE : RL_OUT flag will not work in that case
				// return value will block the loop
				case  RL_VOIDPTRFUNC:{
					if(data == NULL){
						void* (*call)() = (void*(*)())(__raylist_self_list__)->data;
						if(flag == RL_INPLACE){
							__raylist_self_list__->type = RL_VOIDPTR;
							__raylist_self_list__->data = (void*)call();
						}else if(flag == RL_ADDED)
						{
							__raylist_self_list_append__(RL_VOIDPTR , (void*)call());
						}
						call();
					}else{
						void* (*call)(void*) = (void*(*)(void*))(__raylist_self_list__)->data;
						if(flag == RL_INPLACE){
							__raylist_self_list__->type = RL_VOIDPTR;
							__raylist_self_list__->data = (void*)call(data);
						}else if(flag == RL_ADDED)
						{
							__raylist_self_list_append__(RL_VOIDPTR , (void*)call(data));
						}
						call(data);
					}
				}break; 
				case  RL_VOIDFUNC:{
					if(data == NULL){
						void (*call)() = (void(*)())(__raylist_self_list__)->data;
						if(flag == RL_INPLACE){
							__raylist_self_list__->type = RL_VOIDPTR;
							__raylist_self_list__->data = NULL;
						}else if(flag == RL_ADDED)
						{
							__raylist_self_list_append__(RL_VOIDPTR , NULL);
						}
						call();
					}else{
						void (*call)(void*) = (void(*)(void*))(__raylist_self_list__)->data;
						if(flag == RL_INPLACE){
							__raylist_self_list__->type = RL_VOIDPTR;
							__raylist_self_list__->data = NULL;
						}
						call(data);
					}
				}break; 
				case RL_CHARFUNC : {
					if(data == NULL){
						char* (*call)() = ((char*(*)())(__raylist_self_list__)->data);
						if(flag == RL_INPLACE) {
							__raylist_self_list__->type = RL_CHR;
							__raylist_self_list__->data = (char*)call();
						}else{
							__raylist_self_list_append__(RL_CHR , (void*)call());
						}
					}else{
						char* (*call)(void*) = ((char*(*)(void*))(__raylist_self_list__)->data);
						if(flag == RL_INPLACE) {
							__raylist_self_list__->type = RL_CHR;
							__raylist_self_list__->data = (char*)call(data);
						}else{
							__raylist_self_list_append__(RL_CHR , (void*)call(data));
						}
					}
				}break;
				case RL_INTFUNC : {
					if(data == NULL){
						int* (*call)() = ((int*(*)())(__raylist_self_list__)->data);
						if(flag == RL_INPLACE) {
							__raylist_self_list__->type = RL_INT;
							__raylist_self_list__->data = (int*)call();
						}else{
							__raylist_self_list_append__(RL_INT , (void*)call());
						}
					}else{
						int* (*call)(void*) = ((int*(*)(void*))(__raylist_self_list__)->data);
						if(flag == RL_INPLACE) {
							__raylist_self_list__->type = RL_INT;
							__raylist_self_list__->data = (int*)call(data);
						}else{
							__raylist_self_list_append__(RL_INT , (void*)call(data));
						}
					}
				}break;
				case RL_STRFUNC : {
					if(data == NULL){
						string (*call)() = ((string(*)())(__raylist_self_list__)->data);
						if(flag == RL_INPLACE) {
							__raylist_self_list__->type = RL_STR;
							__raylist_self_list__->data = (string)call();
						}else{
							__raylist_self_list_append__(RL_STR , (void*)call());
						}
					}else{
						string (*call)(void*) = ((string(*)(void*))(__raylist_self_list__)->data);
						if(flag == RL_INPLACE) {
							__raylist_self_list__->type = RL_STR;
							__raylist_self_list__->data = (string)call(data);
						}else{
							__raylist_self_list_append__(RL_STR , (void*)call(data));
						}
					}
				}break;
				default :{ 	
					__raylist_self_list__ = (__raylist_self_list__)->next;
					continue;
				}
			}
			__raylist_self_list__ = (__raylist_self_list__)->next;
		}
	}else{
		while(__raylist_self_list__ != NULL){
			if((__raylist_self_list__)->index == idx){
				switch((__raylist_self_list__)->type){
					case  RL_VOIDPTRFUNC:{
					     if(data == NULL){
						     void* (*call)() = (void*(*)())(__raylist_self_list__)->data;
						     if(flag == RL_OUT){
							     return __raylist__impl__result((void*)call());
						     }
						     else if(flag == RL_INPLACE){
							     __raylist_self_list__->type = RL_VOIDPTR;
							     __raylist_self_list__->data = (void*)call();
						     }else
						     {
							     __raylist_self_list_append__(RL_VOIDPTR , (void*)call());
						     }
						     return __raylist__impl__result(NULL);
					     }else{
						     void* (*call)(void*) = (void*(*)(void*))(__raylist_self_list__)->data;
						     if(flag == RL_OUT){
							     return __raylist__impl__result((void*)call(data));
						     }
						     else if(flag == RL_INPLACE){
							     __raylist_self_list__->type = RL_VOIDPTR;
							     __raylist_self_list__->data = (void*)call(data);
						     }else
						     {
							     __raylist_self_list_append__(RL_VOIDPTR , (void*)call(data));
						     }
						     return __raylist__impl__result(NULL);
					     }
					}break; 
					case RL_STRFUNC : {
						if(data == NULL){
							string (*call)() = ((string(*)())(__raylist_self_list__)->data);
							if(flag == RL_OUT)
						     		return __raylist__impl__result((string)call());
							else if(flag == RL_INPLACE) {
								__raylist_self_list__->type = RL_STR;
								__raylist_self_list__->data = (string)call();
							}else{
								__raylist_self_list_append__(RL_STR , (void*)call());
							}
						     	return __raylist__impl__result(NULL);
						}else{
							string (*call)(void*) = ((string(*)(void*))(__raylist_self_list__)->data);
							if(flag == RL_OUT){
						     		return __raylist__impl__result((string)call(data));
							}
							else if(flag == RL_INPLACE) {
								__raylist_self_list__->type = RL_STR;
								__raylist_self_list__->data = (char*)call(data);
							}else{
								__raylist_self_list_append__(RL_STR , (void*)call(data));
							}
						     	return __raylist__impl__result(NULL);
						}
					}break;
					case RL_CHARFUNC : {
						// NOTE : callback function can't return local address variable 
						// 		declare global variable or declare it as static
						if(data == NULL){
							char* (*call)() = ((char*(*)())(__raylist_self_list__)->data);
							if(flag == RL_OUT){
						     		return __raylist__impl__result((void*)call());
							}
							else if(flag == RL_INPLACE) {
								__raylist_self_list__->type = RL_CHR;
								__raylist_self_list__->data = (char*)call();
							}else{
								__raylist_self_list_append__(RL_CHR , (void*)call());
							}
						     	return __raylist__impl__result(NULL);
						}else{
							char* (*call)(void*) = ((char*(*)(void*))(__raylist_self_list__)->data);
							if(flag == RL_OUT){
						     		return __raylist__impl__result((void*)call(data));
							}
							else if(flag == RL_INPLACE) {
								__raylist_self_list__->type = RL_INT;
								__raylist_self_list__->data = (char*)call(data);
							}else{
								__raylist_self_list_append__(RL_CHR , (void*)call(data));
							}
						     	return __raylist__impl__result(NULL);
						}
					}break;
					case RL_INTFUNC : {
						// NOTE : callback function can't return local address variable 
						// 		declare global variable or declare it as static
						if(data == NULL){
							int* (*call)() = ((int*(*)())(__raylist_self_list__)->data);
							if(flag == RL_OUT){
						     		return __raylist__impl__result((void*)call());
							}
							else if(flag == RL_INPLACE) {
								__raylist_self_list__->type = RL_INT;
								__raylist_self_list__->data = (int*)call();
							}else{
								__raylist_self_list_append__(RL_INT , (void*)call());
							}
						     	return __raylist__impl__result(NULL);
						}else{
							int* (*call)(void*) = ((int*(*)(void*))(__raylist_self_list__)->data);
							if(flag == RL_OUT){
						     		return __raylist__impl__result((void*)call(data));
							}
							else if(flag == RL_INPLACE) {
								__raylist_self_list__->type = RL_INT;
								__raylist_self_list__->data = (int*)call(data);
							}else{
								__raylist_self_list_append__(RL_INT , (void*)call(data));
							}
						     	return __raylist__impl__result(NULL);
						}
					}break;
					case  RL_VOIDFUNC:{
						  if(data == NULL){
							  void (*call)() = (void(*)())(__raylist_self_list__)->data;
							  if(flag == RL_INPLACE){
								  __raylist_self_list__->type = RL_VOIDPTR;
								  __raylist_self_list__->data = NULL;
							  }else if(flag == RL_ADDED)
							  {
								  __raylist_self_list_append__(RL_VOIDPTR , NULL);
							  }
							  call();
						}else{
							  void (*call)(void*) = (void(*)(void*))(__raylist_self_list__)->data;
							  if(flag == RL_INPLACE){
								  __raylist_self_list__->type = RL_VOIDPTR;
								  __raylist_self_list__->data = NULL;
							  }
							  call(data);
						}
						return __raylist__impl__result(NULL);
					}break; 
					default :{ 
						__raylist_self_list__ = (__raylist_self_list__)->next;
						continue;
					}
				}
			}
			__raylist_self_list__ = (__raylist_self_list__)->next;
		}
	}
	return __raylist__impl__result(NULL);
}

RLLOCAL RLResult __raylist_self_exec__(int idx , void* data , Exec_Flag flag){
	return __raylist_self_local_exec__(__raylist_self_list__[__raylist_self_index] , idx , data , flag);
}

RLLOCAL RLResult __raylist_self_local_delete__(
		__List** __raylist_self_list__,
		int idx
		)
{
	if(*__raylist_self_list__ == NULL){
		__raylist___self__status__ = LIST_EMPTY;
		return __raylist__impl__result(NULL);
	}
	if(idx > __raylist_self_global_count__[__raylist_self_index] || idx < 0)
	{
		__raylist___self__status__ = LIST_INDEX_OUT_OF_RANGE;
		return __raylist__impl__result(NULL);
	}
	__raylist___self__status__ = FINE;
	__List* ___temp = *__raylist_self_list__;
	__List* prev = NULL;
	if (___temp != NULL && ___temp->index == idx) {
	    *__raylist_self_list__ = ___temp->next;
	    if(___temp->owned)
		    RLFREE(___temp->data);
	    RLFREE(___temp);
	    ___temp = *__raylist_self_list__;
	    while(___temp != NULL && revesed){
	        ___temp->index--;
	        ___temp =  ___temp->next;
	    }
	    __raylist_self_global_count__[__raylist_self_index]--;
	    return __raylist__impl__result(NULL);
	}
	while (___temp != NULL && ___temp->index != idx) {
		if(!revesed && ___temp->index > 0)
			___temp->index--;
		prev = ___temp;
		___temp = ___temp->next;
	}
	if (___temp == NULL) {
		return __raylist__impl__result(NULL);
	}
	prev->next = ___temp->next;
	RLFREE(___temp->data);
	RLFREE(___temp);
	___temp = prev->next;
	while(revesed && ___temp != NULL && ___temp->index > 0){
		___temp->index--;
		___temp =  ___temp->next;
	}
	__raylist_self_global_count__[__raylist_self_index]--;
	__raylist___self__status__ = FINE;
	return __raylist__impl__result(NULL);
}
RLLOCAL RLResult __raylist_self_delete__(int idx)
{
	return __raylist_self_local_delete__(&__raylist_self_list__[__raylist_self_index] , idx);
}

RLLOCAL RLResult __raylist_self_filter__(FILTERCALLBACK callback , Type type , Filter_Flag flag){
	__List* ___temp = __raylist_self_list__[__raylist_self_index];
	while(___temp != NULL){
		if(___temp->type == type){
			if(!callback(___temp->data)){
				if(__raylist_self_delete__(___temp->index).IsError()){
					return __raylist__impl__result(NULL);
				}
			}
		}else if(flag == RL_ALL){
			if(__raylist_self_delete__(___temp->index).IsError()){
				return __raylist__impl__result(NULL);
			}
		}
		___temp = ___temp->next;
	}
	return __raylist__impl__result(NULL);
}

RLLOCAL void __raylist_self_list_map__(MAPCALLBACK callback , Type type){
	__List* ___temp = __raylist_self_list__[__raylist_self_index];
	while(___temp != NULL){
		if(___temp->type == type){
			void* old_ptr = ___temp->data;
			___temp->data = callback(___temp->data);
		}
		___temp = ___temp->next;
	}
}

RLLOCAL void rlwait(RLAPIThread thread){
	RLWait(thread);
}
RLLOCAL void rlkill(RLAPIThread thread)
{
	RLKill(thread);
}

RLLOCAL RLResult __raylist_self_list_exec_async__(int idx , RLAPIParam data){
	__List* ___temp = __raylist_self_list__[__raylist_self_index];

	IfaceThread th = {0};

	if(___temp == NULL)
	{
		__raylist___self__status__ = NULL_VALUE;
		return __raylist__impl__result(NULL);
	}

	if(idx < 0 || idx > __raylist_self_global_count__[__raylist_self_index]){
		__raylist___self__status__ = LIST_INDEX_OUT_OF_RANGE;
		return __raylist__impl__result(NULL);
	}

	while(___temp != NULL)
	{
		if(___temp->index == idx){
			if(___temp->type < 11 && 5 > ___temp->type){
				RLAPIThread thread = RLCreateThread(___temp->data , data);
				th.thread = thread;
				th.Wait = rlwait;
				th.Kill = rlkill;
				break;
			}else {
				__raylist___self__status__ = DATA_NOT_IN_PLACE;
				return __raylist__impl__result(NULL);
			}
		}
		___temp = ___temp->next;
	}
	return __raylist__impl__result(&th);
}

RLLOCAL string __raylist_self_get_error__()
{
	if(__raylist___self__status__ == FINE)
		return NULL;
	return __errors__[__raylist___self__status__];
}

RLLOCAL RLResult __raylist_self_local_list_get__(__List* __raylist_self_list__ , int idx)
{
	if(__raylist_self_list__ == NULL){
		__raylist___self__status__ = LIST_EMPTY;
		return __raylist__impl__result(NULL);
	}
	if(idx > __raylist_self_global_count__[__raylist_self_index] || idx < 0){
		__raylist___self__status__ = LIST_INDEX_OUT_OF_RANGE;
		return __raylist__impl__result(NULL);
	}
	__List* local_list = __raylist_self_list__;
	__raylist___self__status__ = FINE;
	while(local_list != NULL){
		if((local_list)->index == idx){	
			return __raylist__impl__result((local_list)->data);
		}
		local_list = (local_list)->next;
	}
	__raylist___self__status__ = NULL_VALUE;
	return __raylist__impl__result(NULL);
}
RLLOCAL RLResult __raylist_self_list_get__(int idx){
	return __raylist_self_local_list_get__(__raylist_self_list__[__raylist_self_index] , idx);
}
RLLOCAL RLResult __raylist_self_local_list_search__(
		__List* __raylist_self_list__,
		Type type ,
		void* data
)
{
	__List* ___temp = __raylist_self_list__;
	if(___temp == NULL){
		__raylist___self__status__ = LIST_EMPTY;
		return __raylist__impl__result((void*)-1);
	}
	__raylist___self__status__ = FINE;
	while(___temp != NULL){
		switch(type){
			case RL_STR :{
				if(strcmp((char*)data ,(char*) ___temp->data) == 0){
					return __raylist__impl__result((void*)(intptr_t)___temp->index);
				}
			}break;
			case RL_CHR :
			case RL_INT:{
				if(*((int*)data) == (*(int*)___temp->data)){
					return __raylist__impl__result((void*)(intptr_t)___temp->index);
				}
			}break;
			case RL_FLT: {
				if(*(float*)data == *(float*)___temp->data){
					return __raylist__impl__result((void*)(intptr_t)___temp->index);
				}
			}break;
			case RL_BOOL:{
				if(*(RLBOOL*)data){
					return __raylist__impl__result((void*)(intptr_t)___temp->index);
				}
			}break;
			default : {
				___temp = ___temp->next;
				continue;
			 }
		}
		___temp = ___temp->next;
	}
	__raylist___self__status__ = LIST_SEARCH_DATA_NOTFOUND;
	return __raylist__impl__result((void*)-1);
}
RLLOCAL RLResult __raylist_self_list_search__(
		Type type ,
		void* data
	     )
{
	return __raylist_self_local_list_search__(__raylist_self_list__[__raylist_self_index] , type , data);
}
RLLOCAL RLResult __raylist_self_local_list_reverse__(__List** __raylist_self_list__)
{
	if((*__raylist_self_list__) == NULL){
		__raylist___self__status__ = LIST_EMPTY;
		return __raylist__impl__result(NULL);
	}
	__raylist___self__status__ = FINE;
	__List* prev = NULL; 
	__List* current = *__raylist_self_list__; 
	__List* next = NULL; 
	while (current != NULL) { 
	    next = current->next; 
	    current->next = prev; 
	    prev = current; 
	    current = next; 
	} 
	*__raylist_self_list__ = prev; 
	revesed = !revesed;
	return __raylist__impl__result(NULL);
}
RLResult  __raylist_self_list_reverse__(void){
	return __raylist_self_local_list_reverse__(&__raylist_self_list__[__raylist_self_index]);
}
#ifndef RL_NO_IO
void __raylist_self_list_print__()
{
	if(__raylist_self_list__[__raylist_self_index] == NULL){	
		printf("[NULL]");
		__raylist___self__status__ = LIST_EMPTY;
		return ;
	}
	__List* local_list = __raylist_self_list__[__raylist_self_index];
	__raylist___self__status__ = FINE;
	printf("[\n");
	while(local_list != NULL){
		switch((local_list)->type){
			case RL_CHR:{
				printf("\n\t[%d] '%c'",(local_list)->index  ,(*(char*)(local_list)->data) );
			}break;
			case RL_INT:{
				printf("\n\t[%d] %d",(local_list)->index  ,(*(int*)(local_list)->data) );
			}break;
			case RL_FLT:{
				printf("\n\t[%d] %f",(local_list)->index  ,(*(float*)(local_list)->data) );
			}break;

			case RL_STR:{
				printf("\n\t[%d] \"%s\"",(local_list)->index  , (char*)(local_list)->data);
			}break;
			case RL_BOOL:{
				printf("\n\t[%d] %s",(local_list)->index  , *(int*)(local_list)->data ? "true" : "false");
			}break;
			case RL_VOIDPTR :{
				// printing address of the void ptr
				printf("\n\t[%d] %p",(local_list)->index , (local_list)->data);
			}break;
			case  RL_INTFUNC:{
				printf("\n\t<int function : %p><index : %d>" ,(local_list)->data,(local_list)->index);
			}break; 
			case  RL_CHARFUNC:{
				printf("\n\t<character function : %p><index : %d>", (local_list)->data,(local_list)->index);
			}break; 
			case  RL_STRFUNC :{
				printf("\n\t<string function : %p><index : %d>", (local_list)->data,(local_list)->index);
			}break; 
			case  RL_VOIDFUNC:{
				printf("\n\t<void function : %p><index : %d>" ,(local_list)->data,(local_list)->index);
			}break;
			case  RL_VOIDPTRFUNC:{
				printf("\n\t<void* function : %p><index : %d>" ,(local_list)->data,(local_list)->index);
			}break;

		}
		(local_list) = (local_list)->next;
		if((local_list) != NULL)	printf(",");
	}
	printf("\n]\n");
}
#endif // RL_NO_IO

RLBOOL __raylist_private_has_next__() {
	return __raylist__index__iterator__ <= __raylist_self_list_len__();
}

RLResult __raylist_private_next__() {
	return __raylist_self_list_get__(__raylist__index__iterator__++);
}

void __raylist_private_reset__() {
	__raylist__index__iterator__ = 0;
}

Iterator __raylist_self_iterator__() {
	return (Iterator){
		.HasNext 	= 	__raylist_private_has_next__,
		.Next 		= 	__raylist_private_next__,
		.Reset 		= 	__raylist_private_reset__ 
	};
}

void __raylist_self_foreach__(FOREACHDATACALLBACK callback){
	for(int i = 0 ; i < __raylist_self_list_len__() ; i++){
		callback(__raylist_self_list_get__(i));
	}
}

RLList __raylist__init__list__impl(RLList* cl){
	cl->ForEach = __raylist_self_foreach__;
	cl->Iter = __raylist_self_iterator__;
	cl->All = __raylist_self_list_all__;
	cl->Any = __raylist_self_list_any__;
	cl->Insert = __raylist_self_list_insert__;
	cl->Append = __raylist_self_list_append__;
	cl->Del_Index = __raylist_self_delete__;
	cl->Get = __raylist_self_list_get__;
	cl->Search = __raylist_self_list_search__;
	cl->Reverse = __raylist_self_list_reverse__;
#ifndef RL_NO_IO
	cl->Print = __raylist_self_list_print__;
#endif // RL_NO_IO
	cl->Exec_Sync = __raylist_self_exec__;
	cl->Clear = __raylist_self_clear__;
	cl->Get_Error = __raylist_self_get_error__;
	cl->Filter = __raylist_self_filter__;
	cl->Map = __raylist_self_list_map__;
	cl->Exec_Async = __raylist_self_list_exec_async__;
	cl->Len = __raylist_self_list_len__;
	return *cl;
}

RLList List(int count , ...)
{
	__raylist__init_interfaces__();
	va_list args;
	va_start(args , count);

#ifndef _WIN32
	signal(SIGINT , ctrl_c_raylist_handler);
#else
	SetConsoleCtrlHandler(ctrl_c_raylist_handler , TRUE);
#endif

	RLList cl;
	__raylist_self_global_count__[__raylist_self_index] = count;
	int c = 0;
	while(c < count){
		Type t = va_arg(args , Type);
		switch(t){
			case RL_CHR:{
				void* temp = va_arg(args , char*);
				__raylist_self_local_list_add__(&__raylist_self_list__[__raylist_self_index],temp, RL_CHR , c);
			}break;
			case RL_INT:{
				void* temp= va_arg(args , void*);
				__raylist_self_local_list_add__(&__raylist_self_list__[__raylist_self_index],temp, RL_INT , c);
			}break;
			case RL_STR:{
				void* temp = va_arg(args , char*);
				__raylist_self_local_list_add__(&__raylist_self_list__[__raylist_self_index],temp , RL_STR , c);
			}break;
			case RL_BOOL:{
				void* t = va_arg(args , void*);
				void* temp = *(int*)t == true ? "true" : "false";
				__raylist_self_local_list_add__(&__raylist_self_list__[__raylist_self_index],temp, RL_BOOL , c);
			}break;
			case RL_FLT:{
				void* temp = va_arg(args , void*);
				float* ttemp = (float*)temp;
				__raylist_self_local_list_add__(&__raylist_self_list__[__raylist_self_index],ttemp , RL_FLT , c);
			}break;
			case RL_VOIDPTR :{
				void* temp = va_arg(args , void*);
				__raylist_self_local_list_add__(&__raylist_self_list__[__raylist_self_index],temp , RL_VOIDPTR , c);
			}break;
			case  RL_VOIDFUNC:{
				VOIDFUNCTION temp = va_arg(args ,VOIDFUNCTION);
				__raylist_self_local_list_add__(&__raylist_self_list__[__raylist_self_index],(void*)temp , RL_VOIDFUNC , c);
			}break; 
			case  RL_VOIDPTRFUNC:{
				VOIDPTRFUNCTION temp = va_arg(args ,VOIDPTRFUNCTION);
				__raylist_self_local_list_add__(&__raylist_self_list__[__raylist_self_index],(void*)temp , RL_VOIDPTRFUNC , c);
			}break; 
			case  RL_INTFUNC :{
				INTEGERFUNCTION temp = va_arg(args ,INTEGERFUNCTION);
				__raylist_self_local_list_add__(&__raylist_self_list__[__raylist_self_index],(void*)temp , RL_INTFUNC , c);
			}break;
			case  RL_CHARFUNC:{
				CHARACTERFUNCTION temp = va_arg(args ,CHARACTERFUNCTION);
				__raylist_self_local_list_add__(&__raylist_self_list__[__raylist_self_index],(void*)temp , RL_CHARFUNC , c);
			}break;
			case  RL_STRFUNC :{
				STRINGFUNCTION temp = va_arg(args ,STRINGFUNCTION);
				__raylist_self_local_list_add__(&__raylist_self_list__[__raylist_self_index],(void*)temp , RL_STRFUNC , c);
			}break; 
		}
		c++;
	}
	if(c == 0)	__raylist___self__status__ = LIST_EMPTY;
	va_end(args);
	return __raylist__init__list__impl(&cl);
}

RLLOCAL RLCollections __init_raylist_collection_start__(int buffer)
{
	__raylist__init_interfaces__();
	if(buffer != Buf_Disable){
		__raylist_limit_buf__ = true;
		__raylist__buf__ = buffer ;
	}

#ifndef _WIN32
	signal(SIGINT , ctrl_c_raylist_handler);
#else
	SetConsoleCtrlHandler(ctrl_c_raylist_handler , TRUE);
#endif

	__raylist_self_global_count__[__raylist_self_index] = 0;
	return (RLCollections){0};
}

RLCollections Stack(int buffer_size)
{
	RLCollections cl = __init_raylist_collection_start__(buffer_size);

	cl.Max_Buffer = __raylist_self_max_buf__;
	cl.Is_Empty = __raylist_self_is_empty__;
	cl.Push = __raylist_self_push__;
	cl.Clear = __raylist_self_clear__;
	cl.Peek = __raylist_self_stack_peek__;
	cl.Pop = __raylist_self_stack_pop__;
	cl.Get_Error = __raylist_self_get_error__;

	return cl;
}

RLCollections Queue(int buffer_size)
{
	RLCollections cl = __init_raylist_collection_start__(buffer_size);

	cl.Max_Buffer = __raylist_self_max_buf__;
	cl.Pop = __raylist_self_queue_pop__;
	cl.Peek = __raylist_self_queue_peek__;
	cl.Is_Empty = __raylist_self_is_empty__;
	cl.Push = __raylist_self_push__;
	cl.Clear = __raylist_self_clear__;
	cl.Get_Error = __raylist_self_get_error__;

	return cl;
}

#endif /* LIST_C */

#ifdef __cplusplus
}
#endif

#endif
