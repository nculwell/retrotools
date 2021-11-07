
#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdbool.h>

// 64K
#define RAM_SIZE 0x10000

// Constants related to the disk drive
#define SECTOR_SIZE 256
#define DISKDRIVE_BUFFER_COUNT 4
#define DISKDRIVE_COMMAND_BUFFER_SIZE 0x2A

// Limit number of instructions executed when logging, so that we don't
// generate logs until the disk fills up. Not used when running fully
// optimized (with no log generation).
#define INSTRUCTION_COUNT_LIMIT 0x400000

typedef uint8_t byte_t;
typedef uint16_t word_t;

#define IC_FMT "%09" PRIX64
#define EXPECTED_IC_SIZE 8

typedef struct {
  byte_t a;   // A
  byte_t x;   // X
  byte_t y;   // Y
  byte_t p;   // flags register
  word_t s;  // stack pointer
  word_t pc; // program counter
  uint64_t ic; // instruction count
} Registers;

enum {
  FLAG_N = 0x80, // negative
  FLAG_V = 0x40, // overflow
  // bit 5 ignored
  FLAG_B = 0x10, // break (fake flag, only appears when flags saved in RAM)
  FLAG_D = 0x08, // decimal
  FLAG_I = 0x04, // interrupt
  FLAG_Z = 0x02, // zero
  FLAG_C = 0x01, // carry
};

typedef struct {
  unsigned cap;
  unsigned len;
  byte_t* data;
} buf_t;

#define DISKDRIVE_COMMSTATE_TALKING   (1 << 0)
#define DISKDRIVE_COMMSTATE_LISTENING (1 << 1)

typedef struct {
  byte_t memory[SECTOR_SIZE];
  byte_t ptr;
} CommandBuffer;

typedef struct {
  byte_t memory[SECTOR_SIZE];
  byte_t ptr;
  byte_t channel;
} DiskBuffer;

/*
1541 drive RAM layout:
000-0FF: Zero page
100-1FF: Stack
200-229: Command buffer
22A-2FF: work space
300-3FF: Buffer 0
400-4FF: Buffer 1
500-5FF: Buffer 2
600-6FF: Buffer 3
700-7FF: BAM buffer
*/

typedef struct {
  const char* mountedImagePath; // path to d64 file
  const buf_t* mountedImageData; // contents of d64 file
  // Command buffer
  byte_t commandBuffer[DISKDRIVE_COMMAND_BUFFER_SIZE+1]; // extra space for null term
  byte_t commandBufferPointer;
  byte_t commandRecv;
  // The 1541 drive has 4 buffers.
  byte_t diskBuffers[DISKDRIVE_BUFFER_COUNT][SECTOR_SIZE];
  byte_t diskBufferPointers[DISKDRIVE_BUFFER_COUNT];
  byte_t diskBufferChannels[DISKDRIVE_BUFFER_COUNT];
  // Serial comm state
  unsigned commState;
  unsigned secondAddress;
} DiskDrive;


struct Emu_struct;
struct ExecutionHook_struct;

typedef void ExecutionHookCallback(
    struct Emu_struct * m, int pc, struct ExecutionHook_struct* hook);

// These are used as array indexes, so their order and numbering matter.
// The last one, "COUNT", is the count of values, not a value itself.
enum { HOOKTYPE_EXEC, HOOKTYPE_LOAD, HOOKTYPE_STORE, HOOKTYPE_COUNT };

typedef struct ExecutionHook_struct {
  int pcHookAddress;  // PC value where hook will be triggered
  int hookType;       // Action that triggers this hook: exec, load, store
  bool isPostHook;    // false if pre-hook, true if post-hook
  int hookID;         // ID for use by the callback
  const char* name;   // name of hook for logging purposes
  ExecutionHookCallback* callback; // hook implementation code
  void* privateData;      // object for the callback's private use
} ExecutionHook;

typedef struct {
  int pcHookAddress;
  struct {
    int off;
    int len;
  } t[HOOKTYPE_COUNT];
} ExecutionHooksLookupTableRow;

typedef struct {
  int cap;
  int len;
  int lookupLen;
  ExecutionHook* hooks;
  ExecutionHooksLookupTableRow* lookup;
  bool ready;
} ExecutionHooks;

typedef struct {
  byte_t chargen[0x1000];
  byte_t basic[0x2000];
  byte_t kernal[0x2000];
} RomC64;

typedef struct Emu_struct {
  FILE* traceFile;
  Registers reg;
  byte_t ram[RAM_SIZE];
  RomC64 rom;
  DiskDrive diskdrive;
  ExecutionHooks hooks;
  int romCallEmbeddingLevel;
  int serialBusActiveAddress;
} Emu;

#define emu_t Emu

enum instructions {
  ADC=1, AND, ASL, BCC, BCS, BEQ, BIT, BMI, BNE, BPL, BRK,
  BVC, BVS, CLC, CLD, CLI, CLV, CMP, CPX, CPY, DEC, DEX,
  DEY, EOR, INC, INX, INY, JMP, JSR, LDA, LDX, LDY, LSR,
  NOP, ORA, PHA, PHP, PLA, PLP, ROL, ROR, RTI, RTS, SBC,
  SEC, SED, SEI, STA, STX, STY, TAX, TAY, TSX, TXA, TXS,
  TYA,
};

enum {
  AMF_Resolve = 1 << 0,
  //AMF_TwoByte = 1 << 1,
  AMF_Ind     = 1 << 2,
  AMF_Abs     = 1 << 3,
  AMF_Zpg     = 1 << 4,
  AMF_X       = 1 << 5,
  AMF_Y       = 1 << 6,
  AMF_NoIndex = 1 << 7,
};

enum {

  AM_impl = 0x00, // implied
  AM_imm  = 0x01, // immediate
  AM_A    = 0x00, // treat A as implied

  AM_zpg  = 0x03,
  AM_zpgX = 0x04,
  AM_zpgY = 0x05,

  AM_rel  = 0x06,

  AM_abs  = 0x07,
  AM_absX = 0x08,
  AM_absY = 0x09,

  AM_ind  = 0x0A,
  AM_Xind = 0x0B,
  AM_indY = 0x0C,

};

typedef unsigned AddrModeFlags_t;

typedef struct {
  char name[6];
  AddrModeFlags_t flags;
} AddrModeInfo;

extern AddrModeInfo addrModeInfo[];

typedef struct instruction_s {
  byte_t instruction;
  byte_t addressingMode;
} instruction_t;

typedef struct trackinfo_s {
  unsigned trackNumber;
  unsigned sectorCount;
  unsigned offsetInSectors;
  unsigned offsetInBytes;
} trackinfo_t;

extern const char* instructionMnemonics[];
extern const char* addressModeNames[];
extern instruction_t instructionSet[0x100];
extern const char* c64RomErrors[];
extern trackinfo_t TRACK_INFO[];

// Public interface to the emulator
Emu* createEmulator(FILE* traceFile);
void registerHook(Emu* m, ExecutionHook* hook);
void loadRegisters(Emu* m, buf_t* regFile);
void loadROM(const char* path, byte_t* loadBuf, size_t size);
void loadRAM(Emu* m, buf_t* ramFile);
void mountDisk(Emu* m, const char* path, buf_t* diskData);
word_t loadPRG(Emu* m, buf_t* prgFile);
void interp(Emu* m);
void ecaLoaderRegisterHooks(Emu* m);
void dumpRam(Emu* m, const char* path);

// Emulator internals shared across implementation files.

void error(Emu* m, const char* fmt, ...)
  __attribute__((noreturn, format(printf, 2, 3)))
;

static inline bool getFlag(Emu* m, byte_t flag) {
  return (m->reg.p & flag);
}

static inline void setFlag(Emu* m, byte_t flag, bool set) {
  if (set)
    m->reg.p |= flag;
  else
    m->reg.p &= ~flag;
}

void emulateC64ROM(Emu* m, word_t callAddr);
void romError(Emu* m, int errorNumber);
void checkDiskSize(Emu* m, const buf_t* disk);

byte_t diskACPTR(Emu* m);
void diskOPEN(Emu* m);
void diskLISTEN(Emu* m);
void diskSECOND(Emu* m, byte_t second);
void diskCIOUT(Emu* m, byte_t data);
void diskUNLSN(Emu* m);
void diskTKSA(Emu* m, byte_t second);

// Other functions
buf_t* bufCreate(void);
void bufEnsureCap(buf_t* buf, unsigned cap);
void bufEnsureExtraCap(buf_t* buf, unsigned extraCap);
void bufDestroy(buf_t* buf);
void bufAppendChar(buf_t* buf, int c);
void bufAppend(buf_t* buf, const char* str);
buf_t* readFile(const char* path);

// Known RAM locations used by C64 KERNAL

#define RAM_STATUS 0x90 // status word ST (used to return error info)
#define RAM_VERCK 0x93 // load or verify flag (1=verify)

#define RAM_XSAV  0x0097 // save X register
#define RAM_LDTND 0x0098 // how many files open
#define RAM_DFLTN 0x0099 // input device, normally 0
#define RAM_DFLTO 0x009A // output CMD device, normally 3

#define RAM_END_PROG 0x0AE
#define RAM_STAL 0xC1
#define RAM_MEMUSS 0xC3

#define RAM_FNLEN 0xB7 // SETNAM filename length
#define RAM_LA 0xB8 // SETLFS logical file number
#define RAM_SA 0xB9 // SETLFS secondary address (or command?)
#define RAM_FA 0xBA // SETLFS device number
#define RAM_FNADR 0xBB // SETNAM filename address

#define RAM_LAT 0x0259 // LA table
#define RAM_FAT 0x0263 // FA table
#define RAM_SAT 0x026D // SA table

#define C64_ROM_CALL_CHKIN  0xFFC6
#define C64_ROM_CALL_GETIN  0xFFE4
#define C64_ROM_CALL_CLRCHN 0xFFCC
#define C64_ROM_CALL_CIOUT  0xFFA8
#define C64_ROM_CALL_SECOND 0xFF93
#define C64_ROM_CALL_LISTEN 0xFFB1
#define C64_ROM_CALL_UNLSN  0xFFAE
#define C64_ROM_CALL_TALK   0xFFB4
#define C64_ROM_CALL_TKSA   0xFF96
#define C64_ROM_CALL_ACPTR  0xFFA5
#define C64_ROM_CALL_UNTLK  0xFFAB
#define C64_ROM_CALL_SETNAM 0xFFBD
#define C64_ROM_CALL_SETLFS 0xFFBA
#define C64_ROM_CALL_BASIN  0xFFCF
#define C64_ROM_CALL_BSOUT  0xFFD2
#define C64_ROM_CALL_LOAD   0xFFD5
#define C64_ROM_CALL_OPEN   0xFFC0
#define C64_ROM_CALL_CLOSE  0xFFC3
#define C64_ROM_CALL_CLALL  0xFFE7

// BIT FIDDLING HELPERS

static inline byte_t toLo(word_t w) {
  return w & 0xFF;
}

static inline byte_t toHi(word_t w) {
  return w >> 8;
}

static inline word_t toWord(byte_t lo, byte_t hi) {
  return ((word_t)lo) | (((word_t)hi) << 8);
}

// Shortcuts for accessing emulator state.

#define RAM   m->ram
#define A     m->reg.a
#define X     m->reg.x
#define Y     m->reg.y
#define SP    m->reg.s
#define PC    m->reg.pc

