@echo off
setlocal
cd /d "%~dp0"
if not exist build mkdir build
rem Generated sources are included; Flex/Bison are not required to build.
gcc -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Inative\include -Inative\generated -o build\mpc.exe native\src\main.c native\src\ast.c native\src\symtab.c native\src\tac.c native\src\optimize.c native\src\vm.c native\generated\parser.tab.c native\generated\lex.yy.c
if errorlevel 1 exit /b 1
echo Built build\mpc.exe
