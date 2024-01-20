#include "kernel/types.h"
#include "user/user.h"

int main(int argc, char *argv[]) {
  int idx, sieve_num, flag, checker;
  int rdfd,wrfd,readint;
  int p0[2];
  int p1[2];

  pipe(p0);
  rdfd = p1[0];
  wrfd = p0[1];

  idx = 0;
  for (int i = 3; i < 35; i++) {
    write(wrfd, &i, 4);
  }
  close(wrfd);

  sieve_num = 2;
  idx = 1;
  flag = 1;

  while (flag == 1) {
    if (fork() != 0) {
      if (idx % 2 == 1) {
        pipe(p1);
        rdfd = p0[0];
        wrfd = p1[1];
      } else {
        pipe(p0);
        rdfd = p1[0];
        wrfd = p0[1];
      }
      printf("prime %d\n", sieve_num);
      checker = sieve_num;
      flag = 0;
      while (read(rdfd, &readint, 4)) {
        // printf("get %d\n", readint);
        if (readint % checker != 0) {
          if (flag == 0) {
            flag = 1;
            sieve_num = readint;
          } else {
            // printf("send %d\n", readint);
            write(wrfd, &readint, 4);
          }
        } else {
          // printf("delete %d\n", readint);
        }
      }
      close(wrfd);
      // printf("read end!\n");
      idx = idx + 1;
      close(rdfd);
      if (flag == 0) {
        exit(0);
      }

    } else {
      wait(0);
      exit(0);
    }
  }
  exit(0);
}

