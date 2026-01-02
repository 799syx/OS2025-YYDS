// Shell.

#include "types.h"
#include "user/user.h"
#include "fcntl.h"
#include "fs.h"

// Parsed command representation
#define EXEC 1
#define REDIR 2
#define PIPE 3
#define LIST 4
#define BACK 5

#define MAXARGS 10

// ============ 命令历史 ============
#define HISTORY_SIZE 20
#define MAX_CMD_LEN 100

static char history[HISTORY_SIZE][MAX_CMD_LEN];
static int history_count = 0;
static int history_pos = 0;

void add_history(char *cmd) {
  if (cmd[0] == 0 || cmd[0] == '\n') return;
  // 移除换行符
  int len = strlen(cmd);
  if (len > 0 && cmd[len-1] == '\n') len--;
  if (len == 0) return;
  
  // 复制到历史记录
  int idx = history_count % HISTORY_SIZE;
  for (int i = 0; i < len && i < MAX_CMD_LEN-1; i++)
    history[idx][i] = cmd[i];
  history[idx][len < MAX_CMD_LEN-1 ? len : MAX_CMD_LEN-1] = 0;
  history_count++;
  history_pos = history_count;
}

// ============ 后台任务管理 ============
#define MAX_JOBS 8
static struct {
  int pid;
  int running;
  char cmd[32];
} jobs[MAX_JOBS];
static int job_count = 0;

void add_job(int pid, char *cmd) {
  for (int i = 0; i < MAX_JOBS; i++) {
    if (!jobs[i].running) {
      jobs[i].pid = pid;
      jobs[i].running = 1;
      int j;
      for (j = 0; j < 31 && cmd[j] && cmd[j] != '\n'; j++)
        jobs[i].cmd[j] = cmd[j];
      jobs[i].cmd[j] = 0;
      job_count++;
      printf("[%d] %d\n", i+1, pid);
      return;
    }
  }
}

void check_jobs(void) {
  // 简化实现：尝试非阻塞回收僵尸进程
  // 由于没有 WNOHANG，暂时跳过自动检查
  // 用户可以用 jobs 命令查看
}

void list_jobs(void) {
  int found = 0;
  for (int i = 0; i < MAX_JOBS; i++) {
    if (jobs[i].running) {
      printf("[%d] Running: %s (pid=%d)\n", i+1, jobs[i].cmd, jobs[i].pid);
      found = 1;
    }
  }
  if (!found)
    printf("No background jobs\n");
}

struct cmd
{
  int type;
};

struct execcmd
{
  int type;
  char *argv[MAXARGS];
  char *eargv[MAXARGS];
};

struct redircmd
{
  int type;
  struct cmd *cmd;
  char *file;
  char *efile;
  int mode;
  int fd;
};

struct pipecmd
{
  int type;
  struct cmd *left;
  struct cmd *right;
};

struct listcmd
{
  int type;
  struct cmd *left;
  struct cmd *right;
};

struct backcmd
{
  int type;
  struct cmd *cmd;
};

int fork1(void); // Fork but panics on failure.
void panic(char *);
struct cmd *parsecmd(char *);

// Execute cmd.  Never returns.
void runcmd(struct cmd *cmd)
{
  int p[2];
  struct backcmd *bcmd;
  struct execcmd *ecmd;
  struct listcmd *lcmd;
  struct pipecmd *pcmd;
  struct redircmd *rcmd;

  if (cmd == 0)
    exit(1);

  switch (cmd->type)
  {
  default:
    panic("runcmd");

  case EXEC:
    ecmd = (struct execcmd *)cmd;
    if (ecmd->argv[0] == 0)
      exit(1);
    exec(ecmd->argv[0], ecmd->argv);
    fprintf(2, "exec %s failed\n", ecmd->argv[0]);
    break;

  case REDIR:
    rcmd = (struct redircmd *)cmd;
    close(rcmd->fd);
    if (open(rcmd->file, rcmd->mode) < 0)
    {
      fprintf(2, "open %s failed\n", rcmd->file);
      exit(1);
    }
    runcmd(rcmd->cmd);
    break;

  case LIST:
    lcmd = (struct listcmd *)cmd;
    if (fork1() == 0)
      runcmd(lcmd->left);
    wait(0);
    runcmd(lcmd->right);
    break;

  case PIPE:
    pcmd = (struct pipecmd *)cmd;
    if (pipe(p) < 0)
      panic("pipe");
    if (fork1() == 0)
    {
      close(1);
      dup(p[1]);
      close(p[0]);
      close(p[1]);
      runcmd(pcmd->left);
    }
    if (fork1() == 0)
    {
      close(0);
      dup(p[0]);
      close(p[0]);
      close(p[1]);
      runcmd(pcmd->right);
    }
    close(p[0]);
    close(p[1]);
    wait(0);
    wait(0);
    break;

  case BACK:
    bcmd = (struct backcmd *)cmd;
    if (fork1() == 0)
      runcmd(bcmd->cmd);
    break;
  }
  exit(0);
}

// Tab 补全: 查找匹配的文件名
void tab_complete(char *buf, int *pos) {
  // 找到当前正在输入的词的起始位置
  int start = *pos;
  while (start > 0 && buf[start-1] != ' ') start--;
  
  char prefix[32];
  int plen = *pos - start;
  if (plen == 0 || plen >= 32) return;
  
  for (int i = 0; i < plen; i++)
    prefix[i] = buf[start + i];
  prefix[plen] = 0;
  
  // 打开当前目录查找匹配
  int fd = open(".", O_RDONLY);
  if (fd < 0) return;
  
  struct dirent de;
  char match[DIRSIZ+1];
  match[0] = 0;
  int match_count = 0;
  
  while (read(fd, &de, sizeof(de)) == sizeof(de)) {
    if (de.inum == 0) continue;
    
    // 检查前缀匹配
    int m = 1;
    for (int i = 0; i < plen; i++) {
      if (de.name[i] != prefix[i]) { m = 0; break; }
    }
    if (m) {
      if (match_count == 0) {
        // 保存第一个匹配
        int i;
        for (i = 0; i < DIRSIZ && de.name[i]; i++)
          match[i] = de.name[i];
        match[i] = 0;
      }
      match_count++;
    }
  }
  close(fd);
  
  if (match_count == 1) {
    // 唯一匹配，自动补全
    int mlen = strlen(match);
    for (int i = plen; i < mlen && *pos < MAX_CMD_LEN-2; i++) {
      buf[*pos] = match[i];
      printf("%c", match[i]);
      (*pos)++;
    }
    buf[*pos] = 0;
  } else if (match_count > 1) {
    // 多个匹配，显示提示
    printf("\n");
    // 重新读取并显示所有匹配
    fd = open(".", O_RDONLY);
    if (fd >= 0) {
      while (read(fd, &de, sizeof(de)) == sizeof(de)) {
        if (de.inum == 0) continue;
        int m = 1;
        for (int i = 0; i < plen; i++) {
          if (de.name[i] != prefix[i]) { m = 0; break; }
        }
        if (m) printf("%s  ", de.name);
      }
      close(fd);
    }
    printf("\n$ %s", buf);
  }
}

int getcmd(char *buf, int nbuf)
{
  fprintf(2, "$ ");
  memset(buf, 0, nbuf);
  
  // 切换到原始模式
  consctl(1);
  
  int pos = 0;
  history_pos = history_count;
  
  while (1) {
    char c;
    if (read(0, &c, 1) != 1) {
      consctl(0);
      return -1;
    }
    
    if (c == '\n' || c == '\r') {
      buf[pos] = '\n';
      buf[pos+1] = 0;
      printf("\n");
      consctl(0);  // 恢复行缓冲模式
      return 0;
    }
    else if (c == 0x7f || c == '\b') {  // Backspace
      if (pos > 0) {
        pos--;
        buf[pos] = 0;
        printf("\b \b");
      }
    }
    else if (c == '\t') {  // Tab - 自动补全
      tab_complete(buf, &pos);
    }
    else if (c == 0x1b) {  // Escape sequence (arrow keys)
      char seq[2];
      if (read(0, &seq[0], 1) != 1) continue;
      if (read(0, &seq[1], 1) != 1) continue;
      
      if (seq[0] == '[') {
        if (seq[1] == 'A') {  // Up arrow
          if (history_pos > 0 && history_pos > history_count - HISTORY_SIZE) {
            history_pos--;
            // 清除当前行
            while (pos > 0) { printf("\b \b"); pos--; }
            // 复制历史命令
            int idx = history_pos % HISTORY_SIZE;
            strcpy(buf, history[idx]);
            pos = strlen(buf);
            printf("%s", buf);
          }
        }
        else if (seq[1] == 'B') {  // Down arrow
          if (history_pos < history_count - 1) {
            history_pos++;
            while (pos > 0) { printf("\b \b"); pos--; }
            int idx = history_pos % HISTORY_SIZE;
            strcpy(buf, history[idx]);
            pos = strlen(buf);
            printf("%s", buf);
          }
          else if (history_pos < history_count) {
            history_pos = history_count;
            while (pos > 0) { printf("\b \b"); pos--; }
            buf[0] = 0;
            pos = 0;
          }
        }
      }
    }
    else if (c >= 32 && c < 127) {  // 可打印字符
      if (pos < nbuf - 2) {
        buf[pos++] = c;
        buf[pos] = 0;
        printf("%c", c);
      }
    }
  }
}

// 简单字符串比较（忽略末尾换行）
int cmd_eq(char *buf, char *cmd) {
  while (*cmd) {
    if (*buf != *cmd) return 0;
    buf++; cmd++;
  }
  return (*buf == '\n' || *buf == 0 || *buf == ' ');
}

// 检查是否是内置命令
int builtin_cmd(char *buf) {
  // 跳过开头空格
  while (*buf == ' ') buf++;
  
  // cd 命令
  if (buf[0] == 'c' && buf[1] == 'd' && buf[2] == ' ') {
    buf += 3;
    while (*buf == ' ') buf++;  // 跳过空格
    char *end = buf;
    while (*end && *end != '\n') end++;
    *end = 0;
    if (chdir(buf) < 0)
      fprintf(2, "cannot cd %s\n", buf);
    return 1;
  }
  
  // jobs 命令
  if (cmd_eq(buf, "jobs")) {
    list_jobs();
    return 1;
  }
  
  // history 命令
  if (cmd_eq(buf, "history")) {
    int start = history_count > HISTORY_SIZE ? history_count - HISTORY_SIZE : 0;
    for (int i = start; i < history_count; i++) {
      printf("%d: %s\n", i + 1, history[i % HISTORY_SIZE]);
    }
    return 1;
  }
  
  // exit 命令
  if (cmd_eq(buf, "exit")) {
    exit(0);
  }
  
  return 0;
}

// 检查命令是否以 & 结尾（后台运行）
int is_background(char *buf) {
  int len = strlen(buf);
  // 跳过末尾的换行符
  while (len > 0 && (buf[len-1] == '\n' || buf[len-1] == ' '))
    len--;
  return len > 0 && buf[len-1] == '&';
}

int main(void)
{
  static char buf[100];
  int fd;

  // Ensure that three file descriptors are open.
  while ((fd = open("console", O_RDWR)) >= 0)
  {
    if (fd >= 3)
    {
      close(fd);
      break;
    }
  }

  // 初始化后台任务数组
  for (int i = 0; i < MAX_JOBS; i++)
    jobs[i].running = 0;

  // Read and run input commands.
  while (getcmd(buf, sizeof(buf)) >= 0)
  {
    // 检查已完成的后台任务
    check_jobs();
    
    // 空命令
    if (buf[0] == '\n' || buf[0] == 0)
      continue;
    
    // 添加到历史记录
    add_history(buf);
    
    // 内置命令
    if (builtin_cmd(buf))
      continue;
    
    // 检查是否后台运行
    int bg = is_background(buf);
    
    int pid = fork1();
    if (pid == 0)
      runcmd(parsecmd(buf));
    
    if (bg) {
      // 后台任务：不等待，记录到 jobs
      add_job(pid, buf);
    } else {
      // 前台任务：等待完成
      wait(0);
    }
  }
  exit(0);
}

void panic(char *s)
{
  fprintf(2, "%s\n", s);
  exit(1);
}

int fork1(void)
{
  int pid;

  pid = fork();
  if (pid == -1)
    panic("fork");
  return pid;
}

// PAGEBREAK!
//  Constructors

struct cmd *
execcmd(void)
{
  struct execcmd *cmd;

  cmd = malloc(sizeof(*cmd));
  memset(cmd, 0, sizeof(*cmd));
  cmd->type = EXEC;
  return (struct cmd *)cmd;
}

struct cmd *
redircmd(struct cmd *subcmd, char *file, char *efile, int mode, int fd)
{
  struct redircmd *cmd;

  cmd = malloc(sizeof(*cmd));
  memset(cmd, 0, sizeof(*cmd));
  cmd->type = REDIR;
  cmd->cmd = subcmd;
  cmd->file = file;
  cmd->efile = efile;
  cmd->mode = mode;
  cmd->fd = fd;
  return (struct cmd *)cmd;
}

struct cmd *
pipecmd(struct cmd *left, struct cmd *right)
{
  struct pipecmd *cmd;

  cmd = malloc(sizeof(*cmd));
  memset(cmd, 0, sizeof(*cmd));
  cmd->type = PIPE;
  cmd->left = left;
  cmd->right = right;
  return (struct cmd *)cmd;
}

struct cmd *
listcmd(struct cmd *left, struct cmd *right)
{
  struct listcmd *cmd;

  cmd = malloc(sizeof(*cmd));
  memset(cmd, 0, sizeof(*cmd));
  cmd->type = LIST;
  cmd->left = left;
  cmd->right = right;
  return (struct cmd *)cmd;
}

struct cmd *
backcmd(struct cmd *subcmd)
{
  struct backcmd *cmd;

  cmd = malloc(sizeof(*cmd));
  memset(cmd, 0, sizeof(*cmd));
  cmd->type = BACK;
  cmd->cmd = subcmd;
  return (struct cmd *)cmd;
}
// PAGEBREAK!
//  Parsing

char whitespace[] = " \t\r\n\v";
char symbols[] = "<|>&;()";

int gettoken(char **ps, char *es, char **q, char **eq)
{
  char *s;
  int ret;

  s = *ps;
  while (s < es && strchr(whitespace, *s))
    s++;
  if (q)
    *q = s;
  ret = *s;
  switch (*s)
  {
  case 0:
    break;
  case '|':
  case '(':
  case ')':
  case ';':
  case '&':
  case '<':
    s++;
    break;
  case '>':
    s++;
    if (*s == '>')
    {
      ret = '+';
      s++;
    }
    break;
  default:
    ret = 'a';
    while (s < es && !strchr(whitespace, *s) && !strchr(symbols, *s))
      s++;
    break;
  }
  if (eq)
    *eq = s;

  while (s < es && strchr(whitespace, *s))
    s++;
  *ps = s;
  return ret;
}

int peek(char **ps, char *es, char *toks)
{
  char *s;

  s = *ps;
  while (s < es && strchr(whitespace, *s))
    s++;
  *ps = s;
  return *s && strchr(toks, *s);
}

struct cmd *parseline(char **, char *);
struct cmd *parsepipe(char **, char *);
struct cmd *parseexec(char **, char *);
struct cmd *nulterminate(struct cmd *);

struct cmd *
parsecmd(char *s)
{
  char *es;
  struct cmd *cmd;

  es = s + strlen(s);
  cmd = parseline(&s, es);
  peek(&s, es, "");
  if (s != es)
  {
    fprintf(2, "leftovers: %s\n", s);
    panic("syntax");
  }
  nulterminate(cmd);
  return cmd;
}

struct cmd *
parseline(char **ps, char *es)
{
  struct cmd *cmd;

  cmd = parsepipe(ps, es);
  while (peek(ps, es, "&"))
  {
    gettoken(ps, es, 0, 0);
    cmd = backcmd(cmd);
  }
  if (peek(ps, es, ";"))
  {
    gettoken(ps, es, 0, 0);
    cmd = listcmd(cmd, parseline(ps, es));
  }
  return cmd;
}

struct cmd *
parsepipe(char **ps, char *es)
{
  struct cmd *cmd;

  cmd = parseexec(ps, es);
  if (peek(ps, es, "|"))
  {
    gettoken(ps, es, 0, 0);
    cmd = pipecmd(cmd, parsepipe(ps, es));
  }
  return cmd;
}

struct cmd *
parseredirs(struct cmd *cmd, char **ps, char *es)
{
  int tok;
  char *q, *eq;

  while (peek(ps, es, "<>"))
  {
    tok = gettoken(ps, es, 0, 0);
    if (gettoken(ps, es, &q, &eq) != 'a')
      panic("missing file for redirection");
    switch (tok)
    {
    case '<':
      cmd = redircmd(cmd, q, eq, O_RDONLY, 0);
      break;
    case '>':
      cmd = redircmd(cmd, q, eq, O_WRONLY | O_CREATE | O_TRUNC, 1);
      break;
    case '+': // >>
      cmd = redircmd(cmd, q, eq, O_WRONLY | O_CREATE, 1);
      break;
    }
  }
  return cmd;
}

struct cmd *
parseblock(char **ps, char *es)
{
  struct cmd *cmd;

  if (!peek(ps, es, "("))
    panic("parseblock");
  gettoken(ps, es, 0, 0);
  cmd = parseline(ps, es);
  if (!peek(ps, es, ")"))
    panic("syntax - missing )");
  gettoken(ps, es, 0, 0);
  cmd = parseredirs(cmd, ps, es);
  return cmd;
}

struct cmd *
parseexec(char **ps, char *es)
{
  char *q, *eq;
  int tok, argc;
  struct execcmd *cmd;
  struct cmd *ret;

  if (peek(ps, es, "("))
    return parseblock(ps, es);

  ret = execcmd();
  cmd = (struct execcmd *)ret;

  argc = 0;
  ret = parseredirs(ret, ps, es);
  while (!peek(ps, es, "|)&;"))
  {
    if ((tok = gettoken(ps, es, &q, &eq)) == 0)
      break;
    if (tok != 'a')
      panic("syntax");
    cmd->argv[argc] = q;
    cmd->eargv[argc] = eq;
    argc++;
    if (argc >= MAXARGS)
      panic("too many args");
    ret = parseredirs(ret, ps, es);
  }
  cmd->argv[argc] = 0;
  cmd->eargv[argc] = 0;
  return ret;
}

// NUL-terminate all the counted strings.
struct cmd *
nulterminate(struct cmd *cmd)
{
  int i;
  struct backcmd *bcmd;
  struct execcmd *ecmd;
  struct listcmd *lcmd;
  struct pipecmd *pcmd;
  struct redircmd *rcmd;

  if (cmd == 0)
    return 0;

  switch (cmd->type)
  {
  case EXEC:
    ecmd = (struct execcmd *)cmd;
    for (i = 0; ecmd->argv[i]; i++)
      *ecmd->eargv[i] = 0;
    break;

  case REDIR:
    rcmd = (struct redircmd *)cmd;
    nulterminate(rcmd->cmd);
    *rcmd->efile = 0;
    break;

  case PIPE:
    pcmd = (struct pipecmd *)cmd;
    nulterminate(pcmd->left);
    nulterminate(pcmd->right);
    break;

  case LIST:
    lcmd = (struct listcmd *)cmd;
    nulterminate(lcmd->left);
    nulterminate(lcmd->right);
    break;

  case BACK:
    bcmd = (struct backcmd *)cmd;
    nulterminate(bcmd->cmd);
    break;
  }
  return cmd;
}
