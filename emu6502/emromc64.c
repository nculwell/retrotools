
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <malloc.h>
#include <assert.h>
#include <stdarg.h>
#include <string.h>
#include <memory.h>

#include "em.h"
#include "emtrace.h"

// C64 ROM EMULATION

#define SERIAL_BUS_STATE_TALKER   0x40
#define SERIAL_BUS_STATE_LISTENER 0x20


#define RAM_NDX  0x00C6 // keyboard buffer count
#define RAM_INDX 0x00C8 // end-of-line for input pointer
#define RAM_LSXP 0x00C9 // input cursor log (row)
#define RAM_LSTP 0x00CA // input cursor log (col)
#define RAM_SFDX 0x00CB // key pressed (64 if no key)
#define RAM_CRSW 0x00D0 // input from screen/keyboard
#define RAM_PNT  0x00D1 // pointer to screen line (word size)
#define RAM_PNT_LO 0x00D1 // low byte
#define RAM_PNT_HI 0x00D2 // high byte
#define RAM_PNTR 0x00D3 // position of cursor on above line
#define RAM_QTSW 0x00D4 // 0 = direct cursor, else programmed
#define RAM_LNMX 0x00D5 // current screen line length
#define RAM_TBLX 0x00D6 // row where cursor is
#define RAM_DATA 0x00D6 // last inkey/checksum/buffer (scratch space)


static inline int getSerialBusAddrState(Emu* m) {
  return m->serialBusActiveAddress & (~0x1F);
}

static inline int getSerialBusAddrDevice(Emu* m) {
  return m->serialBusActiveAddress & 0x1F;
}

static int romLookupFileNumber(emu_t* m, int fileNumberToFind, bool clearStatus) {
  if (clearStatus)
    RAM[RAM_STATUS] = 0;
  int nFiles = RAM[RAM_LDTND];
  byte_t* fileTable = &RAM[RAM_LAT];
  for (int i=0; i < nFiles; i++) {
    if (fileTable[i] == fileNumberToFind) {
      return i;
    }
  }
  return -1;
}

static char renderDisplayChar(byte_t c) {
  char displayChar = (0x20 <= c && c < 0x7F) ? (char)c : '~';
  return displayChar;
}

// Set FA, LA, SA. (Has no return value because it operates by side effect.)
static void romFetchFileTableEntries(emu_t* m, int fileTableRow) {
  RAM[RAM_LA] = RAM[RAM_LAT + fileTableRow];
  RAM[RAM_FA] = RAM[RAM_FAT + fileTableRow];
  RAM[RAM_SA] = RAM[RAM_SAT + fileTableRow];
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"

#pragma GCC diagnostic pop

static void romCLRCH(emu_t* m) {
  byte_t outputChannel = RAM[RAM_DFLTO];
  byte_t inputChannel = RAM[RAM_DFLTN];
  if (outputChannel < 3) {
    emulateC64ROM(m, C64_ROM_CALL_UNLSN);
  }
  if (inputChannel < 3) {
    emulateC64ROM(m, C64_ROM_CALL_UNTLK);
  }
  RAM[RAM_DFLTO] = 3; // output to screen
  RAM[RAM_DFLTN] = 0; // input from keyboard
}

static void romOpen(emu_t* m) {
  int fileTableRow = romLookupFileNumber(m, RAM[RAM_LA], true);
  if (fileTableRow >= 0) {
    romError(m, 2);
    return;
  }
  unsigned nextFileIndex = RAM[RAM_LDTND]; // # of open files
  if (nextFileIndex == 10) { // max # of open files
    romError(m, 1);
    return;
  }
  RAM[RAM_LDTND]++; // increase # of open files
  RAM[RAM_LAT + nextFileIndex] = RAM[RAM_LA];
  RAM[RAM_SA] |= 0x60; // convert to serial command
  RAM[RAM_SAT + nextFileIndex] = RAM[RAM_SA];
  byte_t deviceNumber = RAM[RAM_FA];
  RAM[RAM_FAT + nextFileIndex] = deviceNumber;
  if (deviceNumber >= 8)
    diskOPEN(m);
  else
    switch (deviceNumber) {
      case 0: // keyboard
      case 3: // screen
        break; // do nothing
      case 1:
        error(m, "Datasette I/O not supported.");
      case 2:
        error(m, "RS-232C I/O not supported.");
      default:
        error(m, "Raw serial I/O not supported.");
    }
  setFlag(m, FLAG_C, false); // good return status
}

#if TRACE_ON
void romTrace(emu_t* m, const char* fmt, ...) {
  assert(m->romCallEmbeddingLevel < TRACE_BUFSIZ/2);
  char buf[TRACE_BUFSIZ];
  if (!m->traceFile) return;
  int i;
  for (i=0; i < m->romCallEmbeddingLevel-1; i++)
    buf[i] = '>';
  va_list ap;
  va_start(ap, fmt);
  int n = vsnprintf(buf+i, TRACE_BUFSIZ-i, fmt, ap);
  va_end(ap);
  snprintf(buf+i+n, TRACE_BUFSIZ-i-n, " IC:" IC_FMT, m->reg.ic);
  trace(m, true, buf);
}
#endif

void romCHKIN(emu_t* m, int logicalFileNumber) {
  int fileTableRow = romLookupFileNumber(m, logicalFileNumber, true);
  if (fileTableRow < 0) {
    romError(m, 3); // file not open
    return;
  }
  romFetchFileTableEntries(m, fileTableRow);
  unsigned deviceNumber = RAM[RAM_FA];
  switch (deviceNumber) {
    case 0:
    case 3:
      // do nothing
      break;
    case 1:
      error(m, "Datasette not supported.");
    case 2:
      error(m, "RS-232 not supported.");
    default:
      {
        A = deviceNumber;
        emulateC64ROM(m, C64_ROM_CALL_TALK);
        A = RAM[RAM_SA];
        if (A >= 0) {
          emulateC64ROM(m, C64_ROM_CALL_TKSA);
        }
      }
      break;
  }
}

void removeFileTableEntry(Emu* m, int tableIndex) {
  // Decrement the count of table indexes.
  // This new number is, for now, the index of the last entry in the file table.
  // If the file we wanted to remove was the last in the list then we're done,
  // we just needed to shrink the table. If it wasn't, then we need to move the
  // last entry to replace the one we're removing.
  int lastTableIndex = --RAM[RAM_LDTND];
  if (lastTableIndex != tableIndex) {
    RAM[RAM_LAT + tableIndex] = RAM[RAM_LAT + lastTableIndex];
    RAM[RAM_FAT + tableIndex] = RAM[RAM_FAT + lastTableIndex];
    RAM[RAM_SAT + tableIndex] = RAM[RAM_SAT + lastTableIndex];
  }
}

void romCloseSerial(Emu* m) {
  if ((RAM[RAM_SA] & 0x80) == 0) {
    A = RAM[RAM_FA];
    emulateC64ROM(m, C64_ROM_CALL_LISTEN);
    A = (RAM[RAM_SA] & 0xEF) | 0xE0;
    emulateC64ROM(m, C64_ROM_CALL_SECOND);
    emulateC64ROM(m, C64_ROM_CALL_UNLSN);
  }
  setFlag(m, FLAG_C, false);
}

void romCloseInternal(Emu* m, int fd) {
  int fileTableRow = romLookupFileNumber(m, fd, false);
  if (fileTableRow == -1)
    return;
  romFetchFileTableEntries(m, fileTableRow); 
  int devNo = RAM[RAM_FA];
  switch (devNo) {
    case 0: // keyboard
    case 3: // screen
      break; // nothing to do
    case 1: // Datasette
    case 2: // RS232
      error(m, "Unsupported device: %d", devNo);
    default: // serial
      romCloseSerial(m);
  }
  removeFileTableEntry(m, fileTableRow);
}

void romClose(Emu* m, int fd) {
  romCloseInternal(m, fd);
  setFlag(m, FLAG_C, 0); // clear carry on exit
}

void romClrch(Emu* m) {
  // unlisten output / untalk input if serial devices
  if (RAM[RAM_DFLTO] > 3) {
    emulateC64ROM(m, C64_ROM_CALL_UNLSN);
  }
  if (RAM[RAM_DFLTN] > 3) {
    emulateC64ROM(m, C64_ROM_CALL_UNTLK);
  }
  // Restore default values.
  RAM[RAM_DFLTO] = 3; // output = screen
  RAM[RAM_DFLTN] = 0; // input = keyboard
}

#if 0
void romBsoutScreen(Emu* m, byte_t character) {
  char displayChar = renderDisplayChar(character);
  romTrace(m, "Print character to screen: '%c'", displayChar);
  // TODO: Implement routine so we write to screen RAM
}

void romInputLineUntilCR(Emu* m) {
  // BASIN for keyboard, screen
  // label LOOP5
  int c = RAM[RAM_CRSW];
  if (c) {
    byte_t y = RAM[RAM_PNTR];
    byte_t a = RAM[RAM_PNT + y];
    RAM[RAM_DATA] = a;
    a &= 0x3F;
    // Original 6502 code sets flags using ASL and BIT
    // ASL
    int fC = a & 0x80; // bit 7
    // BIT
    int fN = a & 0x40; // bit 6
    int fV = a & 0x20; // bit 5
    RAM[RAM_DATA] = a << 1; // preserve side effect
    if (fN) {
      a |= 0x80;
    }
    if ((!fC || RAM[RAM_QTSW] == 0) && !fV) {
      a |= 0x40;
    }
    RAM[RAM_PNTR]++;
    // JSR QTSWC
    if (y == RAM[RAM_INDX]) {
      RAM[RAM_CRSW] = 0;
      a = 0xD;
      if (RAM[RAM_DFLTN] != 3 && RAM[RAM_DFLTO] != 3)
        JSR PRT;
      // TODO incomplete
    }
    RAM[RAM_DATA] = 0xD;
  } else {
    // LOOP3
    RAM[RAM_AUTODN] = RAM[RAM_BLNSW] = RAM[RAM_NDX];
    // TODO: incomplete
  }
}
#endif

void emulateC64ROM(emu_t* m, word_t callAddr) {
  m->romCallEmbeddingLevel++;
  switch (callAddr) {

    case C64_ROM_CALL_CHKIN:
      romTrace(m, "ROM %04X: CHKIN(X:logFiNo=%02X)", callAddr, X);
      romCHKIN(m, X);
      break;

    case C64_ROM_CALL_GETIN:
      // FIXME: RETURNING DUMMY DATA
      A = 0x30; // this is what ACS receives here in VICE
      romTrace(m, "ROM %04X: GETIN() -> %02X", callAddr, A);
      break;

    case C64_ROM_CALL_CLRCHN:
      romTrace(m, "ROM %04X: CLRCHN()", callAddr);
      romClrch(m);
      break;

    case C64_ROM_CALL_BSOUT:
      {
        int device = RAM[RAM_DFLTO];
        char displayChar = renderDisplayChar(A);
        romTrace(m, "ROM %04X: BSOUT(A=%02X '%c') [dev=%d]",
            callAddr, A, displayChar, device);
        switch (device) {
          case 0:
            error(m, "BSOUT to keyboard is invalid.");
          case 1:
          case 2:
            error(m, "BSOUT not supported on device %d.", device);
          case 3:
            romTrace(m, "ROM (BSOUT not yet supported for screen, ignored)");
            //error(m, "BSOUT not yet supported for screen.");
            // FIXME: Uncomment.
            //romBsoutScreen(m, A);
            break;
          default:
            romTrace(m, "BSOUT calls CIOUT for serial device.");
            emulateC64ROM(m, C64_ROM_CALL_CIOUT);
        }
      }
      break;

    case C64_ROM_CALL_BASIN:
      // BASIN: INPUT CHARACTER FROM CHANNEL INPUT DIFFERS FROM GET ON DEVICE
      // #0 FUNCTION WHICH IS KEYBOARD. THE SCREEN EDITOR MAKES READY AN ENTIRE
      // LINE WHICH IS PASSED CHAR BY CHAR UP TO THE CARRIAGE RETURN. OTHER
      // DEVICES ARE: 0 KEYBOARD; 1 CASSETTE #1; 2 RS232; 3 SCREEN; 4-31 SERIAL
      // BUS
      {
        int device = RAM[RAM_DFLTN];
        if (device == 0) {
        } else if (device == 3) {
        }
        char displayChar = renderDisplayChar(A);
        romTrace(m, "ROM %04X: BSOUT(A=%02X '%c') [dev=%d]",
            callAddr, A, displayChar, device);
        switch (device) {
          case 0:
            error(m, "BASIN not supported on keyboard yet.");
#if 0
            // save cursor row, column
            RAM[RAM_LSXP] = RAM[RAM_TBLX]; // row
            RAM[RAM_LSTP] = RAM[RAM_PNTR]; // col
            romInputLineUntilCR(m);
            break;
#endif
          case 1:
          case 2:
            error(m, "BASIN not supported on device %d.", device);
          case 3:
            error(m, "BASIN not supported on screen yet.");
#if 0
            RAM[RAM_CRSW] = device;
            RAM[RAM_INDX] = RAM[RAM_LNMX];
            romInputLineUntilCR(m);
            break;
#endif
          default: // serial device
            if (RAM[RAM_STATUS] == 0) {
              romTrace(m, "BASIN calls ACPTR for serial device.");
              emulateC64ROM(m, C64_ROM_CALL_ACPTR);
            } else {
              // error
              A = 0xD;
              setFlag(m, FLAG_C, false);
            }
            break;
        }
      }
      break;

    case C64_ROM_CALL_CIOUT:
      {
        byte_t device = getSerialBusAddrDevice(m);
        char displayChar = renderDisplayChar(A);
        romTrace(m, "ROM %04X: CIOUT(A:data=%02X '%c') [dev=%d]",
            callAddr, A, displayChar, device);
        if (device <= 3)
          error(m, "Invalid device for CIOUT: %d (must be serial device)", device);
        if (device < 8)
          error(m, "CIOUT not supported on device %d.", device);
        if (getSerialBusAddrState(m) != SERIAL_BUS_STATE_LISTENER)
          error(m, "CIOUT called while no device is listening.");
        diskCIOUT(m, A);
      }
      break;

    case C64_ROM_CALL_SECOND:
      {
        romTrace(m, "ROM %04X: SECOND(A:sec=%02X)", callAddr, A);
        int device = getSerialBusAddrDevice(m);
        if (getSerialBusAddrState(m) != SERIAL_BUS_STATE_LISTENER)
          error(m, "SECOND called while no device is listening.");
        if (device >= 8)
          diskSECOND(m, A);
        else
          error(m, "SECOND not supported on device %d.", device);
      }
      break;

    case C64_ROM_CALL_LISTEN:
      romTrace(m, "ROM %04X: LISTEN(A:dev=%02X)", callAddr, A);
      {
        byte_t device = A;
        RAM[RAM_FA] = device;
        m->serialBusActiveAddress = A | SERIAL_BUS_STATE_LISTENER;
        if (device == 8) {
          diskLISTEN(m);
        } else {
          error(m, "LISTEN not supported on device %d.", device);
        }
      }
      break;

    case C64_ROM_CALL_UNLSN:
      {
        byte_t device = RAM[RAM_FA];
        romTrace(m, "ROM %04X: UNLSN() [dev=%d]", callAddr, device);
        if (device >= 8)
          diskUNLSN(m);
        else
          error(m, "UNLSN not supported on device %d.", device);
        m->serialBusActiveAddress = 0;
      }
      break;

    case C64_ROM_CALL_TALK:
      romTrace(m, "ROM %04X: TALK(A:dev=%02X)", callAddr, A);
      RAM[RAM_FA] = A; // device number XXX is this right?
      m->serialBusActiveAddress = A | SERIAL_BUS_STATE_TALKER;
      break;

    case C64_ROM_CALL_TKSA:
      {
        int device = getSerialBusAddrDevice(m);
        romTrace(m, "ROM %04X: TKSA(A:sec=%02X) [dev=%02X]", callAddr, A, device);
        if (getSerialBusAddrState(m) != SERIAL_BUS_STATE_TALKER)
          error(m, "TKSA called while no device is talking.");
        if (device >= 8)
          diskTKSA(m, A);
        else
          error(m, "TKSA not supported for device %d.", A);
      }
      break;

    case C64_ROM_CALL_ACPTR:
      {
        // Returns a received byte in A.
        int device = getSerialBusAddrDevice(m);
        if (getSerialBusAddrState(m) != SERIAL_BUS_STATE_TALKER)
          error(m, "ACPTR called while no device is talking.");
        // I think the secondary for a disk command is 0x60 | channel.
        if (device >= 8)
          A = diskACPTR(m);
        else
          error(m, "ROM: ACPTR not supported on device %d.", device);
        setFlag(m, FLAG_C, false); // no error
        romTrace(m, "ROM %04X: ACPTR() [dev=%02X] -> %02X", callAddr, device, A);
      }
      break;

    case C64_ROM_CALL_UNTLK:
      romTrace(m, "ROM %04X: UNTLK()", callAddr);
      m->serialBusActiveAddress = 0;
      // nothing else to do
      break;

    case C64_ROM_CALL_SETNAM:
      {
        assert(A <= 16);
        RAM[RAM_FNLEN] = A; // filename length
        RAM[RAM_FNADR] = X; // filename address, lo
        RAM[RAM_FNADR+1] = Y; // filename address, hi
        char filename[17];
        memcpy(filename, &RAM[toWord(X, Y)], A);
        filename[A] = 0;
        romTrace(m, "ROM %04X: SETNAM(A:fnLen=%02X,X:fnAdrLo=%02X,Y:fnAdrHi=%02X) '%s'",
            callAddr, A, X, Y, filename);
      }
      break;

    case C64_ROM_CALL_SETLFS:
      romTrace(m, "ROM %04X: SETLFS(A:logFiNo=%02X,X:dev=%02X,Y:sec=%02X)",
          callAddr, A, X, Y);
      RAM[RAM_LA] = A; // logical file number
      RAM[RAM_FA] = X; // device number
      RAM[RAM_SA] = Y; // command / secondary address
      break;

    case C64_ROM_CALL_LOAD:
      romTrace(m, "ROM %04X: LOAD(A:vfy=%02X,X:adrHi=%02X,Y:adrLo=%02X)",
          callAddr, A, X, Y);
      {
        byte_t dev = RAM[RAM_FA];
        if (dev != 8)
          error(m, "Load only supports device 8, selected device %d", dev);
        RAM[RAM_VERCK] = A;
        RAM[RAM_MEMUSS] = X;
        RAM[RAM_MEMUSS+1] = Y;
        // TODO: Call loadPRG or something like that.
        // Need to port Lua load file code.
        // set RAM_END_PROG to the end of the loaded file (EAL/EAH)
      }
      break;

    case C64_ROM_CALL_OPEN:
      romTrace(m, "ROM %04X: OPEN()", callAddr);
      romOpen(m);
      break;

    case C64_ROM_CALL_CLOSE:
      romTrace(m, "ROM %04X: CLOSE(A:fd=%02X)", callAddr, A);
      romClose(m, A);
      break;

    case C64_ROM_CALL_CLALL:
      romTrace(m, "ROM %04X: CLALL()", callAddr);
      A = 0;
      RAM[RAM_LDTND] = A;
      emulateC64ROM(m, C64_ROM_CALL_CLRCHN);
      break;

    default:
      error(m, "Unsupported ROM procedure: %04X", callAddr);
  }
  assert(m->romCallEmbeddingLevel > 0);
  m->romCallEmbeddingLevel--;
}

void romError(emu_t* m, int errorNumber) {
  romCLRCH(m);
  trace(m, true, "CBM I/O ERROR #%d: %s", errorNumber, c64RomErrors[errorNumber]);
  setFlag(m, FLAG_C, true); // set carry to mark error condition
}

// RESOLVE ADDRESSING MODES

static inline word_t deref(emu_t* m, word_t pointer) {
  return toWord(RAM[pointer], RAM[pointer+1]);
}

