// progress: levels passed and medals won, kept in progress.txt next to the records.
//   done   <pack>            the level was lived through to a finale at least once
//   medal  <pack>  <medal>   that medal was won
#ifndef PROGRESS_H
#define PROGRESS_H

void progress_load(void);
void progress_save(void);
int  progress_done(const char *pack);
void progress_set_done(const char *pack);
int  progress_medal(const char *pack, const char *medal);
void progress_add_medal(const char *pack, const char *medal);

#endif
