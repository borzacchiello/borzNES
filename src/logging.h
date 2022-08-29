#ifndef LOGGING_H
#define LOGGING_H

void info(char* format, ...);
void warning(char* format, ...);

__attribute__((noreturn)) void panic(char* format, ...);

#endif
