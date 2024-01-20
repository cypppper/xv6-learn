#include "kernel/types.h"
#include "kernel/param.h"
#include "user/user.h"

char *argvv[32];

char input[320];

int main(int argc, char *argv[]) {

  if(argc < 2){
    fprintf(2, "usage: xargs exec [args..]\n");
    exit(1);
  }
  // printf("argc:%d\n", argc);
  // for (int i = 1; i< argc;i++) {
  //   printf("argv: %s\n", argv[i]);
  // }
  int argidx = 0;
  for (;argidx < argc - 1; argidx++) {
    argvv[argidx] = argv[argidx + 1];
  }

  char * p = input;
  char * q = p;

  char ch;
  // printf("start!!!\n");
  argvv[argidx] = malloc(512);
  while (read(0, &ch, 1)) {
    // printf("read result : %c\n", ch);
    if (ch != '\n') {
      *p++ = ch;
    } else {
      *p++ = '\0';
      argvv[argidx++] = q;
      q = p;
    }
  }

  // for (int i = 0; i < argidx; i++) {
  //   printf("recv str:%s\n", argvv[i]);
  // } 

  if (fork() == 0) {
    exec(argv[1], argvv);
 
  } else {
    wait(0);
  
    exit(0);
  }

  exit(0);
}