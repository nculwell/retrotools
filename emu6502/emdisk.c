
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <malloc.h>
#include <assert.h>
#include <stdarg.h>
#include <string.h>
#include <memory.h>
#include <ctype.h>

#include "em.h"
#include "emtrace.h"

#define DIRECTORY_ENTRY_SIZE 0x20
#define MAX_DIRENTRY_COUNT 20 // real limit is 144+ but I won't need more
#define D64_SIZE 0x2AB00
#define MAX_TRACK_NUMBER 40

#define DIRENTRY_FLAG_LOCKED 0x40
#define DIRENTRY_FLAG_CLOSED 0x80

enum {
  FILETYPE_DEL = 0,
  FILETYPE_SEQ,
  FILETYPE_PRG,
  FILETYPE_USR,
  FILETYPE_REL,
  FILETYPE_COUNT,
};

const char* FILETYPE_NAMES[] = {
  "DEL",
  "SEQ",
  "PRG",
  "USR",
  "REL",
};

static const char* getFiletypeName(emu_t* m, unsigned filetypeID) {
  if (filetypeID >= FILETYPE_COUNT)
    error(m, "Invalid filetype ID: %d", filetypeID);
  return FILETYPE_NAMES[filetypeID];
}

// Compute the offset in bytes of the given track/sector relative to the start
// of the disk image.
static unsigned trackAndSectorAddr(unsigned track, unsigned sector) {
  assert(track <= MAX_TRACK_NUMBER);
  assert(sector < TRACK_INFO[track].sectorCount);
  unsigned trackAddr = TRACK_INFO[track].offsetInBytes;
  return trackAddr + sector * SECTOR_SIZE;
}

// Check that the disk image has an expected size.
// In fact there might be different possible sizes but this is the size I
// usually see so I won't change it unless I need to.
void checkDiskSize(emu_t* m, const buf_t* disk) {
  assert(disk);
  if (disk->len != D64_SIZE)
    error(m, "Attached disk image is wrong size.");
}

// Disk commands take args in different formats.
// See page 51 of the Commodore 1541 Disk Drive User's Guide (1982)
// for a summary.
// (bin) = binary: each arg is a number encoded as a byte
//   Looks like: "M-W<$6A><$C5>"
// (dec) = decimal; each arg is a number encoded as PETSCII text
//   Looks like: "B-P:2,0"
enum DiskCommands {
  DISK_CMD_INVALID = 0,
  DISK_CMD_BLOCK_ALLOCATE,  // B-A: (dec) drive; track; block
  DISK_CMD_BLOCK_EXECUTE,   // B-E: (dec) channel; drive; track; block
  DISK_CMD_BLOCK_FREE,      // B-F: (dec) drive; track; block
  DISK_CMD_BLOCK_READ,      // B-R: (dec) channel; drive; track; block
  DISK_CMD_BLOCK_WRITE,     // B-W: (dec) channel; drive; track; block
  DISK_CMD_BUFFER_POINTER,  // B-P: (dec) channel; location
  DISK_CMD_MEMORY_EXECUTE,  // M-E: (bin) addr lo byte; hi byte
  DISK_CMD_MEMORY_READ,     // M-R: (bin) addr lo byte; hi byte
  DISK_CMD_MEMORY_WRITE,    // M-W: (bin) addr lo byte; hi byte
  DISK_CMD_U1, // U1: (dec) channel; drive; track; block (aka UA)
  DISK_CMD_U2, // U2: (dec) channel; drive; track; block (aka UB)
  DISK_CMD_U9, // U9: no args [disk soft reset command]  (aka UI)
  DISK_CMD_UJ, // UJ: no args [old-style disk soft reset command]
  // TODO: others I see in the manual that aren't even parsed right now
  DISK_CMD_NEW,         // N: no args
  DISK_CMD_COPY,        // C: "C:newfile=:originalFile"
  DISK_CMD_RENAME,      // R: "C:newname=oldname"
  DISK_CMD_SCRATCH,     // S: "S:filename"
  DISK_CMD_INITIALIZE,  // I: no args
  DISK_CMD_VALIDATE,    // V: no args
  DISK_CMD_DUPLICATE,   // not for single drives
  DISK_CMD_POSITION,    // P: (bin) channel#; rec#lo; rec#hi; position
  // I could also represent more user commands but I won't for now because I'm
  // never going to implement them.
  DISK_CMD_COUNT
};

static const char* DiskCommandNames[] = {
  "<INVALID>",
  "BLOCK-ALLOCATE",
  "BLOCK-EXECUTE",
  "BLOCK-FREE",
  "BLOCK-READ",
  "BLOCK-WRITE",
  "BUFFER-POINTER",
  "MEMORY-EXECUTE",
  "MEMORY-READ",
  "MEMORY-WRITE",
  "U1",
  "U2",
  "U9",
  "UJ",
  "NEW",
  "COPY",
  "RENAME",
  "SCRATCH",
  "INITIALIZE",
  "VALIDATE",
  "DUPLICATE",
  "POSITION",
};

static int parseDiskCmdName(emu_t* m, const byte_t* b, int* argStartIndex) {
  // The drive also accepts full-length names, but I'm not going to implement
  // those unless I need to. I doubt they're used much.
  // This code assumes command names are correct, so it does the bare minimum
  // to differentiate them.
  *argStartIndex = 1;
  int cmdID = 0;
  switch (b[0]) {

    case 'N': cmdID = DISK_CMD_NEW; goto optionalDriveNumber;
    case 'C': cmdID = DISK_CMD_COPY; goto optionalDriveNumber;
    case 'R': cmdID = DISK_CMD_RENAME; goto optionalDriveNumber;
    case 'S': cmdID = DISK_CMD_SCRATCH; goto optionalDriveNumber;
    case 'I': cmdID = DISK_CMD_INITIALIZE; goto optionalDriveNumber;
    case 'V': cmdID = DISK_CMD_VALIDATE; goto optionalDriveNumber;
    case 'P': cmdID = DISK_CMD_POSITION; goto optionalDriveNumber;

    case 'B':
              *argStartIndex = 3;
              if (b[1] == '-')
                switch(b[2]) {
                  case 'A': return DISK_CMD_BLOCK_ALLOCATE;
                  case 'E': return DISK_CMD_BLOCK_EXECUTE;
                  case 'F': return DISK_CMD_BLOCK_FREE;
                  case 'R': return DISK_CMD_BLOCK_READ;
                  case 'W': return DISK_CMD_BLOCK_WRITE;
                  case 'P': return DISK_CMD_BUFFER_POINTER;
                }
              break;

    case 'M':
              *argStartIndex = 3;
              if (b[1] == '-')
                switch(b[2]) {
                  case 'E': return DISK_CMD_MEMORY_EXECUTE;
                  case 'R': return DISK_CMD_MEMORY_READ;
                  case 'W': return DISK_CMD_MEMORY_WRITE;
                }
              break;

    case 'U':
              *argStartIndex = 2;
              switch (b[1]) {
                case 'A':
                case '1': cmdID = DISK_CMD_U1; goto expectColon;
                case 'B':
                case '2': cmdID = DISK_CMD_U2; goto expectColon;
                case 'I':
                case '9': return DISK_CMD_U9;
                case 'J': return DISK_CMD_UJ;
              }
              break;

  }
  return 0; // not matched

optionalDriveNumber:
  if (isdigit(b[1])) {
    int driveNumber = b[1] - '0';
    if (driveNumber != 0)
      error(m, "Drive number %d specified in command, but only drive 0 is valid.", driveNumber);
    (*argStartIndex)++;
  }

  byte_t nextChar;
expectColon:
  nextChar = b[*argStartIndex];
  if (nextChar != 0 && nextChar != ':')
    error(m, "Colon expected after drive command.");

  return cmdID;
}

#define DISK_CMD_MAX_ARG_COUNT 4

static int parseDecimalArgs(emu_t* m, const byte_t* argStart, byte_t* args) {
  int argIndex = 0;
  unsigned argValue;
  const byte_t* p = argStart;
  while (*p) {
    if (*p != ':' && *p != ',')
      return -1; // invalid delimiter
    if (argIndex == DISK_CMD_MAX_ARG_COUNT)
      return -1; // too many args
    argValue = 0;
    // Skip leading spaces.
    while (isspace(*(++p))) {
      // skip
    }
    --p; // Back up because the spaces scan advanced the pointer.
    while (isdigit(*(++p)))
      argValue = (10 * argValue) + (*p - '0');
    // We're assuming these args must be in range 0-255, check this.
    if (argValue > UINT8_MAX)
      error(m, "Decimal argument out of range: %d", argValue);
    args[argIndex] = argValue;
    argIndex++;
  }
  return argIndex;
}

static int parseBinaryArgs(
    const byte_t* argStart,
    const byte_t* argEnd,
    byte_t* args,
    int nArgsExpected)
{
  assert(nArgsExpected <= DISK_CMD_MAX_ARG_COUNT);
  int nArgsFound = argEnd - argStart;
  assert(nArgsFound >= 0);
  if (nArgsFound != nArgsExpected)
    return -1; // wrong number of arguments
  for (int i=0; i < nArgsExpected; i++) {
    args[i] = argStart[i];
  }
  return nArgsExpected;
}

static int parseDiskCmdArgs(
    emu_t* m, int diskCmd, const byte_t* argStart, const byte_t* argEnd, byte_t* args) {
  switch (diskCmd) {

    // commands with no args
    
    case DISK_CMD_NEW:
    case DISK_CMD_INITIALIZE:
    case DISK_CMD_VALIDATE:
    case DISK_CMD_U9:
    case DISK_CMD_UJ:
      return 0;

      // commands with string args

      // There's probably no need to implement any of these commands so I won't
      // bother parsing them. If I were going to do it I'd use the argument
      // bytes as (offset1, length1, offset2, length2) to store the bounds of
      // the argument strings.

    case DISK_CMD_COPY:
      error(m, "Disk command COPY not implemented.");
    case DISK_CMD_RENAME:
      error(m, "Disk command RENAME not implemented.");
    case DISK_CMD_SCRATCH:
      error(m, "Disk command SCRATCH not implemented.");

      // commands with decimal args

    case DISK_CMD_BLOCK_READ:
    case DISK_CMD_BLOCK_WRITE:
    case DISK_CMD_BLOCK_ALLOCATE:
    case DISK_CMD_BLOCK_FREE:
    case DISK_CMD_BUFFER_POINTER:
    case DISK_CMD_U1:
    case DISK_CMD_U2:
    case DISK_CMD_BLOCK_EXECUTE:
      return parseDecimalArgs(m, argStart, args);

      // commands with binary args

    case DISK_CMD_POSITION:
      return parseBinaryArgs(argStart, argEnd, args, 4);
    case DISK_CMD_MEMORY_READ:
      // TODO: can take an optional third parameter, the data length; if not
      // sent then it defaults to 1.
      return parseBinaryArgs(argStart, argEnd, args, 2);
    case DISK_CMD_MEMORY_EXECUTE:
      return parseBinaryArgs(argStart, argEnd, args, 2);

    case DISK_CMD_MEMORY_WRITE:
      // This one has trailing data, so we need to validate it differently.
      if (argEnd - argStart < 3)
        return -1;
      for (int i=0; i < 3; i++)
        args[i] = argStart[i];
      // The third argument is the expected length of the trailing data,
      // validate this against what was actually sent.
      if (argEnd - argStart - 3 != args[2])
        error(m, "MEMORY-WRITE has incorrect data length.");
      return 3;

    default:
      error(m, "Unimplemented disk command: ID %d", diskCmd);
  }
}

void renderPetscii(char* buf, int bufSize, const byte_t* src, int srcLen) {
  // leave space for incomplete marker and null terminator
  int bufLimit = bufSize - 2;
  int renderLen = srcLen < bufLimit ? srcLen : bufLimit;
  int i;
  for (i=0; i < renderLen; i++) {
    byte_t c = src[i];
    if (c < 0x20 || c >= 0x7E)
      buf[i] = '~';
    else
      buf[i] = src[i];
  }
  if (renderLen == bufLimit)
    buf[i++] = '|';
  buf[i] = 0;
}

unsigned getChannelBufferID(emu_t* m, unsigned channel) {
  unsigned bufferID;
  for (bufferID=0; bufferID < DISKDRIVE_BUFFER_COUNT; bufferID++) {
    if (channel == m->diskdrive.diskBufferChannels[bufferID])
      break;
  }
  if (bufferID == DISKDRIVE_BUFFER_COUNT)
    error(m, "Channel not mapped to a buffer: %d", channel);
  return bufferID;
}

// COMPUTE! article about the drive number bug:
// https://archive.org/details/1985-10-compute-magazine/page/n83
// drives numbers are 0 and 1
static void execDiskCmd(emu_t* m, int cmd, byte_t* args, byte_t* argStart, unsigned argLen) {
  switch (cmd) {

    case DISK_CMD_UJ:
    case DISK_CMD_INITIALIZE:
      // Reset drive to initial state.
      // The mounted image shouldn't be reset (that represents physically
      // inserting the disk into the drive) and the buffer contents don't need
      // to be cleared.
      {
        DiskDrive* d = &m->diskdrive;
        d->commandBufferPointer = 0;
        for (int i=0; i < DISKDRIVE_BUFFER_COUNT; i++) {
          d->diskBufferPointers[i] = 0;
          d->diskBufferChannels[i] = 0;
        }
        d->commState = 0;
        d->secondAddress = 0;
      }
      romTrace(m, "ROM/disk: drive init");
      break;

    case DISK_CMD_BLOCK_ALLOCATE:  // drive; track; block
      error(m, "Disk command BLOCK-ALLOCATE not implemented.");
      break;
    case DISK_CMD_BLOCK_EXECUTE:   // channel; drive; track; block
      error(m, "Disk command BLOCK-EXECUTE not implemented.");
      break;
    case DISK_CMD_BLOCK_FREE:      // drive; track; block
      error(m, "Disk command BLOCK-FREE not implemented.");
      break;
    case DISK_CMD_BLOCK_READ:      // channel; drive; track; block
      error(m, "Disk command BLOCK-READ not implemented.");
      break;
    case DISK_CMD_BLOCK_WRITE:     // channel; drive; track; block
      error(m, "Disk command BLOCK-WRITE not implemented.");
      break;

    case DISK_CMD_BUFFER_POINTER:  // B-P: channel; location
      {
        // TODO: Associate channel with buffer in OPEN command.
        int channel = args[0];
        int location = args[1];
        int bufferID = getChannelBufferID(m, channel);
        m->diskdrive.diskBufferPointers[bufferID] = location;
      }
      break;

    case DISK_CMD_MEMORY_EXECUTE:  // M-E: addr lo byte; hi byte
      {
        word_t addr = toWord(args[0], args[1]);
        romTrace(m, "MEMORY-EXECUTE(addr=%04X)", addr);
        error(m, "MEMORY-EXECUTE not implemented.");
      }
      break;

    case DISK_CMD_MEMORY_READ:     // M-R: addr lo byte; hi byte
      {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-variable"
        // Optional 3rd argument specifies data length, default is 1.
        word_t addr = toWord(args[0], args[1]);
        unsigned len = argLen == 3 ? args[2] : 1;
        romTrace(m, "ROM/disk: MEMORY-READ(addr=%04X,len=%02X)", addr, len);
        if (len > 1)
          error(m, "ROM/disk: MEMORY-READ only supports length=1");
        // XXX: The value that ACS is expecting is in C6C4 so we give it what
        // it wants. This behavior is seen in the loader bytecode.
        m->diskdrive.commandRecv = RAM[0xC6C4];
#pragma GCC diagnostic pop
      }
      break;

    case DISK_CMD_MEMORY_WRITE:    // M-W: addr lo byte; hi byte; data len; data...
      {
#define MW_BUFSIZ 16
        const byte_t* data = argStart + 3;
        word_t addr = toWord(args[0], args[1]);
        int len = args[2];
        if (len > 1) {
          char buf[MW_BUFSIZ];
          renderPetscii(buf, MW_BUFSIZ, data, len);
          romTrace(m, "ROM/disk: MEMORY-WRITE(addr=%04X,len=%02X,data=%s)", addr, len, buf);
        } else {
          romTrace(m, "ROM/disk: MEMORY-WRITE(addr=%04X,len=%02X,data=%02X)", addr, len, data[0]);
        }
        // XXX: Most of the time it won't make sense to actually write here
        // because the locations are ROM variables and we're not using the ROM.
        // I might have to implement some of these with special handling.
      }
      break;

    case DISK_CMD_U1: // channel; drive; track; block
      {
        // TODO: Associate channel with buffer in OPEN command.
        unsigned channel = args[0];
        unsigned drive = args[1];
        unsigned track = args[2];
        unsigned sector = args[3];
        unsigned bufferID = getChannelBufferID(m, channel);
        if (drive != 0)
          error(m, "Only one drive is supported.");
        // Fill the buffer.
        if (m->diskdrive.mountedImageData == NULL)
          error(m, "Disk is not ready to read.");
        unsigned sectorAddr = trackAndSectorAddr(track, sector);
        assert(sectorAddr + SECTOR_SIZE <= D64_SIZE);
        memcpy(m->diskdrive.diskBuffers[bufferID],
            m->diskdrive.mountedImageData->data + sectorAddr,
            SECTOR_SIZE);
        m->diskdrive.diskBufferPointers[bufferID] = 0xFF;
        romTrace(m, "ROM/disk: U1 read TS $%02X:%02X [%X] into buffer %d.", track, sector, sectorAddr, bufferID);
      } 
      break;

    case DISK_CMD_U2: // channel; drive; track; block
      error(m, "U2 not implemented.");
      break;

    default:
      error(m, "Invalid disk command ID: %d", cmd);
  }
}

static void diskCommand(emu_t* m) {
  // TODO: Return error codes instead of crashing on errors.
  byte_t* b = m->diskdrive.commandBuffer;
  int len = m->diskdrive.commandBufferPointer;
  byte_t* e = b + len;
  *e = 0;
  int argStartIndex;
  int diskCmdID = parseDiskCmdName(m, b, &argStartIndex);
  assert(diskCmdID < DISK_CMD_COUNT);
  const char* diskCmdName = DiskCommandNames[diskCmdID];
  if (diskCmdID == 0)
    error(m, "Invalid disk command: %s", b);
  romTrace(m, "ROM/disk: COMMAND [%s] \"%s\"", diskCmdName, b);
  byte_t args[DISK_CMD_MAX_ARG_COUNT];
  int argCount = parseDiskCmdArgs(m, diskCmdID, b + argStartIndex, e, args);
  if (argCount < 0)
    error(m, "Invalid disk command arguments: %s", b);
  char buf[DISKDRIVE_COMMAND_BUFFER_SIZE+1];
  renderPetscii(buf, DISKDRIVE_COMMAND_BUFFER_SIZE+1, b, len);
  romTrace(m, "ROM/disk: %s", buf);
  execDiskCmd(m, diskCmdID, args, b + argStartIndex, e - (b + argStartIndex));
}

#define DISK_DIRECTORY_FILENAME_LEN 16

typedef struct {
  byte_t filetypeID;
  byte_t flags;
  byte_t startTrack;
  byte_t startSector;
  word_t fileSizeInSectors;
  char filetypeName[4];
  byte_t relSideTrack;
  byte_t relSideSector;
  byte_t relRecordLen;
  char filename[DISK_DIRECTORY_FILENAME_LEN+1]; // extra space for null terminator
} direntry_t;

typedef struct {
  unsigned entryCount;
  direntry_t entries[MAX_DIRENTRY_COUNT];
} directory_t;

void readFileDirectory(emu_t* m, int deviceNumber, directory_t* dir) {
  // Check state.
  if (deviceNumber != 8)
    error(m, "Only one drive is supported.");
  if (m->diskdrive.mountedImagePath == NULL)
    error(m, "No disk mounted.");
  if (m->diskdrive.mountedImageData == NULL)
    m->diskdrive.mountedImageData = readFile(m->diskdrive.mountedImagePath);
  checkDiskSize(m, m->diskdrive.mountedImageData);
  // Initial setup.
  const byte_t* d64 = m->diskdrive.mountedImageData->data;
  // Sector 0 contains a "next track/sector" notation but it's ignored.
  unsigned nextTrack = 18; 
  unsigned nextSector = 1;
  // Iterate over entries.
  dir->entryCount = 0;
  for (;;) {
    if (nextTrack == 0)
      break;
    if (nextTrack > MAX_TRACK_NUMBER)
      error(m, "Track number out of range: %d", nextTrack);
    unsigned secOff = trackAndSectorAddr(nextTrack, nextSector);
    assert(secOff + SECTOR_SIZE <= m->diskdrive.mountedImageData->len);
    nextTrack = d64[secOff];
    nextSector = d64[secOff + 1];
    for (
        unsigned off = secOff;
        off < secOff + SECTOR_SIZE;
        off += DIRECTORY_ENTRY_SIZE
        )
    {
      const byte_t* ent = &d64[off];
      unsigned fileTrack = ent[3];
      unsigned fileSector = ent[4];
      if (fileTrack > 0) {

        if (fileTrack > MAX_TRACK_NUMBER)
          error(m, "Track number out of range: %d", fileTrack);
        direntry_t* entry = &dir->entries[dir->entryCount];
        unsigned filetypeByte = ent[2];
        unsigned filetypeID = filetypeByte & 7; // 3 low bits
        const char* filetypeName = getFiletypeName(m, filetypeID);
        entry->filetypeID = filetypeID;
        entry->flags = filetypeByte & (~7); // remove filetypeID
        strcpy(entry->filetypeName, filetypeName);
        // Read the filename. It is a 16-byte fixed-width field, and empty
        // bytes are filled with 0xA0 (called a "shifted space", 0x20 | 0x80).
        // We convert the padding characters to null bytes so that C code can
        // work with the filename. This may clash with filenames that contain 0
        // characters, but we won't worry about that. The filename is actually
        // PETSCII, not ASCII, but we won't worry about that either.
        for (int i=0; i < DISK_DIRECTORY_FILENAME_LEN; i++) {
          byte_t c = ent[5+i];
          if (c == 0xA0) // padding character
            entry->filename[i] = 0;
          else
            entry->filename[i] = (char)c;
        }
        entry->filename[DISK_DIRECTORY_FILENAME_LEN] = 0;
        entry->fileSizeInSectors = toWord(ent[0x1E], ent[0x1E + 1]);
        entry->startTrack = fileTrack;
        entry->startSector = fileSector;
        if (filetypeID == FILETYPE_REL) {
          entry->relSideTrack = ent[0x15];
          entry->relSideSector = ent[0x15+1];
          entry->relRecordLen = ent[0x17];
        }
        dir->entryCount++;

      }
    }
  }
}

buf_t* readFileFromDiskImage(buf_t* disk, unsigned startTrack, unsigned startSector) {
  byte_t* d = disk->data;
  buf_t* file = bufCreate();
  unsigned blockAddr = trackAndSectorAddr(startTrack, startSector);
  // The track/sector of the next sector in the chain is stored in the first
  // two bytes of each sector.
  unsigned nextBlockTrack = d[blockAddr];
  unsigned nextBlockSector = d[blockAddr+1];
  while (nextBlockTrack > 0) {
    blockAddr = trackAndSectorAddr(nextBlockTrack, nextBlockSector);
    nextBlockTrack = d[blockAddr];
    nextBlockSector = d[blockAddr+1];
    size_t bytesInSector;
    if (nextBlockTrack == 0) {
      // In the last block in the chain, the "sector number" is really the
      // number of bytes in this sector that are actually part of the file.
      bytesInSector = nextBlockSector;
    } else {
      // Otherwise, read the entire sector.
      bytesInSector = SECTOR_SIZE;
    }
    bufEnsureExtraCap(file, bytesInSector-2);
    memcpy(file->data + file->len, d + blockAddr, bytesInSector-2);
  }
  return file;
}

void diskSECOND(emu_t* m, byte_t second) {
  assert(RAM[RAM_FA] >= 8);
  m->diskdrive.secondAddress = second;
}

void diskTKSA(emu_t* m, byte_t second) {
  assert(RAM[RAM_FA] >= 8);
  m->diskdrive.secondAddress = second;
}

byte_t diskACPTR(emu_t* m) {
  assert(RAM[RAM_FA] >= 8);
  unsigned channel = m->diskdrive.secondAddress & 0x0F;
  romTrace(m, "ROM/disk: ACPTR [channel=%02X]", channel);
  byte_t responseByte;
  if (channel == 15) {
    responseByte = m->diskdrive.commandRecv;
  } else {
    unsigned bufferID = getChannelBufferID(m, channel);
    //if (m->diskdrive.readBufferNxt >= m->diskdrive.readBufferLen)
    //  error(m, "ACPTR on disk drive: no data available in buffer.");
    responseByte = m->diskdrive.diskBuffers[bufferID][m->diskdrive.diskBufferPointers[bufferID]];
    m->diskdrive.diskBufferPointers[bufferID]++;
  }
  return responseByte;
}

void diskCIOUT(emu_t* m, byte_t data) {
  assert(RAM[RAM_FA] >= 8);
  if (m->diskdrive.commandBufferPointer == DISKDRIVE_COMMAND_BUFFER_SIZE)
    error(m, "Disk drive command buffer is full.");
  m->diskdrive.commandBuffer[m->diskdrive.commandBufferPointer++] = data;
}

void diskOpenFile(emu_t* m, unsigned channel) {
  byte_t* b = m->diskdrive.commandBuffer;
  int len = m->diskdrive.commandBufferPointer;
  if (len == 0)
    error(m, "OPEN with empty filename.");
  b[len] = 0; // add null term
  if (b[0] == '#') {
    int bufferRequest = -1;
    if (len > 2)
      error(m, "Invalid OPEN buffer argument: %s", b);
    if (len == 2) {
      bufferRequest = b[1] - '0';
      if (bufferRequest < 0 || bufferRequest >= DISKDRIVE_BUFFER_COUNT)
        error(m, "Invalid buffer number: %c", b[1]);
      if (m->diskdrive.diskBufferChannels[bufferRequest] != 0)
        error(m, "Buffer #%d is in use.", bufferRequest);
    } else {
      for (int i=0; i < DISKDRIVE_BUFFER_COUNT; i++) {
        if (m->diskdrive.diskBufferChannels[i] == 0) {
          bufferRequest = i;
          break;
        }
      }
      if (bufferRequest == -1)
        error(m, "No buffers free.");
    }
    m->diskdrive.diskBufferChannels[bufferRequest] = channel;
  }
}

void diskLISTEN(emu_t* m) {
  // Clear the disk buffer to prepare to receive a command.
  m->diskdrive.commandBufferPointer = 0;
  for (int i=0; i < DISKDRIVE_COMMAND_BUFFER_SIZE; i++) {
    m->diskdrive.commandBuffer[i] = 0;
  }
}

void diskUNLSN(emu_t* m) {
  unsigned sec = m->diskdrive.secondAddress;
  unsigned command = sec & 0xF0;
  unsigned channel = sec & 0x0F;
  trace(m, true, "ROM/disk: DISK UNLSN: command=$%02X, channel=%d", command, channel);
  switch (command) {
    case 0x60: // serial command
      if (channel == 15) {
        // command channel
        diskCommand(m);
      } else {
        diskCommand(m);
        //diskOpenFile(m, channel);
      }
      break;
    case 0xE0: // CLOSE
      // Do nothing.
      // TODO: Flush buffer to disk if we're going to emulating disk writing.
      break;
    case 0xF0: // OPEN
      if (channel == 15) {
        // command channel
        diskCommand(m);
      } else {
        diskOpenFile(m, channel);
      }
      break;
    default:
      error(m, "OPEN: unhandled command %02X.", command);
  }
}

void diskOPEN(emu_t* m) {
  if ((int8_t)RAM[RAM_SA] < 0)
    return;
  int filenameLength = RAM[RAM_FNLEN];
  if (filenameLength == 0)
    return;
  RAM[RAM_STATUS] = 0;
  A = RAM[RAM_FA];
  emulateC64ROM(m, C64_ROM_CALL_LISTEN);
  A = RAM[RAM_SA];
  A |= 0xF0;
  emulateC64ROM(m, C64_ROM_CALL_SECOND);
  if ((int8_t)RAM[RAM_STATUS] < 0) {
    romError(m, 5);
    return;
  }
  // Send filename address to device.
  word_t filenameAddress = toWord(RAM[RAM_FNADR], RAM[RAM_FNADR+1]);
  for (int i=0; i < filenameLength; i++) {
    A = RAM[filenameAddress + i];
    emulateC64ROM(m, C64_ROM_CALL_CIOUT);
  }
  emulateC64ROM(m, C64_ROM_CALL_UNLSN);
}

