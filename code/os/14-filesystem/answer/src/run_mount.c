#include "run.h"

#include <stdio.h>
#include <string.h>

static int soft(const char *name) {
    printf("result=%s\n", name);
    return 0;
}

static int mnt_rc(int rc) {
    if (rc == MNT_OK) {
        return 0;
    }
    if (rc == MNT_NO) {
        return soft("no-file");
    }
    if (rc == MNT_EXIST) {
        return soft("exist");
    }
    if (rc == MNT_FULL) {
        return soft("full");
    }
    if (rc == MNT_NOTDIR) {
        return soft("not-dir");
    }
    if (rc == MNT_ROOT) {
        return soft("root");
    }
    return soft("no-file");
}

static int ready(const MountSet *m) {
    if (!m->ready) {
        return fail("no-mount");
    }
    return 0;
}

static int mount_add(MountSet *m, char tok[][TOK_W], int ntok) {
    int fs;
    int file = strcmp(tok[1], "file") == 0;
    int want = file ? 6 : 5;
    int rc;

    if (need(ntok, want) || take_i(tok[2], 0, 1, &fs)) {
        return 1;
    }
    if (token_path(tok[3]) != 0 || tok[3][0] != '/' || token_name(tok[4], 1, 11) != 0) {
        return fail("bad-command");
    }
    if (file && token_name(tok[5], 1, 40) != 0) {
        return fail("bad-command");
    }
    rc = file ? mount_file(m, fs, tok[3], tok[4], tok[5]) : mount_mkdir(m, fs, tok[3], tok[4]);
    if (rc != MNT_OK) {
        return mnt_rc(rc);
    }
    printf("mount %s fs=%d parent=%s name=%s\n", file ? "file" : "mkdir", fs, tok[3], tok[4]);
    return 0;
}

static int mount_cat_cmd(MountSet *m, char tok[][TOK_W], int ntok) {
    MountStep steps[MNT_STEP];
    int nsteps = 0;
    int fs = 0;
    int node = 0;
    int i;
    int rc;

    if (need(ntok, 3)) {
        return 1;
    }
    if (token_path(tok[2]) != 0 || tok[2][0] != '/') {
        return fail("bad-command");
    }
    rc = mount_cat(m, tok[2], steps, MNT_STEP, &nsteps, &fs, &node);
    if (rc != MNT_OK) {
        return mnt_rc(rc);
    }
    for (i = 0; i < nsteps && i < MNT_STEP; i++) {
        printf("step fs=%d name=%s cross=%d\n", steps[i].fs, steps[i].name, steps[i].cross);
    }
    if (m->node[fs][node].is_dir) {
        printf("cat path=%s fs=%d dir=1\n", tok[2], fs);
    } else {
        printf("cat path=%s fs=%d data=%s\n", tok[2], fs, m->node[fs][node].data);
    }
    return 0;
}

static int mount_at(MountSet *m, char tok[][TOK_W], int ntok) {
    int rc;

    if (need(ntok, 3)) {
        return 1;
    }
    if (token_path(tok[2]) != 0 || tok[2][0] != '/') {
        return fail("bad-command");
    }
    rc = mount_attach(m, tok[2]);
    if (rc != MNT_OK) {
        return mnt_rc(rc);
    }
    printf("mount at path=%s fs=1\n", tok[2]);
    return 0;
}

static int mount_show(MountSet *m, char tok[][TOK_W], int ntok) {
    char path[64];

    if (need(ntok, 2)) {
        return 1;
    }
    (void)tok;
    if (mount_point(m, path, (int)sizeof path) != 0) {
        printf("mount point=none\n");
    } else {
        printf("mount point=%s fs=1\n", path);
    }
    return 0;
}

int cmd_mount(MountSet *m, char tok[][TOK_W], int ntok) {
    if (ntok < 2) {
        return fail("bad-command");
    }
    if (strcmp(tok[1], "reset") == 0) {
        if (need(ntok, 2)) {
            return 1;
        }
        mount_reset(m);
        printf("mount reset fs=2\n");
        return 0;
    }
    if (ready(m)) {
        return 1;
    }
    if (strcmp(tok[1], "mkdir") == 0 || strcmp(tok[1], "file") == 0) {
        return mount_add(m, tok, ntok);
    }
    if (strcmp(tok[1], "at") == 0) {
        return mount_at(m, tok, ntok);
    }
    if (strcmp(tok[1], "show") == 0) {
        return mount_show(m, tok, ntok);
    }
    if (strcmp(tok[1], "cat") == 0) {
        return mount_cat_cmd(m, tok, ntok);
    }
    return fail("bad-command");
}
