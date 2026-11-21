#include "dev/dev_layer.h"

#include <stdio.h>

int dev_layer_read(const char *logic, const char *phys, char lines[][80], int cap) {
    if (logic == NULL || phys == NULL || logic[0] == '\0' || phys[0] == '\0' || lines == NULL || cap < 4) {
        return -1;
    }
    snprintf(lines[0], 80, "layer n=1 name=user duty=read logic=%s", logic);
    snprintf(lines[1], 80, "layer n=2 name=independent duty=map phys=%s buffer=system", phys);
    snprintf(lines[2], 80, "layer n=3 name=driver duty=device-command chs=driver");
    snprintf(lines[3], 80, "layer n=4 name=interrupt duty=wake eoi=cpu-to-controller");
    return 4;
}
