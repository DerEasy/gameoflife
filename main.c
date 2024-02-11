#include "gameoflife.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
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
    struct IntTuple res = {GOL_defaultWindowWidth, GOL_defaultWindowHeight};
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
    unsigned u = GOL_defaultTickRate;
    for (int i = 0; i < argc - 1; ++i) {
        if (!strcmp(argv[i], "-u")) {
            errno = 0;
            u = (unsigned) strtoull(argv[i + 1], NULL, 10);
            if (errno != 0)
                u = GOL_defaultTickRate;
        }
    }
    return u;
}


static struct GOL_Pattern parsePatternToLoad(int argc, char **argv) {
    struct GOL_Pattern p = {NULL, GOL_NOTYPE, false};
    char *filename = NULL;
    for (int i = 0; i < argc - 1; ++i) {
        if (!strcmp(argv[i], "-p")) {
            filename = argv[i + 1];
            p.type = GOL_PLAINTEXT;
        } else if (!strcmp(argv[i], "-r")) {
            filename = argv[i + 1];
            p.type = GOL_RLE;
        }
    }

    if (!filename) return p;
    FILE *f = fopen(filename, "r");
    if (!f) return p;

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    rewind(f);

    char *pattern = malloc(size + 1);
    if (!pattern) return p;
    size_t tmp = fread(pattern, 1, size, f); (void) tmp;
    pattern[size] = '\0';
    fclose(f);

    p.pattern = pattern;
    p.freeString = true;
    return p;
}


int main(int argc, char **argv) {
    struct IntTuple res = parseResolution(argc - 1, argv + 1);
    unsigned updates = parseUpdateRate(argc - 1, argv + 1);
    struct GOL_Pattern patinfo = parsePatternToLoad(argc - 1, argv + 1);
    gameOfLife(res.w, res.h, updates, patinfo);
}
