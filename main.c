#include "gameoflife.h"
#include <stdlib.h>
#include <string.h>
#include <errno.h>

/*
 * The below argument parsing functions should be passed the argument array
 * and corresponding length beginning at index 1 instead of 0 because
 * we have no use for the program name.
 */


// resolution
struct IntTuple {
    int w, h;
};

static struct IntTuple parseResolution(int argc, char **argv) {
    struct IntTuple res = {GOLdefaultWindowWidth, GOLdefaultWindowHeight};
    enum States {SEARCHING, WIDTH, HEIGHT};
    enum States state = SEARCHING;

    for (int i = 0; i < argc; ++i) {
        switch (state) {
        case SEARCHING: {
            if (!strcmp(argv[i], "-w"))
                state = WIDTH;
            else if (!strcmp(argv[i], "-h"))
                state = HEIGHT;
            break;
        }
        case WIDTH: {
            errno = 0;
            int w = (int) strtol(argv[i], NULL, 10);
            if (w > 0 && errno == 0)
                res.w = w;
            state = SEARCHING;
            break;
        }
        case HEIGHT: {
            errno = 0;
            int h = (int) strtol(argv[i], NULL, 10);
            if (h > 0 && errno == 0)
                res.h = h;
            state = SEARCHING;
            break;
        }
        }
    }

    return res;
}


static unsigned parseUpdateRate(int argc, char **argv) {
    unsigned u = GOLdefaultUpdateRate;
    for (int i = 0; i < argc - 1; ++i) {
        if (!strcmp(argv[i], "-u")) {
            errno = 0;
            u = (unsigned) strtoull(argv[i + 1], NULL, 10);
            if (errno != 0)
                u = GOLdefaultUpdateRate;
        }
    }
    return u;
}


int main(int argc, char **argv) {
    struct IntTuple res = parseResolution(argc - 1, argv + 1);
    unsigned updates = parseUpdateRate(argc - 1, argv + 1);
    gameOfLife(res.w, res.h, updates);
}
