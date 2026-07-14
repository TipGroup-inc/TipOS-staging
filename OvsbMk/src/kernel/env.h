#ifndef ENV_H
#define ENV_H

void env_init(void);
const char *env_get(const char *name);
void env_set(const char *name, const char *val);
const char *alias_get(const char *name);
void alias_set(const char *name, const char *val);
int alias_unset(const char *name);
int env_count_get(void);
const char *env_name_get(int i);
const char *env_val_get(int i);
int alias_count_get(void);
const char *alias_name_get(int i);
const char *alias_val_get(int i);

#endif
