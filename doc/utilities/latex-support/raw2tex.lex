%{
/* Copyright (c) by Denys Duchier, April 1996, Universitaet des Saarlandes */
#include <stdio.h>
#include <string.h>
#define warning(msg) fprintf(stderr,"Warning at line %d: %s\n",yylineno)
#define error(msg) { \
  fprintf(stderr,"Error at line %d: %s\n",yylineno); \
  exit(-1) ; }
int space=0;
#define SPACE space++
#define TAB   space += ++space % 8
#define RESET space=0
#define FLUSH {if (space>0) printf("\\OzSpace{%d}",space); RESET; }
#define PUSH yy_push_state
#define POP  yy_pop_state()
int level=0;
int after_skip;
#define INCR level++
#define DECR if (--level<=0) POP
#define OZCHAR { FLUSH; printf("\\OzChar\\%c",yytext[0]); }
#define OZKEYWORD { FLUSH; printf("\\OzKeyword{%s}",yytext); }
#define FLUSHECHO {FLUSH;ECHO;}
#define ENDPOP    {printf("}");POP;}
#define FLUSHPOP  {FLUSH;printf("}");POP;}
#define ESCAPE    {                                             \
  int c;                                                        \
  FLUSH;                                                        \
  while ((c=input())!='') {                                   \
    if (c==EOF) error("end of file in escape from Oz code");    \
    putchar(c); } }
#define STARTFWD { FLUSH; printf("\\OzFwd{"); PUSH(QUOTED); }
#define STARTBWD { FLUSH; printf("\\OzBwd{"); PUSH(QUOTED); }
#define STARTSTRING { FLUSH; printf("\\OzString{"); PUSH(STRING); }
void banner() {
  printf("%s","%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%\n");
  printf("%s","%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%\n");
  printf("%s","%                                                                  %\n");
  printf("%s","%    This file has been generated by raw2tex from a .raw file      %\n");
  printf("%s","%           raw2tex (flex) by Denys Duchier, April 1996            %\n");
  printf("%s","%                   DON'T EDIT THIS FILE !                         %\n");
  printf("%s","%                                                                  %\n");
  printf("%s","%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%\n");
  printf("%s","%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%\n");
}
%}
IDENT           [a-zA-Z0-9_]+
KEYWORD         (proc|fun|local|declare|"if"|or|dis|choice|"case"|then|"else"|elseif|of|elseof|elsecase|end|"class"|create|meth|"extern"|from|with|attr|feat|self|true|false|touch|div|mod|andthen|orelse|thread|job|conc|in|condis|not)
%option stack
%option yylineno
%x TEX DISPLAY QUOTED STRING COMMENT EOLCOMMENT INLINE INDEX ENTRY SEE SKIP
%%
        banner(); BEGIN(TEX);

<TEX>"\\begin{ozdisplay""*"?"}"([ \t]*\n)*      {
  printf("\\begin{oz2texdisplay}");
  RESET; BEGIN(DISPLAY); }
<TEX>"\\?"              { printf("\\OzInline{"); RESET; PUSH(INLINE); }
<TEX>"\\ozindex{"       { printf("\\index{");    RESET; PUSH(INDEX);  }
<TEX>%.*$               ECHO;
<TEX>"\\".              ECHO;
<TEX>.                  ECHO;
<TEX>\n                 ECHO;

<DISPLAY>^"@H@"         ;
<DISPLAY>^"@D@".*\n     ;
<DISPLAY>[ \t\n]*"\\end{ozdisplay""*"?"}"       {
  printf("\\end{oz2texdisplay}");
  BEGIN(TEX); }
<DISPLAY>      ESCAPE;
<DISPLAY>"/*"   { FLUSH; printf("\\OzComment{"); PUSH(COMMENT); }
<DISPLAY>"%"    { FLUSH; printf("\\OzEolComment{"); PUSH(EOLCOMMENT); }
<DISPLAY>" "    SPACE;
<DISPLAY>\t     TAB;
<DISPLAY>\n     { RESET; printf("\\OzEol "); }
<DISPLAY>[\\{}$&#^_%~]  OZCHAR;
<DISPLAY>{KEYWORD}              OZKEYWORD;
<DISPLAY>{IDENT}                FLUSHECHO;
<DISPLAY>\'     STARTFWD;
<DISPLAY>\`     STARTBWD;
<DISPLAY>\"     STARTSTRING;
<DISPLAY>.      FLUSHECHO;

<QUOTED>[\\{}$&#^_%~]   OZCHAR;
<QUOTED>[\'\`]          { FLUSH; printf("}"); POP; }
<QUOTED>" "             SPACE;
<QUOTED>\t              TAB;
<QUOTED>.               FLUSHECHO;
<QUOTED>\n              {
  SPACE; warning("newline in quoted symbol (ignored)"); }

<STRING>\\.     {
  printf("\\OzChar\\\\");
  if (index("\\{}$&#^_%~'`\"",yytext[1]))
    printf("\\OzChar\\%c",yytext[1]);
  else
    putchar(yytext[1]); }
<STRING>\"      FLUSHPOP;
<STRING>" "     SPACE;
<STRING>\t      TAB;
<STRING>\n      { FLUSH; printf("\\OzEol "); }
<STRING>[{}$&#^_%~\'\`] OZCHAR;
<STRING>.       FLUSHECHO;

<COMMENT>"*/"   FLUSHPOP;
<COMMENT>[\\{}$&#^_%~]  OZCHAR;
<COMMENT>" "    SPACE;
<COMMENT>\t     TAB;
<COMMENT>\n     { RESET; printf("\\OzEol "); }
<COMMENT>.      FLUSHECHO;

<EOLCOMMENT>\n  ENDPOP;
<EOLCOMMENT>[\\{}$&#^_%~]       OZCHAR;
<EOLCOMMENT>" " SPACE;
<EOLCOMMENT>\t  TAB;
<EOLCOMMENT>.   FLUSHECHO;

<INLINE>"?"     ENDPOP;
<INLINE>       ESCAPE;
<INLINE>"/*"    { FLUSH; printf("\\OzComment{"); PUSH(COMMENT); }
<INLINE>"%"     { FLUSH; printf("\\OzEolComment{"); PUSH(EOLCOMMENT); }
<INLINE>" "     SPACE;
<INLINE>\t      TAB;
<INLINE>\n      {
  SPACE; warning("newline in inline Oz code (ignored)"); }
<INLINE>[\\{}$&#^_%~]   OZCHAR;
<INLINE>{KEYWORD}       OZKEYWORD;
<INLINE>{IDENT}         FLUSHECHO;
<INLINE>\'      STARTFWD;
<INLINE>\`      STARTBWD;
<INLINE>\"      STARTSTRING;
<INLINE>.       FLUSHECHO;

<INDEX>[^!|\}]*[!|\}]   {
  int n=yyleng-1; char*s=yytext;
  RESET;
  while (n-->0) putchar(*s++);
  printf("@\\OzBox{");
  yyless(0);
  BEGIN(ENTRY); }
<INDEX>.        error("unparseable \\ozindex");

<ENTRY>" "      SPACE;
<ENTRY>\t       TAB;
<ENTRY>\n       {
  SPACE; warning("newline in index entry (ignored)"); }
<ENTRY>"!"      {printf("}!");BEGIN(INDEX);}
<ENTRY>"}"      {printf("}}");POP;}
<ENTRY>"|see{"  {printf("}|see{\\OzBox{");BEGIN(SEE);}
<ENTRY>"|"      {printf("}|"); level=1; BEGIN(SKIP);}
<ENTRY>[\\$&#^_~]       OZCHAR;
<ENTRY>{KEYWORD}        OZKEYWORD;
<ENTRY>{IDENT}          FLUSHECHO;
<ENTRY>\'       STARTFWD;
<ENTRY>\`       STARTBWD;
<ENTRY>\"       STARTSTRING;
<ENTRY>.        FLUSHECHO;

<SEE>"}"        { FLUSH; printf("}}"); level=1; BEGIN(SKIP); }
<SEE>" "        SPACE;
<SEE>\t         TAB;
<SEE>\n         {
  SPACE; warning("newline in index/see (ignored)"); }
<SEE>[\\$&#^_~] OZCHAR;
<SEE>{KEYWORD}  OZKEYWORD;
<SEE>{IDENT}    FLUSHECHO;
<SEE>\'         STARTFWD;
<SEE>\`         STARTBWD;
<SEE>\"         STARTSTRING;
<SEE>.          FLUSHECHO;

<SKIP>%.*$      ECHO;
<SKIP>\\.       ECHO;
<SKIP>"{"       ECHO;INCR;
<SKIP>"}"       ECHO;DECR;
<SKIP>\n        ECHO;
<SKIP>.         ECHO;
%%
