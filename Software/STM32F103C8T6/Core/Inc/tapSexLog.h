//
// Created by lii on 2022/4/10.
//
#ifndef STM32F103C8T6_TAPSEXLOG_H
#define STM32F103C8T6_TAPSEXLOG_H

#include <stdio.h>

#define LOG_COLOUR_BLACK "\x1B[2;30m"
#define LOG_COLOUR_RED "\x1B[2;31m"
#define LOG_COLOUR_GREEN "\x1B[2;32m"
#define LOG_COLOUR_YELLOW "\x1B[2;33m"
#define LOG_COLOUR_BLUE "\x1B[2;34m"
#define LOG_COLOUR_MAGENTA "\x1B[2;35m"
#define LOG_COLOUR_CYAN "\x1B[2;36m"
#define LOG_COLOUR_WHITE "\x1B[2;37m"
#define LOG_COLOUR_RESET "\x1B[0m"

#define TAPSEX_LOG(fmt, ...)	printf(fmt, ##__VA_ARGS__)
#define TAPSEX_LOG_ERROR(fmt, ...) printf(LOG_COLOUR_RED  "[error] %s %s %d" LOG_COLOUR_RESET fmt, __FILE__, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define TAPSEX_LOG_DEBUG(fmt, ...) printf(LOG_COLOUR_CYAN "[debug] %s %s %d" LOG_COLOUR_RESET fmt, __FILE__, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define TAPSEX_LOG_INFO(fmt, ...) printf(LOG_COLOUR_GREEN "[info] %s %s %d" LOG_COLOUR_RESET fmt, __FILE__, __FUNCTION__, __LINE__, ##__VA_ARGS__)

#endif //STM32F103C8T6_TAPSEXLOG_H
