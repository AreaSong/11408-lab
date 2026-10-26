#ifndef RUN_H
#define RUN_H

#include "group.h"
#include "image.h"
#include "line.h"
#include "mount.h"

int cmd_calc(Group *group, char tok[][TOK_W], int ntok);
int cmd_live(Image *image, MountSet *mount, char tok[][TOK_W], int ntok);

#endif
