#ifndef SHELL_CMDS_H
#define SHELL_CMDS_H

void cmd_help(void);
void cmd_clear(void);
void cmd_echo(const char *args);
void cmd_about(void);
void cmd_shutdown(void);
void cmd_ls(void);
void cmd_touch(const char *name);
void cmd_rm(const char *name);
void cmd_cat(const char *name);
void cmd_edit(const char *name);
void cmd_mkdir(const char *name);
void cmd_cd(const char *name);
void cmd_pwd(void);
void cmd_exec(const char *name);

#endif
