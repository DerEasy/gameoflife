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
        if (!strcmp(argv[i], "-t")) {
            errno = 0;
            u = (unsigned) strtoull(argv[i + 1], NULL, 10);
            if (errno != 0)
                u = GOL_defaultTickRate;
        }
    }
    return u;
}


static struct GOL_Pattern parsePatternToLoad(int argc, char **argv) {
    struct GOL_Pattern p = {0};
    char *filename = NULL;
    for (int i = 0; i < argc - 1; ++i) {
        if (!strcmp(argv[i], "-fp")) {
            filename = argv[i + 1];
            p.type = GOL_PLAINTEXT;
        } else if (!strcmp(argv[i], "-fr")) {
            filename = argv[i + 1];
            p.type = GOL_RLE;
        } else if (!strcmp(argv[i], "-f")) {
            filename = argv[i + 1];
            p.type = GOL_INDETERMINATE;
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

    if (p.type == GOL_INDETERMINATE) {
        char *ending = strrchr(filename, '.');
        if (ending) {
            if (!strcmp(ending, ".cells"))
                p.type = GOL_PLAINTEXT;
            else if (!strcmp(ending, ".rle"))
                p.type = GOL_RLE;
        }
    }

    p.pattern = pattern;
    p.freePattern = true;
    return p;
}


static const char *parseRulestringToLoad(int argc, char **argv) {
    const char *rules = NULL;
    for (int i = 0; i < argc - 1; ++i) {
        if (!strcmp(argv[i], "-r"))
            rules = argv[i + 1];
    }
    return rules;
}


static bool showHelp(int argc, char **argv) {
    for (int i = 0; i < argc; ++i) {
        if (!strcmp(argv[i], "-h"))
            goto help;
        if (!strcmp(argv[i], "--help"))
            goto help;
    }

    return false;
help:
    puts(
        "The Game of Life - famous turing-complete cellular automaton zero-player game\n"
        "\n"
        "Options:\n"
        "    -h, --help       - Display this help screen.\n"
        "    -w               - Set initial window width.\n"
        "    -h               - Set initial window height.\n"
        "    -t               - Set initial game tick rate.\n"
        "    -fp              - Load plaintext pattern file.\n"
        "    -fr              - Load RLE pattern file.\n"
        "    -f               - Load pattern file. Type determined by file extension.\n"
        "    -r               - Override rulestring.\n"
        "Any option may override previous options. All options and their parameters are space-separated.\n"
        "\n"
        "\n"
        "Controls:\n"
        "    ENTER / P                - Pause or resume the game. The game is paused at start.\n"
        "    UP / W                   - Move camera up by one cell width.\n"
        "    DOWN / S                 - Move camera down by one cell width.\n"
        "    LEFT / A                 - Move camera left by one cell width.\n"
        "    RIGHT / D                - Move camera right by one cell width.\n"
        "    PLUS / WHEEL UP          - Zoom into the world. Hold CTRL to accelerate.\n"
        "    MINUS / WHEEL DOWN       - Zoom out of the world. Hold CTRL to accelerate.\n"
        "    LEFT CLICK               - Place a new cell in the world.\n"
        "    RIGHT CLICK              - Remove an existing cell from the world.\n"
        "    CLICK + DRAG             - Move camera in any direction with fine control.\n"
        "    BACKSPACE                - Clear the world.\n"
        "    Q                        - Decrease tick rate by 1; 10 when holding SHIFT, 100 when holding CTRL.\n"
        "    E                        - Increase tick rate by 1; 10 when holding SHIFT, 100 when holding CTRL.\n"
        "    B                        - Store a snapshot of the game state.\n"
        "    R                        - Restore the most recently stored game state snapshot.\n"
        "    Number keys              - Switch between available cell textures.\n"
        "    ESCAPE                   - Exit game."
    );
    return true;
}


int main(int argc, char **argv) {
    if (showHelp(argc - 1, argv + 1))
        return 0;
    struct IntTuple res = parseResolution(argc - 1, argv + 1);
    unsigned updates = parseUpdateRate(argc - 1, argv + 1);
    struct GOL_Pattern patinfo = parsePatternToLoad(argc - 1, argv + 1);
    patinfo.rules = parseRulestringToLoad(argc - 1, argv + 1);
    gameOfLife(res.w, res.h, updates, patinfo);
}
