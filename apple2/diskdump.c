// vim: et ts=8 sts=2 sw=2

// diskdump.c
// Author: Nathan Culwell-Kanarek
// Date: 2021-11-05
// I place this code in the public domain.
// Sources of documentation:
//   DOS 3.2 Instruction and Reference Manual
//   Beneath Apple DOS

// Diskette directory:
// 00     Not used
// 01     Next track in directory
// 02     Next sector in directory (0/0 if none)
// 03-0A  Unused
// 0B-2D  Entry #1
// 2E-50  Entry #2
// 51-73  Entry #3
// 74-96  Entry #4
// 97-B9  Entry #5
// BA-DC  Entry #6
// DD-FF  Entry #7

// Entry:
// 00     First track/sector list track (FF if deleted, 0 if not created)
// 01     First track/sector list sector
// 02     File type
// 03-20  File name
// 21     Sector count (size in sectors mod 256)
// 22     End mark (normally 0, but changed to first block track if deleted)

// File type:
// BIT  CATALOG   MEANING
// 7    *         Locked
// 6-3            Reserved
// 2    B         Binary
// 1    A         Applesoft BASIC
// 0    I         Integer BASIC
//      T         Text file (if bits 0-6 are 0)

// File's track/sector list:
// 00     Unused
// 01     Next block track
// 02     Next block sector (0/0 if none)
// 03-0B  Unused
// 0C     Sector #1 track
// 0D     Sector #1 sector
// 0E-0F  Sector #2 track/sector
// 10-11  Sector #3 track/sector
// ...
// FE-FF  Sector #122 track/sector

#include "shared.h"

// In a DSK image tracks have 16 sectors.
// DOS 3.2 only supports 13 sectors per track (according to the manual).
#define TRACK_SIZE (16 * SECTOR_SIZE)
#define TRACK_COUNT 35
#define SECTORS_PER_TRACK 13
#define ENTRIES_PER_SECTOR 7
#define FILENAME_LEN (sizeof(((DirectoryEntry*)0)->file_name))
#define TSLIST_SECTOR_COUNT 122

struct TrackAndSector {
  unsigned char track;
  unsigned char sector;
} __attribute__((packed));
typedef struct TrackAndSector TrackAndSector;

struct TrackSectorList {
  unsigned char unused1;
  TrackAndSector next_tslist_link;
  unsigned char unused2[9];
  TrackAndSector sectors[TSLIST_SECTOR_COUNT];
} __attribute__((packed));
typedef struct TrackSectorList TrackSectorList;

struct DirectoryEntry {
  TrackAndSector first_tslist_link;
  unsigned char file_type;
  char file_name[0x21-0x03];
  unsigned char sector_count;
  unsigned char end_mark;
} __attribute__((packed));
typedef struct DirectoryEntry DirectoryEntry;

struct DirectorySector {
  unsigned char unused1;
  TrackAndSector next_block_link;
  unsigned char unused2[0x0B-0x03];
  DirectoryEntry entries[7];
} __attribute__((packed));
typedef struct DirectorySector DirectorySector;

struct VtocSector {
  unsigned char unused1;            // $02
  TrackAndSector first_dir_link;    // $11 / $0C
  unsigned char dos_release_number; // $02
  unsigned char unused2[2];
  unsigned char diskette_volume_number;   // default $FE
  unsigned char unused3[0x27-0x07];
  unsigned char max_ts_pairs_per_sector;  // should be $7A
  unsigned char unused4[0x30-0x28];
  unsigned char track_bitmap_mask[4]; // $FF F8 00 00
  unsigned char tracks_per_diskette;  // $23
  unsigned char sectors_per_track;    // $0D
  unsigned char bytes_per_sector_lo;  // $00
  unsigned char bytes_per_sector_hi;  // $01
  unsigned char track_bitmap[35][4];
  unsigned char unused5[0x100-0xC4];
} __attribute__((packed));
typedef struct VtocSector VtocSector;

void ListProgram(const char* src_path, FILE* dst);

Stream dsk_stream;
unsigned char* dsk;

VtocSector* vtoc;
DirectorySector* directory_sectors[SECTORS_PER_TRACK];
int directory_sector_count = -1;

unsigned TrackSectorAddress(unsigned track, unsigned sector) {
  assert(track < TRACK_COUNT);
  assert(sector < 16);
  assert(!vtoc || sector < vtoc->sectors_per_track);
  return track * TRACK_SIZE + sector * SECTOR_SIZE;
}

char TranslateFileType(const DirectoryEntry* de, bool* is_locked) {
  unsigned char ft = de->file_type;
  if (is_locked)
    *is_locked = !!(ft & 0x80);
  switch (ft & 0x7F) {
    case 0: return 'T';
    case 1: return 'I';
    case 2: return 'A';
    case 4: return 'B';
    default: Die("Invalid filetype byte: %02X", ft);
  }
}

void ReadDsk(const char* path) {
  InitStream(&dsk_stream);
  ReadFileWithKnownLength(path, &dsk_stream, DSK_SIZE);
  dsk = dsk_stream.buf;
}

void ScanDirectory() {
  vtoc = (VtocSector*)(dsk + TrackSectorAddress(0x11, 0));
  int i = 0;
  unsigned track = vtoc->first_dir_link.track,
           sector = vtoc->first_dir_link.sector;
  while (track != 0 || sector != 0) {
    assert(i < vtoc->sectors_per_track);
    unsigned sector_addr = TrackSectorAddress(track, sector);
    directory_sectors[i] = (DirectorySector*)(dsk + sector_addr);
    track = directory_sectors[i]->next_block_link.track;
    sector = directory_sectors[i]->next_block_link.sector;
    i++;
  }
  directory_sector_count = i;
}

unsigned CopyFilename(const DirectoryEntry* de, char* filename) {
  unsigned i;
  for (i = 0; i < FILENAME_LEN; i++) {
    filename[i] = de->file_name[i] & 0x7F;
  }
  filename[i] = 0; // add null terminator for insurance
  int len = i;
  while (len > 0) {
    len--;
    if (filename[len] != ' ')
      break;
    filename[len] = 0;
  }
  return len;
}

void ExtractFileSectors(const DirectoryEntry* de, FILE* out) {
  unsigned tslist_track = de->first_tslist_link.track;
  unsigned tslist_sector = de->first_tslist_link.sector;
  int sector_number = 0;
  while (tslist_track != 0 || tslist_sector != 0) {
    unsigned tslist_block_sector_addr =
      TrackSectorAddress(tslist_track, tslist_sector);
    TrackSectorList* tsl = (TrackSectorList*)(dsk + tslist_block_sector_addr);
    for (int i=0; i < TSLIST_SECTOR_COUNT; i++) {
      TrackAndSector ts = tsl->sectors[i];
      if (ts.track != 0 || ts.sector != 0) {
        // sector is in use
        unsigned sector_addr = TrackSectorAddress(ts.track, ts.sector);
        size_t bytes_written = fwrite(dsk + sector_addr, 1, SECTOR_SIZE, out);
        if (SECTOR_SIZE != bytes_written)
          Die("Error writing sector #%d to file: only wrote %d bytes.",
              sector_number, bytes_written);
      }
    }
    tslist_track = tsl->next_tslist_link.track;
    tslist_sector = tsl->next_tslist_link.sector;
  }
}

void TranslateTextFile(const char* dst_path, const char* src_path) {
  FILE* src = fopen(src_path, "rb");
  if (!src)
    Die("Failed to open file for reading: %s", src_path);
  FILE* dst = fopen(dst_path, "wt");
  if (!dst)
    Die("Failed to open file for writing: %s", dst_path);
  int c;
  while ((c = getc(src)) != EOF) {
    int dst_char = c & 0x7F;
    if (dst_char == '\r')
      putc('\n', dst); // translate Apple's CR line ending
    else if (dst_char != 0) // skip nulls at end of file
      putc(dst_char, dst);
  }
  if (ferror(dst))
    Die("Error writing to file '%s': %s", dst_path, strerror(errno));
  if (ferror(src))
    Die("Error reading from file '%s': %s", src_path, strerror(errno));
  fclose(dst);
  fclose(src);
}

void ListIntegerBasicFile(const char* dst_path, const char* src_path) {
  FILE* dst = fopen(dst_path, "wt");
  if (!dst)
    Die("Unable to open file: %s", dst_path);
  ListProgram(src_path, dst);
  fclose(dst);
}

void ExtractFile(const DirectoryEntry* de, const char* dir) {
  static char filename[FILENAME_LEN+1];
  static char output_path[PATH_LEN];
  static char tr_output_path[PATH_LEN];
  CopyFilename(de, filename);
  ConcatPath(output_path, PATH_LEN, dir, filename, PATH_SEPARATOR);
  printf("    Extracting file: '%s' -> '%s'\n", filename, output_path);
  FILE* f = fopen(output_path, "wb");
  if (!f)
    Die("Unable to open output file: %s", output_path);
  ExtractFileSectors(de, f);
  fclose(f);
  char ft = TranslateFileType(de, NULL);
  switch (ft) {
    case 'T':
      ConcatPath(tr_output_path, PATH_LEN, output_path, "txt", '.');
      TranslateTextFile(tr_output_path, output_path);
      break;
    case 'I':
      ConcatPath(tr_output_path, PATH_LEN, output_path, "I.bas", '.');
      ListIntegerBasicFile(tr_output_path, output_path);
      break;
    case 'A':
      ConcatPath(tr_output_path, PATH_LEN, output_path, "A.bas", '.');
      // TODO: List Applesoft BASIC.
      break;
    case 'B':
      ConcatPath(tr_output_path, PATH_LEN, output_path, "bin.txt", '.');
      // TODO: Generate hex dump of binary file.
      break;
  }
}

void PrintDirectory() {
  for (int s=0; s < directory_sector_count; s++) {
    const DirectorySector* ds = directory_sectors[s];
    for (int e=0; e < ENTRIES_PER_SECTOR; e++) {
      const DirectoryEntry* de = &ds->entries[e];
      if (de->first_tslist_link.track != 0) {
        //printf("Directory entry: %lX\n", (unsigned long)((void*)de - (void*)dsk));
        bool is_locked;
        char ft = TranslateFileType(de, &is_locked);
        printf("%c%c ", is_locked ? '*' : ' ', ft);
        for (unsigned i=0; i < sizeof(de->file_name); i++) {
          printf("%c", de->file_name[i] & 0x7F);
        }
        printf("\n");
      }
    }
  }
}

void ExtractFiles(const char* extract_dir) {
  for (int s=0; s < directory_sector_count; s++) {
    const DirectorySector* ds = directory_sectors[s];
    for (int e=0; e < ENTRIES_PER_SECTOR; e++) {
      const DirectoryEntry* de = &ds->entries[e];
      if (de->first_tslist_link.track != 0) {
        ExtractFile(de, extract_dir);
      }
    }
  }
}

int main(int argc, char** argv) {
  assert(sizeof(DirectorySector) == SECTOR_SIZE);
  assert(sizeof(VtocSector) == SECTOR_SIZE);
  assert(sizeof(TrackSectorList) == SECTOR_SIZE);
  if (argc != 2) {
    Die("Invalid arguments.");
  }
  ReadDsk(argv[1]);
  ScanDirectory();
  printf("Found %d sectors in the directory.\n", directory_sector_count);
  printf("Diskette volume number: %02X\n", vtoc->diskette_volume_number);
  /*
  printf("Track/sector count: %d/%d (%d sectors / %dK total)\n",
      vtoc->tracks_per_diskette, vtoc->sectors_per_track,
      vtoc->tracks_per_diskette * vtoc->sectors_per_track,
      vtoc->tracks_per_diskette * vtoc->sectors_per_track * SECTOR_SIZE / 1024
      );
  */
  PrintDirectory();
  ExtractFiles("scratch");
  return 0;
}

