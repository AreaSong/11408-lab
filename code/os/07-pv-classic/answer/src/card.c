#include "card.h"

#include <stdio.h>

void scene_card(void) {
    puts("tool=sem saved=yes blocks-when=negative needs-mutex=no");
    puts("tool=cond saved=no blocks-when=wait needs-mutex=yes");
    puts("tool=monitor saved=inside blocks-when=condition needs-mutex=hidden");
}
