#ifndef LOG_H
#define LOG_H

extern void printf(char *, ...);
extern int threadid();
extern void shutdown();

#if defined(LOG_LEVEL_ERROR) //如果定义了LOG_LEVEL_ERROR就定义USE_LOG_ERROR，后续到46行和这个一样

#define USE_LOG_ERROR

#endif // LOG_LEVEL_ERROR

#if defined(LOG_LEVEL_WARN)

#define USE_LOG_ERROR
#define USE_LOG_WARN

#endif // LOG_LEVEL_ERROR

#if defined(LOG_LEVEL_INFO)

#define USE_LOG_ERROR
#define USE_LOG_WARN
#define USE_LOG_INFO

#endif // LOG_LEVEL_INFO

#if defined(LOG_LEVEL_DEBUG)

#define USE_LOG_ERROR
#define USE_LOG_WARN
#define USE_LOG_INFO
#define USE_LOG_DEBUG

#endif // LOG_LEVEL_DEBUG

#if defined(LOG_LEVEL_TRACE)

#define USE_LOG_ERROR
#define USE_LOG_WARN
#define USE_LOG_INFO
#define USE_LOG_DEBUG
#define USE_LOG_TRACE

#endif // LOG_LEVEL_TRACE
//定义一个枚举，用来表示输出信息的颜色
enum LOG_COLOR {	
	RED = 31,
	GREEN = 32,
	BLUE = 34,
	GRAY = 90,
	YELLOW = 93,
};
//printf("\033[31mThis text is red \033[0mThis text has default color\n");
//如果定义了USE_LOG_ERROR，errorf先调用threadid获取tid，然后使用了printf带颜色的打印，
//\x1b表示0x27，\x1b[%dm和\x1b[0m都是printf带颜色打印的参数，将ERROR和tid打印出来，
//然后再打印fmt和可变参数，##_VA_ARGS__表示可变参数列表
#if defined(USE_LOG_ERROR)
#define errorf(fmt, ...)                                                       \
	do {                                                                   \
		int tid = threadid();                                          \
		printf("\x1b[%dm[%s %d]" fmt "\x1b[0m\n", RED, "ERROR", tid,   \
		       ##__VA_ARGS__);                                         \
	} while (0)
#else
#define errorf(fmt, ...)
#endif // USE_LOG_ERROR

#if defined(USE_LOG_WARN)
#define warnf(fmt, ...)                                                        \
	do {                                                                   \
		int tid = threadid();                                          \
		printf("\x1b[%dm[%s %d]" fmt "\x1b[0m\n", YELLOW, "WARN", tid, \
		       ##__VA_ARGS__);                                         \
	} while (0)
#else
#define warnf(fmt, ...)
#endif // USE_LOG_WARN

#if defined(USE_LOG_INFO)
#define infof(fmt, ...)                                                        \
	do {                                                                   \
		int tid = threadid();                                          \
		printf("\x1b[%dm[%s %d]" fmt "\x1b[0m\n", BLUE, "INFO", tid,   \
		       ##__VA_ARGS__);                                         \
	} while (0)
#else
#define infof(fmt, ...)
#endif // USE_LOG_INFO

#if defined(USE_LOG_DEBUG)
#define debugf(fmt, ...)                                                       \
	do {                                                                   \
		int tid = threadid();                                          \
		printf("\x1b[%dm[%s %d]" fmt "\x1b[0m\n", GREEN, "DEBUG", tid, \
		       ##__VA_ARGS__);                                         \
	} while (0)
#else
#define debugf(fmt, ...)
#endif // USE_LOG_DEBUG

#if defined(USE_LOG_TRACE)
#define tracef(fmt, ...)                                                       \
	do {                                                                   \
		int tid = threadid();                                          \
		printf("\x1b[%dm[%s %d]" fmt "\x1b[0m\n", GRAY, "TRACE", tid,  \
		       ##__VA_ARGS__);                                         \
	} while (0)
#else
#define tracef(fmt, ...)
#endif // USE_LOG_TRACE
//__FILE__为文件名，__LINE__为当前行号
#define panic(fmt, ...)                                                        \
	do {                                                                   \
		int tid = threadid();                                          \
		printf("\x1b[%dm[%s %d] %s:%d: " fmt "\x1b[0m\n", RED,         \
		       "PANIC", tid, __FILE__, __LINE__, ##__VA_ARGS__);       \
		shutdown();                                                    \
                                                                               \
	} while (0)

#endif //! LOG_H
