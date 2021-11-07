typedef unsigned char byte_t;
typedef struct {
  unsigned cap;
  unsigned len;
  byte_t* data;
} buf_t;
buf_t *readFile(const char *path);
