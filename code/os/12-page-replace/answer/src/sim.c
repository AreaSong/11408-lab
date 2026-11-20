#include "sim_priv.h"

#include <string.h>

int sim_exec(const char *algo, int frames, const Access *warm, int nwarm, const Access *refs,
             int nrefs, Report *out) {
    int id = algo_id(algo);
    int rc;

    if (id < 0) {
        return -1;
    }
    if (frames < 1 || frames > FRAME_MAX || nrefs < 1 || nrefs > REF_MAX || nwarm < 0 ||
        nwarm > frames || (nwarm > 0 && warm == NULL) || refs == NULL) {
        return -2;
    }
    memset(out, 0, sizeof *out);
    memcpy(out->algo, algo, strlen(algo) + 1);
    out->frames = frames;
    out->n = nrefs;
    if (id <= 1) {
        rc = queue_sim(id == 1, frames, warm, nwarm, refs, nrefs, out);
    } else if (id == 2) {
        rc = opt_sim(frames, warm, nwarm, refs, nrefs, out);
    } else {
        rc = clock_sim(id == 4, frames, warm, nwarm, refs, nrefs, out);
    }
    return rc;
}
