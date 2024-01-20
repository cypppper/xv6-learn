#include "kernel/types.h"
#include "user/user.h"

int main(int argc, char *argv[]) {
  int p1[2];
  int p2[2];
  pipe(p1);
  pipe(p2);
  if(fork() == 0) {
    char h2;
    close(p1[1]);
    read(p1[0], &h2, 1);
    close(p1[0]);
    int pid2 = getpid();
    printf("%d: received ping\n", pid2);
    close(p2[0]);
    write(p2[1], "p", 1);
    close(p2[1]);
    exit(0);
  } else {
    close(p1[0]);
    write(p1[1], "h", 1);
    close(p1[1]);
    char h;
    close(p2[1]);
    read(p2[0], &h, 1);
    close(p2[0]);
    int pid1 = getpid();
    printf("%d: received pong\n", pid1);
    exit(0);
  }
}

