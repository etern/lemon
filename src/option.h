#ifndef _OPTION_H_
#define _OPTION_H_

#include <stdio.h>

enum option_type { OPT_FLAG=1,  OPT_INT,  OPT_DBL,  OPT_STR,
         OPT_FFLAG, OPT_FINT, OPT_FDBL, OPT_FSTR};
struct s_options {
  enum option_type type;
  const char *label;
  char *arg;
  const char *message;
};
int    OptInit(char**,struct s_options*,FILE*);
int    OptNArgs(void);
char  *OptArg(int);
void   OptErr(int);
void   OptPrint(void);

#endif // _OPTION_H_
