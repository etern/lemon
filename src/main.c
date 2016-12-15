#include "config.h"

int showPrecedenceConflict = 0;
char *msort(char*,char**,int(*)(const char*,const char*));

/*
** Compilers are starting to complain about the use of sprintf() and strcpy(),
** saying they are unsafe.  So we define our own versions of those routines too.
**
** There are three routines here:  lemon_sprintf(), lemon_vsprintf(), and
** lemon_addtext(). The first two are replacements for sprintf() and vsprintf().
** The third is a helper routine for vsnprintf() that adds texts to the end of a
** buffer, making sure the buffer is always zero-terminated.
**
** The string formatter is a minimal subset of stdlib sprintf() supporting only
** a few simply conversions:
**
**   %d
**   %s
**   %.*s
**
*/
void lemon_addtext(
  char *zBuf,           /* The buffer to which text is added */
  int *pnUsed,          /* Slots of the buffer used so far */
  const char *zIn,      /* Text to add */
  int nIn,              /* Bytes of text to add.  -1 to use strlen() */
  int iWidth            /* Field width.  Negative to left justify */
){
  if( nIn<0 ) for(nIn=0; zIn[nIn]; nIn++){}
  while( iWidth>nIn ){ zBuf[(*pnUsed)++] = ' '; iWidth--; }
  if( nIn==0 ) return;
  memcpy(&zBuf[*pnUsed], zIn, nIn);
  *pnUsed += nIn;
  while( (-iWidth)>nIn ){ zBuf[(*pnUsed)++] = ' '; iWidth++; }
  zBuf[*pnUsed] = 0;
}
int lemon_vsprintf(char *str, const char *zFormat, va_list ap){
  int i, j, k, c;
  int nUsed = 0;
  const char *z;
  char zTemp[50];
  str[0] = 0;
  for(i=j=0; (c = zFormat[i])!=0; i++){
    if( c=='%' ){
      int iWidth = 0;
      lemon_addtext(str, &nUsed, &zFormat[j], i-j, 0);
      c = zFormat[++i];
      if( ISDIGIT(c) || (c=='-' && ISDIGIT(zFormat[i+1])) ){
        if( c=='-' ) i++;
        while( ISDIGIT(zFormat[i]) ) iWidth = iWidth*10 + zFormat[i++] - '0';
        if( c=='-' ) iWidth = -iWidth;
        c = zFormat[i];
      }
      if( c=='d' ){
        int v = va_arg(ap, int);
        if( v<0 ){
          lemon_addtext(str, &nUsed, "-", 1, iWidth);
          v = -v;
        }else if( v==0 ){
          lemon_addtext(str, &nUsed, "0", 1, iWidth);
        }
        k = 0;
        while( v>0 ){
          k++;
          zTemp[sizeof(zTemp)-k] = (v%10) + '0';
          v /= 10;
        }
        lemon_addtext(str, &nUsed, &zTemp[sizeof(zTemp)-k], k, iWidth);
      }else if( c=='s' ){
        z = va_arg(ap, const char*);
        lemon_addtext(str, &nUsed, z, -1, iWidth);
      }else if( c=='.' && memcmp(&zFormat[i], ".*s", 3)==0 ){
        i += 2;
        k = va_arg(ap, int);
        z = va_arg(ap, const char*);
        lemon_addtext(str, &nUsed, z, k, iWidth);
      }else if( c=='%' ){
        lemon_addtext(str, &nUsed, "%", 1, 0);
      }else{
        fprintf(stderr, "illegal format\n");
        exit(1);
      }
      j = i+1;
    }
  }
  lemon_addtext(str, &nUsed, &zFormat[j], i-j, 0);
  return nUsed;
}
int lemon_sprintf(char *str, const char *format, ...){
  va_list ap;
  int rc;
  va_start(ap, format);
  rc = lemon_vsprintf(str, format, ap);
  va_end(ap);
  return rc;
}
void lemon_strcpy(char *dest, const char *src){
  while( (*(dest++) = *(src++))!=0 ){}
}
void lemon_strcat(char *dest, const char *src){
  while( *dest ) dest++;
  lemon_strcpy(dest, src);
}


/* a few forward declarations... */
struct rule;
struct lemon;
struct action;

struct action *Action_new(void);
struct action *Action_sort(struct action *);

#include "build.h"
#include "configlist.h"
#include "error.h"
#include "option.h"
#include "parse.h"
#include "plink.h"
#include "report.h"
#include "set.h"
#include "struct.h"
#include "table.h"

/**************** From the file "main.c" ************************************/
/*
** Main program file for the LEMON parser generator.
*/

/* Report an out-of-memory condition and abort.  This function
** is used mostly by the "MemoryCheck" macro in struct.h
*/
void memory_error(){
  fprintf(stderr,"Out of memory.  Aborting...\n");
  exit(1);
}

int nDefine = 0;      /* Number of -D options on the command line */
char **azDefine = 0;  /* Name of the -D macros */

/* This routine is called with the argument to each -D command-line option.
** Add the macro defined to the azDefine array.
*/
static void handle_D_option(char *z){
  char **paz;
  nDefine++;
  azDefine = (char **) realloc(azDefine, sizeof(azDefine[0])*nDefine);
  if( azDefine==0 ){
    fprintf(stderr,"out of memory\n");
    exit(1);
  }
  paz = &azDefine[nDefine-1];
  *paz = (char *) malloc( lemonStrlen(z)+1 );
  if( *paz==0 ){
    fprintf(stderr,"out of memory\n");
    exit(1);
  }
  lemon_strcpy(*paz, z);
  for(z=*paz; *z && *z!='='; z++){}
  *z = 0;
}

char *user_templatename = NULL;
static void handle_T_option(char *z){
  user_templatename = (char *) malloc( lemonStrlen(z)+1 );
  if( user_templatename==0 ){
    memory_error();
  }
  lemon_strcpy(user_templatename, z);
}

/* Merge together to lists of rules order by rule.iRule */
static struct rule *Rule_merge(struct rule *pA, struct rule *pB){
  struct rule *pFirst = 0;
  struct rule **ppPrev = &pFirst;
  while( pA && pB ){
    if( pA->iRule<pB->iRule ){
      *ppPrev = pA;
      ppPrev = &pA->next;
      pA = pA->next;
    }else{
      *ppPrev = pB;
      ppPrev = &pB->next;
      pB = pB->next;
    }
  }
  if( pA ){
    *ppPrev = pA;
  }else{
    *ppPrev = pB;
  }
  return pFirst;
}

/*
** Sort a list of rules in order of increasing iRule value
*/
static struct rule *Rule_sort(struct rule *rp){
  int i;
  struct rule *pNext;
  struct rule *x[32];
  memset(x, 0, sizeof(x));
  while( rp ){
    pNext = rp->next;
    rp->next = 0;
    for(i=0; i<sizeof(x)/sizeof(x[0]) && x[i]; i++){
      rp = Rule_merge(x[i], rp);
      x[i] = 0;
    }
    x[i] = rp;
    rp = pNext;
  }
  rp = 0;
  for(i=0; i<sizeof(x)/sizeof(x[0]); i++){
    rp = Rule_merge(x[i], rp);
  }
  return rp;
}

/* forward reference */
static const char *minimum_size_type(int lwr, int upr, int *pnByte);

/* Print a single line of the "Parser Stats" output
*/
static void stats_line(const char *zLabel, int iValue){
  int nLabel = lemonStrlen(zLabel);
  printf("  %s%.*s %5d\n", zLabel,
         35-nLabel, "................................",
         iValue);
}

/* The main program.  Parse the command line and do it... */
int main(int argc, char **argv)
{
  static int version = 0;
  static int rpflag = 0;
  static int basisflag = 0;
  static int compress = 0;
  static int quiet = 0;
  static int statistics = 0;
  static int mhflag = 0;
  static int nolinenosflag = 0;
  static int noResort = 0;
  static struct s_options options[] = {
    {OPT_FLAG, "b", (char*)&basisflag, "Print only the basis in report."},
    {OPT_FLAG, "c", (char*)&compress, "Don't compress the action table."},
    {OPT_FSTR, "D", (char*)handle_D_option, "Define an %ifdef macro."},
    {OPT_FSTR, "f", 0, "Ignored.  (Placeholder for -f compiler options.)"},
    {OPT_FLAG, "g", (char*)&rpflag, "Print grammar without actions."},
    {OPT_FSTR, "I", 0, "Ignored.  (Placeholder for '-I' compiler options.)"},
    {OPT_FLAG, "m", (char*)&mhflag, "Output a makeheaders compatible file."},
    {OPT_FLAG, "l", (char*)&nolinenosflag, "Do not print #line statements."},
    {OPT_FSTR, "O", 0, "Ignored.  (Placeholder for '-O' compiler options.)"},
    {OPT_FLAG, "p", (char*)&showPrecedenceConflict,
                    "Show conflicts resolved by precedence rules"},
    {OPT_FLAG, "q", (char*)&quiet, "(Quiet) Don't print the report file."},
    {OPT_FLAG, "r", (char*)&noResort, "Do not sort or renumber states"},
    {OPT_FLAG, "s", (char*)&statistics,
                                   "Print parser stats to standard output."},
    {OPT_FLAG, "x", (char*)&version, "Print the version number."},
    {OPT_FSTR, "T", (char*)handle_T_option, "Specify a template file."},
    {OPT_FSTR, "W", 0, "Ignored.  (Placeholder for '-W' compiler options.)"},
    {OPT_FLAG,0,0,0}
  };
  int i;
  int exitcode;
  struct lemon lem;
  struct rule *rp;

  OptInit(argv,options,stderr);
  if( version ){
     printf("Lemon version 1.0\n");
     exit(0); 
  }
  if( OptNArgs()!=1 ){
    fprintf(stderr,"Exactly one filename argument is required.\n");
    exit(1);
  }
  memset(&lem, 0, sizeof(lem));
  lem.errorcnt = 0;

  /* Initialize the machine */
  Strsafe_init();
  Symbol_init();
  State_init();
  lem.argv0 = argv[0];
  lem.filename = OptArg(0);
  lem.basisflag = basisflag;
  lem.nolinenosflag = nolinenosflag;
  Symbol_new("$");
  lem.errsym = Symbol_new("error");
  lem.errsym->useCnt = 0;

  /* Parse the input file */
  Parse(&lem);
  if( lem.errorcnt ) exit(lem.errorcnt);
  if( lem.nrule==0 ){
    fprintf(stderr,"Empty grammar.\n");
    exit(1);
  }

  /* Count and index the symbols of the grammar */
  Symbol_new("{default}");
  lem.nsymbol = Symbol_count();
  lem.symbols = Symbol_arrayof();
  for(i=0; i<lem.nsymbol; i++) lem.symbols[i]->index = i;
  qsort(lem.symbols,lem.nsymbol,sizeof(struct symbol*), Symbolcmpp);
  for(i=0; i<lem.nsymbol; i++) lem.symbols[i]->index = i;
  while( lem.symbols[i-1]->type==MULTITERMINAL ){ i--; }
  assert( strcmp(lem.symbols[i-1]->name,"{default}")==0 );
  lem.nsymbol = i - 1;
  for(i=1; ISUPPER(lem.symbols[i]->name[0]); i++);
  lem.nterminal = i;

  /* Assign sequential rule numbers */
  for(i=0, rp=lem.rule; rp; rp=rp->next){
    rp->iRule = rp->code ? i++ : -1;
  }
  for(rp=lem.rule; rp; rp=rp->next){
    if( rp->iRule<0 ) rp->iRule = i++;
  }
  lem.startRule = lem.rule;
  lem.rule = Rule_sort(lem.rule);

  /* Generate a reprint of the grammar, if requested on the command line */
  if( rpflag ){
    Reprint(&lem);
  }else{
    /* Initialize the size for all follow and first sets */
    SetSize(lem.nterminal+1);

    /* Find the precedence for every production rule (that has one) */
    FindRulePrecedences(&lem);

    /* Compute the lambda-nonterminals and the first-sets for every
    ** nonterminal */
    FindFirstSets(&lem);

    /* Compute all LR(0) states.  Also record follow-set propagation
    ** links so that the follow-set can be computed later */
    lem.nstate = 0;
    FindStates(&lem);
    lem.sorted = State_arrayof();

    /* Tie up loose ends on the propagation links */
    FindLinks(&lem);

    /* Compute the follow set of every reducible configuration */
    FindFollowSets(&lem);

    /* Compute the action tables */
    FindActions(&lem);

    /* Compress the action tables */
    if( compress==0 ) CompressTables(&lem);

    /* Reorder and renumber the states so that states with fewer choices
    ** occur at the end.  This is an optimization that helps make the
    ** generated parser tables smaller. */
    if( noResort==0 ) ResortStates(&lem);

    /* Generate a report of the parser generated.  (the "y.output" file) */
    if( !quiet ) ReportOutput(&lem);

    /* Generate the source code for the parser */
    ReportTable(&lem, mhflag);

    /* Produce a header file for use by the scanner.  (This step is
    ** omitted if the "-m" option is used because makeheaders will
    ** generate the file for us.) */
    if( !mhflag ) ReportHeader(&lem);
  }
  if( statistics ){
    printf("Parser statistics:\n");
    stats_line("terminal symbols", lem.nterminal);
    stats_line("non-terminal symbols", lem.nsymbol - lem.nterminal);
    stats_line("total symbols", lem.nsymbol);
    stats_line("rules", lem.nrule);
    stats_line("states", lem.nxstate);
    stats_line("conflicts", lem.nconflict);
    stats_line("action table entries", lem.nactiontab);
    stats_line("total table size (bytes)", lem.tablesize);
  }
  if( lem.nconflict > 0 ){
    fprintf(stderr,"%d parsing conflicts.\n",lem.nconflict);
  }

  /* return 0 on success, 1 on failure. */
  exitcode = ((lem.errorcnt > 0) || (lem.nconflict > 0)) ? 1 : 0;
  exit(exitcode);
  return (exitcode);
}
