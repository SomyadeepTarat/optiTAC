/* lexer.l - splits the source into tokens (Abhishek) */
%option noyywrap yylineno nounput noinput

%{
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include "ast.h"
#include "parser.tab.h"
%}

%%

"//".*              { /* comment */ }
[ \t\r\n]+          { /* whitespace */ }

"int"               { return INT; }
"if"                { return IF; }
"else"              { return ELSE; }
"while"             { return WHILE; }
"return"            { return RETURN; }

[0-9]+              {
                        char *end; long value;
                        errno = 0; value = strtol(yytext, &end, 10);
                        if (errno || *end || value > INT_MAX) {
                            fprintf(stderr, "line %d: integer literal out of range\n", yylineno); exit(1);
                        }
                        yylval.num = (int)value; return NUM;
                    }
[a-zA-Z_][a-zA-Z0-9_]*  {
                        yylval.str = malloc((size_t)yyleng + 1);
                        if (!yylval.str) { fprintf(stderr, "out of memory\n"); exit(1); }
                        memcpy(yylval.str, yytext, (size_t)yyleng + 1); return ID;
                    }

"=="                { return EQ; }
"!="                { return NE; }
"<="                { return LE; }
">="                { return GE; }
[-+*/<>=(){},;]     { return yytext[0]; }

.                   { fprintf(stderr, "line %d: unknown character '%s'\n", yylineno, yytext); exit(1); }

%%

/* Token kinds remain parser-compatible; the report also shows name roles. */
void print_tokens(FILE *f) {
    int t;
    yyin = f;
    t = yylex();
    while (t != 0) {
        int line = yylineno;
        char *lexeme = strdup(yytext);
        const char *type;
        if (!lexeme) { fprintf(stderr, "out of memory\n"); exit(1); }
        if (t == ID) free(yylval.str);
        /* One-token lookahead recognizes declarations and calls, including
           whitespace and comments between a function name and its '('. */
        int next = yylex();
        if (t == ID && next == '(')
            type = strcmp(lexeme, "main") == 0 ? "ENTRY_POINT_FUNCTION" : "FUNCTION_IDENTIFIER";
        else if (t == ID) type = "IDENTIFIER";
        else if (t == NUM) type = "INTEGER_LITERAL";
        else if (t == INT || t == IF || t == ELSE || t == WHILE || t == RETURN) type = "KEYWORD";
        else if (t == '+' || t == '-' || t == '*' || t == '/') type = "ARITHMETIC_OPERATOR";
        else if (t == '=') type = "ASSIGNMENT_OPERATOR";
        else if (t == EQ || t == NE || t == LE || t == GE || t == '<' || t == '>') type = "RELATIONAL_OPERATOR";
        else type = "PUNCTUATION";
        printf("line %-3d %-21s %s\n", line, type, lexeme);
        free(lexeme);
        t = next;
    }
    rewind(f);
    yyrestart(f);
    yylineno = 1;
}
