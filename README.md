# Cshell
Implementation of a basic shell in C as a project of the Operating Systems Course, Monsoon15.

main program - shell.c
prompt function - used for prompt of the shell
command function - used to parse commands and run them
built_in function - used to run built in commands like cd, pwd, echo, exit
not_built_in function - used to run commands which have predefined binaries runs programs in both foreground and background
handler - it is the signal handler for the signal function

Features implemented:

1) shell prompt as stated
2) user defined commands as given (cd, pwd, echo)
3) run other commands both in foregrounad and background (using &)
<bonus> when background process exits, it prints out it's pid 
