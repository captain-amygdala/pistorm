#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include "ide.h"

int main(int argc, const char *argv[])
{
  int t, fd;
  if (argc != 3) {
    fprintf(stderr, "%s [type] [path]\n", argv[0]);
    exit(1);
  }
  t = atoi(argv[1]);
  if (t < 1 || t > MAX_DRIVE_TYPE) {
    fprintf(stderr, "%s: unknown drive type.\n", argv[0]);
    exit(1);
  }
  fd = open(argv[2], O_WRONLY|O_TRUNC|O_CREAT|O_EXCL, 0666);
  if (fd == -1) {
    perror(argv[2]);
    exit(1);
  }
  if (ide_make_drive(t, fd) < 0) {
    perror(argv[2]);
    exit(1);
  }
  return 0;
}
