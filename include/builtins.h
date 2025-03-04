#ifndef _BUILTINS_H_
#define _BUILTINS_H_

#define BUILTIN_ERROR 2

#define LKILL_ERROR_MSG "Builtin 'lkill' error.\n"
#define LLS_ERROR_MSG "Builtin 'lls' error.\n"
#define LCD_ERROR_MSG "Builtin 'lcd' error.\n"
#define EXIT_ERROR_MSG "Builtin 'exit' error.\n"


typedef struct {
	char* name;
	int (*fun)(char**); 
} builtin_pair;

extern builtin_pair builtins_table[];

#endif /* !_BUILTINS_H_ */
