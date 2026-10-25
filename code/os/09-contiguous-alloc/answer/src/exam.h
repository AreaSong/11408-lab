#ifndef EXAM_H
#define EXAM_H

/* note 可为 NULL。没有附注时 *note 写成 NULL。未知题号返回 -1。 */
int exam_letter(const char *id, char *letter, const char **note);
int exam_print(const char *id);
void card_print(void);

#endif
