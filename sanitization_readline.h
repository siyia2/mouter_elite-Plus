// sanitization_readline.h

#ifndef SANITIZATION_READLINE_H
#define SANITIZATION_READLINE_H
#include <cstdio>
#include <readline/readline.h>
#include <string>

std::string shell_escape(const std::string& s);
std::string readInputLine(const std::string& prompt);
#endif // SANITIZATION_READLINE_H
