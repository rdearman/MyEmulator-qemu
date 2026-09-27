#ifndef NOTE_STORE_H
#define NOTE_STORE_H

int note_append(const char *path, const char *tag, const char *text);
int note_list(const char *path, const char *needle);

#endif
