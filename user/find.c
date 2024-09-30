#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"

//这个函数用来补齐空格？
char *fmtname(char *path) {
  static char buf[DIRSIZ + 1];
  char *p;

  // Find first character after last slash.
  for (p = path + strlen(path); p >= path && *p != '/'; p--)
    ;
  p++;

  // Return blank-padded name.
  if (strlen(p) >= DIRSIZ) return p;
  memmove(buf, p, strlen(p));
  // memset(buf + strlen(p), ' ', DIRSIZ - strlen(p));
  return buf;
}


void find(char *path,char *filename) {
  char buf[512], *p;
  int fd;
  struct dirent de;
  struct stat st;

  if ((fd = open(path, 0)) < 0) {
    fprintf(2, "find: cannot open %s\n", path);
    return;
  }

  if (fstat(fd, &st) < 0) {
    fprintf(2, "find: cannot stat %s\n", path);
    close(fd);
    return;
  }

  switch (st.type) {
    case T_FILE:
        printf("this is file %s\n",fmtname(path));
    //   printf("%s %d %d %l\n", fmtname(path), st.type, st.ino, st.size);
      break;

    case T_DIR:
      if (strlen(path) + 1 + DIRSIZ + 1 > sizeof buf) {
        printf("find: path too long\n");
        break;
      }
      strcpy(buf, path);
      p = buf + strlen(buf);
      *p++ = '/';
      while (read(fd, &de, sizeof(de)) == sizeof(de)) {
        if (de.inum == 0) continue; //表示该条目无效
        memmove(p, de.name, DIRSIZ); 
        p[DIRSIZ] = 0;
        if (stat(buf, &st) < 0) {
          printf("find: cannot stat %s\n", buf);
          continue;
        }
        if(strcmp())
        printf("this is dir %s %d %d %d\n", fmtname(buf), st.type, st.ino, st.size);
      }
      
      break;
  }
  close(fd);
}

int main(int argc, char *argv[]) {
  int i;

  if (argc < 2) {
    find(".");
    exit(0);
  }
  printf("argc is%d\n",argc);
  for (i = 1; i < argc; i++){
    find(argv[i]);
    printf("%s",argv[i]);
  } 
  exit(0);
}
