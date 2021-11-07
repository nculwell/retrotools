
#ifdef TRACE_OFF
# define TRACE_ON 0
#else
# define TRACE_ON 1
# define TRACE_EXTRA 1
# define TRACE_BUFSIZ 256
#endif

#ifdef TRACE_EXTRA
#define TRACE_INSTR_FLAGS_COL 42
#else
#define TRACE_INSTR_FLAGS_COL 36
#endif

#define PRINT_STACK_BUFSIZ 40
#define PRINT_STACK_MAX_BYTES ((PRINT_STACK_BUFSIZ - 5 - 1) / 3)

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"

#if TRACE_ON

void trace(Emu* m, bool indent, const char* fmt, ...)
  __attribute__((format(printf, 3, 4)))
;

void romTrace(Emu* m, const char* fmt, ...)
  __attribute__((format(printf, 2, 3)))
;

#else

static inline void trace(Emu* m, bool indent, const char* fmt, ...) {}
static inline void romTrace(emu_t* m, const char* fmt, ...) {}

#endif

#ifdef TRACE_EXTRA

void traceSetPC(Emu* m, word_t toAddr);
void traceStack(emu_t* m, byte_t affectedByte, char sepChar);

#else

static inline void traceSetPC(Emu* m, word_t toAddr) {}
static inline void traceStack(emu_t* m, byte_t affectedByte, char sepChar) {}

#endif

#pragma GCC diagnostic pop

