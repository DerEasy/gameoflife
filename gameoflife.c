//
// Created by easy on 30.01.24.
//

#include "gameoflife.h"
#include "sdl_viewport.h"
#include "square0_png.h"
#include "square1_png.h"
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <errno.h>
#include <math.h>
#include <axvector.h>
#include <axqueue.h>
#include <axstack.h>
#include <SDL.h>
#include <SDL_image.h>

#define MAX(a, b) ((a) > (b) ? (a) : (b))

typedef enum InputType {
    ZOOM, CAMERA_VERTICAL, CAMERA_HORIZONTAL, SQUARE_PLACE,
    SQUARE_DELETE, PAUSE, GENOCIDE, TICKRATE, WINDOW_RESIZE,
    BACKUP, RESTORE, TEXTURE
} InputType;

typedef struct Square {
    double x, y;
} Square;

typedef struct Input {
    union {
        double magnitude;
        struct {
            int x, y;
        };
    };
    InputType type;
    bool usedMouse;
} Input;

typedef struct MouseTracker {
    int xDown, yDown;
} MouseTracker;

struct SingleRule {
    Uint8 nums[8];
    Uint8 len;
    bool isRange;
};

typedef struct Rules {
    struct SingleRule birth, survival;
} Rules;

// set origin to NULL before calling filter with removeDuplicates()
struct args_removeDuplicates {
    int (*comp)(const void *, const void *);
    void *origin;
};


static bool tick(void);
static bool handleEvents(void);
static void *getTinyMemory(void);
static void update(void);
static void draw(void);
static void destructSquare(void *);
static void destructInput(void *);
static void destructSnapshot(void *);
static bool filterEqualSquares(const void *, void *);
static bool removeDuplicates(const void *, void *);
static void *mapNewSquares(void *);
static int compareSquares(const void *, const void *);
static void processInputs(void);
static void processLife(void);
static void loadPlaintextPattern(const char *);
static char *loadRLEPattern(const char *);
static Rules parseRulestring(const char *);


static SDL_Window *window;
static SDL_Renderer *renderer;
static SDL_Texture *textures[2];
static SDL_Texture *chosenTexture;
static Rules rules;
static axstack *tinyPool;
static axvector *squares;
static axqueue *inputs;
static axstack *snapshots;
static DRect camera;
static DRect defaultCamera;     // width is always the same, height is multiplied by display ratio
static double zoom;
static MouseTracker mouseleft;
static MouseTracker mouseright;
static Uint64 updateAccumulator;
static Uint64 tickTimeAccumulator;
static Uint64 updatesPerSec;
static Uint64 tickrate;
static bool paused;


void gameOfLife(int w, int h, unsigned tickrate_, struct GOL_Pattern patinfo) {
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER);

    window = SDL_CreateWindow("Game of Life", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, w, h, SDL_WINDOW_RESIZABLE);
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    SDL_RWops *embeddedTexture = SDL_RWFromConstMem(square0_png, sizeof square0_png);
    textures[0] = IMG_LoadTexture_RW(renderer, embeddedTexture, true);
    embeddedTexture = SDL_RWFromConstMem(square1_png, sizeof square1_png);
    textures[1] = IMG_LoadTexture_RW(renderer, embeddedTexture, true);
    chosenTexture = *textures;
    SDL_SetRenderDrawColor(renderer, 0xFF, 0xFF, 0xFF, SDL_ALPHA_OPAQUE);
    SDL_DisplayMode dm; SDL_GetCurrentDisplayMode(SDL_GetWindowDisplayIndex(window), &dm);
    squares = axv.setDestructor(axv.setComparator(axv.new(), compareSquares), destructSquare);
    inputs = axq.setDestructor(axq.new(), destructInput);
    tinyPool = axs.setDestructor(axs.new(), free);
    snapshots = axs.setDestructor(axs.new(), destructSnapshot);
    updatesPerSec = dm.refresh_rate;
    tickrate = tickrate_;
    zoom = 1. / (1 << 2);
    paused = true;
    defaultCamera = (DRect) {0, 0, 120, ((double) h / (double) w) * 120};   // display ratio in height
    camera = (DRect) {0, 0, defaultCamera.w * zoom, defaultCamera.h * zoom};

    if (patinfo.pattern) {
        if (patinfo.type == GOL_PLAINTEXT)
            loadPlaintextPattern(patinfo.pattern);
        if (patinfo.type == GOL_RLE) {
            char *rulestring = loadRLEPattern(patinfo.pattern);
            if (!patinfo.rules) {
                patinfo.rules = rulestring;
                patinfo.freeRulestring = true;
            }
            else free(rulestring);
        }
    }

    rules = parseRulestring(patinfo.rules ? patinfo.rules : "B3/S23");
    if (patinfo.freeRulestring)
        free((void *) patinfo.rules);
    if (patinfo.freePattern)
        free((void *) patinfo.pattern);

    while (tick());

    axs.destroy(snapshots);
    axv.destroy(squares);
    axq.destroy(inputs);
    axs.destroy(tinyPool);
    SDL_DestroyTexture(textures[0]);
    SDL_DestroyTexture(textures[1]);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
}


static bool tick(void) {
    Uint64 starttime = SDL_GetPerformanceCounter();

    if (handleEvents())
        return false;
    update();
    draw();

    Uint64 difftime = SDL_GetPerformanceCounter() - starttime;
    updateAccumulator += difftime;
    tickTimeAccumulator += difftime * !paused;
    return true;
}


static void update(void) {
    const Uint64 updateDuration = SDL_GetPerformanceFrequency() / updatesPerSec;
    const Uint64 gametickDuration = SDL_GetPerformanceFrequency() / tickrate;

    processInputs();
    if (updateAccumulator >= updateDuration) {
        if (!paused) {
            Uint64 frametimeConsumed = 0;
            while (tickTimeAccumulator >= gametickDuration && frametimeConsumed < updateDuration) {
                Uint64 starttime = SDL_GetPerformanceCounter();
                processLife();
                frametimeConsumed += SDL_GetPerformanceCounter() - starttime;
                tickTimeAccumulator -= gametickDuration;
            }
        }
        updateAccumulator -= updateDuration;
    }
}


// insertion sort the last n items into the vector (obvious pre-condition: rest of vector is sorted)
static void insertionSortTail(axvector *v, int n) {
    if (n <= 0)
        return;

    axvsnap s1 = axv.snapshot(v);
    s1.i = axv.len(v) - n;
    axvsnap s2 = axv.snapshot(v);
    int (*cmp)(const void *, const void *) = axv.getComparator(v);

    for (; s1.i < s1.len; ++s1.i) {
        for (s2.i = s1.i; s2.i > 0; --s2.i) {
            if (cmp(&s2.vec[s2.i], &s2.vec[s2.i - 1]) >= 0)
                break;
            void *tmp = s2.vec[s2.i];
            s2.vec[s2.i] = s2.vec[s2.i - 1];
            s2.vec[s2.i - 1] = tmp;
        }
    }
}


static bool determineWorthy(void *square, void *args) {
    axvector *survivors = ((axvector **) args)[0];
    axvector *potentials = ((axvector **) args)[1];
    Square *s = square;
    int taillen = 0;
    Uint8 neighbours = 0;

    for (double offsetX = -1; offsetX <= +1; ++offsetX) {
        for (double offsetY = -1; offsetY <= +1; ++offsetY) {
            if (offsetX == 0 && offsetY == 0)
                continue;

            Square neighbour = {s->x + offsetX, s->y + offsetY};
            long i = axv.binarySearch(squares, &neighbour);
            neighbours += i != -1;

            if (i == -1 && axv.binarySearch(potentials, &neighbour) == -1) {
                Square *potential = getTinyMemory();
                *potential = neighbour;
                axv.push(potentials, potential);
                ++taillen;
            }
        }
    }

    // insertion sorting the last few items is HUGELY more efficient than
    // calling axv.sort(potentials) every damn time this function is called (which is a lot!)
    insertionSortTail(potentials, taillen);

    if (rules.survival.isRange) {
        if (rules.survival.nums[0] <= neighbours && neighbours <= rules.survival.nums[1])
            axv.push(survivors, s);
    } else {
        if (memchr(rules.survival.nums, neighbours, rules.birth.len))
            axv.push(survivors, s);
    }

    return true;
}


static bool determineSpawning(const void *square, void *_) {
    (void) _;
    const Square *s = square;
    Uint8 neighbours = 0;

    for (double offsetX = -1; offsetX <= +1; ++offsetX) {
        for (double offsetY = -1; offsetY <= +1; ++offsetY) {
            if (offsetX == 0 && offsetY == 0)
                continue;

            Square ns = {s->x + offsetX, s->y + offsetY};
            long i = axv.binarySearch(squares, &ns);
            neighbours += i != -1;

            if (neighbours > rules.birth.nums[rules.birth.len - 1])
                return false;
        }
    }

    if (rules.birth.isRange)
        return rules.birth.nums[0] <= neighbours && neighbours <= rules.birth.nums[1];
    else
        return memchr(rules.birth.nums, neighbours, rules.birth.len);
}


static bool keepIdenticalSquares(const void *square, void *survivors) {
    if (square == axv.top(survivors)) {
        axv.pop(survivors);
        return true;
    } else {
        return false;
    }
}


static void processLife(void) {
    axvector *potentials = axv.setDestructor(axv.setComparator(axv.new(), compareSquares), destructSquare);
    axvector *survivors = axv.new();
    struct args_removeDuplicates argsrd = {axv.getComparator(squares), NULL};
    axv.filter(axv.sort(squares), removeDuplicates, &argsrd);
    axv.foreach(squares, determineWorthy, (axvector *[2]) {survivors, potentials});
    axv.filter(potentials, determineSpawning, NULL);
    axv.filter(squares, keepIdenticalSquares, axv.reverse(survivors));
    axv.extend(squares, potentials);
    axv.destroy(survivors);
    axv.destroy(potentials);
}


static void processInputs(void) {
    int renW;   // width only because height is composite of width times display ratio
    SDL_GetRendererOutputSize(renderer, &renW, NULL);

    for (Input *input; axq.len(inputs); axs.push(tinyPool, input)) {
        input = axq.dequeue(inputs);

        switch (input->type) {
        case CAMERA_VERTICAL: {
            if (input->usedMouse)   // 8.5 seems to be some kind of magic number to get correct movement speed
                camera.y += input->magnitude * zoom / 8.5 * ((double) GOL_defaultWindowWidth / renW);
            else
                camera.y += input->magnitude;
            break;
        }
        case CAMERA_HORIZONTAL: {
            if (input->usedMouse)
                camera.x += input->magnitude * zoom / 8.5 * ((double) GOL_defaultWindowWidth / renW);
            else
                camera.x += input->magnitude;
            break;
        }
        case ZOOM: {
            SDL_Keymod mod = SDL_GetModState();
            double zoomDiff;
            if (mod & KMOD_CTRL)
                zoomDiff = input->magnitude * (1. / (1 << 2));
            else
                zoomDiff = input->magnitude * (1. / (1 << 6));

            if (zoom - zoomDiff > 0) {
                zoom -= zoomDiff;
                camera.x += zoomDiff * defaultCamera.w / 2;
                camera.y += zoomDiff * defaultCamera.h / 2;
                camera.w = defaultCamera.w * zoom;
                camera.h = defaultCamera.h * zoom;
            }
            break;
        }
        case SQUARE_PLACE: {
            Square *square = getTinyMemory();
            double ratio = renW / camera.w;
            square->x = floor(camera.x + (double) input->x / ratio);
            square->y = floor(camera.y + (double) input->y / ratio);
            axv.push(squares, square);
            break;
        }
        case SQUARE_DELETE: {
            double cameraRatio = renW / camera.w;
            Square square = {
                    floor(camera.x + (double) input->x / cameraRatio),
                    floor(camera.y + (double) input->y / cameraRatio)
            };
            axv.filter(squares, filterEqualSquares, &square);
            break;
        }
        case PAUSE: {
            paused = !paused;
            break;
        }
        case GENOCIDE: {
            axv.clear(squares);
            break;
        }
        case TICKRATE: {
            SDL_Keymod mod = SDL_GetModState();
            Sint64 speedOffset = (Sint64) input->magnitude;
            if (mod & KMOD_CTRL)
                speedOffset *= 100;
            else if (mod & KMOD_SHIFT)
                speedOffset *= 10;
            tickrate += speedOffset;
            if ((Sint64) tickrate < 1)
                tickrate = 1;
            break;
        }
        case WINDOW_RESIZE: {
            double displayRatio = (double) input->y / (double) input->x;
            SDL_DisplayMode dm;
            SDL_GetCurrentDisplayMode(SDL_GetWindowDisplayIndex(window), &dm);
            defaultCamera = (DRect) {0, 0, 120, displayRatio * 120};
            camera.w = defaultCamera.w * zoom;
            camera.h = defaultCamera.h * zoom;
            break;
        }
        case BACKUP: {
            axs.push(snapshots, axv.setDestructor(axv.map(axv.copy(squares), mapNewSquares), axv.getDestructor(squares)));
            break;
        }
        case RESTORE: {
            if (axs.len(snapshots)) {
                axv.destroy(squares);
                squares = axs.pop(snapshots);
            }
            break;
        }
        case TEXTURE: {
            chosenTexture = textures[input->x];
            break;
        }
        }
    }
}


static void draw(void) {
    SDL_RenderClear(renderer);

    SDL_Rect vdst;
    vdst.x = vdst.y = 0;
    SDL_GetRendererOutputSize(renderer, &vdst.w, &vdst.h);
    DRect pos;
    pos.w = pos.h = 1;

    for (axvsnap s = axv.snapshot(squares); s.i < s.len; ++s.i) {
        SDL_FRect dst;
        Square *square = s.vec[s.i];
        pos.x = square->x;
        pos.y = square->y;
        if (sdl_inViewport(&camera, &pos)) {
            sdl_getViewportDstFRect(&camera, &pos, &vdst, &dst);
            SDL_RenderCopyF(renderer, chosenTexture, NULL, &dst);
        }
    }

    SDL_RenderPresent(renderer);
}


static bool handleEvents(void) {
    while (axq.len(inputs) > 1024)
        axs.push(tinyPool, axq.dequeue(inputs));

    for (SDL_Event e; SDL_PollEvent(&e); ) {
        if (e.type == SDL_KEYDOWN) {
            switch (e.key.keysym.sym) {
            case SDLK_ESCAPE:
                return true;
            case SDLK_UP:
            case SDLK_w: {
                Input *input = getTinyMemory();
                input->type = CAMERA_VERTICAL;
                input->magnitude = -1;
                input->usedMouse = false;
                axq.enqueue(inputs, input);
                break;
            }
            case SDLK_DOWN:
            case SDLK_s: {
                Input *input = getTinyMemory();
                input->type = CAMERA_VERTICAL;
                input->magnitude = 1;
                input->usedMouse = false;
                axq.enqueue(inputs, input);
                break;
            }
            case SDLK_LEFT:
            case SDLK_a: {
                Input *input = getTinyMemory();
                input->type = CAMERA_HORIZONTAL;
                input->magnitude = -1;
                input->usedMouse = false;
                axq.enqueue(inputs, input);
                break;
            }
            case SDLK_RIGHT:
            case SDLK_d: {
                Input *input = getTinyMemory();
                input->type = CAMERA_HORIZONTAL;
                input->magnitude = 1;
                input->usedMouse = false;
                axq.enqueue(inputs, input);
                break;
            }
            case SDLK_PLUS:
            case SDLK_KP_PLUS: {
                Input *input = getTinyMemory();
                input->type = ZOOM;
                input->magnitude = 1;
                axq.enqueue(inputs, input);
                break;
            }
            case SDLK_MINUS:
            case SDLK_KP_MINUS: {
                Input *input = getTinyMemory();
                input->type = ZOOM;
                input->magnitude = -1;
                axq.enqueue(inputs, input);
                break;
            }
            case SDLK_RETURN:
            case SDLK_KP_ENTER:
            case SDLK_p: {
                Input *input = getTinyMemory();
                input->type = PAUSE;
                axq.enqueue(inputs, input);
                break;
            }
            case SDLK_BACKSPACE: {
                Input *input = getTinyMemory();
                input->type = GENOCIDE;
                axq.enqueue(inputs, input);
                break;
            }
            case SDLK_q: {
                Input *input = getTinyMemory();
                input->type = TICKRATE;
                input->magnitude = -1;
                axq.enqueue(inputs, input);
                break;
            }
            case SDLK_e: {
                Input *input = getTinyMemory();
                input->type = TICKRATE;
                input->magnitude = 1;
                axq.enqueue(inputs, input);
                break;
            }
            case SDLK_b: {
                Input *input = getTinyMemory();
                input->type = BACKUP;
                axq.enqueue(inputs, input);
                break;
            }
            case SDLK_r: {
                Input *input = getTinyMemory();
                input->type = RESTORE;
                axq.enqueue(inputs, input);
                break;
            }
            case SDLK_KP_1:
            case SDLK_1: {
                Input *input = getTinyMemory();
                input->type = TEXTURE;
                input->x = 0;
                axq.enqueue(inputs, input);
                break;
            }
            case SDLK_KP_2:
            case SDLK_2: {
                Input *input = getTinyMemory();
                input->type = TEXTURE;
                input->x = 1;
                axq.enqueue(inputs, input);
                break;
            }
            }
        }

        else if (e.type == SDL_MOUSEBUTTONDOWN) {
            MouseTracker *tracker = e.button.button == SDL_BUTTON_LEFT ? &mouseleft : &mouseright;
            SDL_GetMouseState(&tracker->xDown, &tracker->yDown);
        }

        else if (e.type == SDL_MOUSEMOTION) {
            if (e.motion.state & (SDL_BUTTON_LMASK | SDL_BUTTON_RMASK)) {
                Input *input = getTinyMemory();
                input->type = CAMERA_VERTICAL;
                input->magnitude = -e.motion.yrel;
                input->usedMouse = true;
                axq.enqueue(inputs, input);
                input = getTinyMemory();
                input->type = CAMERA_HORIZONTAL;
                input->magnitude = -e.motion.xrel;
                input->usedMouse = true;
                axq.enqueue(inputs, input);
            }
        }

        else if (e.type == SDL_MOUSEBUTTONUP) {
            bool left = e.button.button == SDL_BUTTON_LEFT;
            MouseTracker *tracker = left ? &mouseleft : &mouseright;
            int xUp, yUp;
            SDL_GetMouseState(&xUp, &yUp);

            if (tracker->xDown == xUp && tracker->yDown == yUp) {
                Input *input = getTinyMemory();
                input->type = left ? SQUARE_PLACE : SQUARE_DELETE;
                input->x = xUp;
                input->y = yUp;
                axq.enqueue(inputs, input);
            }
        }

        else if (e.type == SDL_MOUSEWHEEL) {
            Input *input = getTinyMemory();
            input->type = ZOOM;
            input->magnitude = e.wheel.preciseY;
            axq.enqueue(inputs, input);
        }

        else if (e.type == SDL_WINDOWEVENT) {
            if (e.window.event == SDL_WINDOWEVENT_RESIZED) {
                Input *input = getTinyMemory();
                input->type = WINDOW_RESIZE;
                input->x = e.window.data1;
                input->y = e.window.data2;
                axq.enqueue(inputs, input);
            }
        }

        else if (e.type == SDL_QUIT)
            return true;
    }

    return false;
}


static void *getTinyMemory(void) {
    while (axs.len(tinyPool) > 1000000)
        axs.destroyItem(tinyPool, axs.pop(tinyPool));
    if (axs.len(tinyPool) != 0)
        return axs.pop(tinyPool);

    for (int i = 0; i < 16; ++i) {
        void *p = malloc(MAX(sizeof(Input), sizeof(Square)));
        if (!p || axs.push(tinyPool, p)) {
            fprintf(stderr, "Tiny memory pool ran out of memory.\n");
            abort();
        }
    }
    
    return axs.pop(tinyPool);
}


static void destructSquare(void *s) {
    if (s) axs.push(tinyPool, s);
}


static void destructInput(void *i) {
    if (i) axs.push(tinyPool, i);
}


static void destructSnapshot(void *v) {
    if (v) axv.destroy(v);
}


static void destructTemporary(void *t) {
    if (t) axs.push(tinyPool, t);
}


static int compareSquares(const void *a, const void *b) {
    const Square *s1 = *(Square **) a;
    const Square *s2 = *(Square **) b;
    const int xOrder = (s1->x > s2->x) - (s1->x < s2->x);
    if (xOrder) return xOrder;
    return (s1->y > s2->y) - (s1->y < s2->y);
}


static bool filterEqualSquares(const void *s, void *arg) {
    return compareSquares(&s, &arg);
}


static void *mapNewSquares(void *square) {
    Square *s1 = square;
    Square *s2 = getTinyMemory();
    *s2 = *s1;
    return s2;
}


/* PRE-CONDITION: vector is sorted! */
static bool removeDuplicates(const void *current, void *args_) {
    struct args_removeDuplicates *args = args_;

    if (args->origin && args->comp(&args->origin, &current) == 0) {
        return false;
    } else {
        args->origin = (void *) current;
        return true;
    }
}


static void loadPlaintextPattern(const char *s) {
    while (*s && *s == '!') {
        while (*s && *s != '\n')
            ++s;
        s += !!*s;
    }

    for (Sint64 x = 0, y = 0; *s; ++s, ++x) {
        if (*s == '.')
            continue;
        if (*s == '\r')
            continue;
        if (*s == '\n') {
            x = -1;
            ++y;
            continue;
        }
        if (*s == 'O') {
            Square *square = getTinyMemory();
            square->x = (double) x;
            square->y = (double) y;
            axv.push(squares, square);
        }
    }
}


static char *loadRLEPattern(const char *s) {
    while (*s && *s == '#') {
        while (*s && *s != '\n')
            ++s;
        s += !!*s;
    }

    Uint64 w = 0, h = 0;
    for (Uint64 *fill = &w; *s && *s != '\n'; ++s) {
        if (!isdigit(*s))
            continue;

        errno = 0;
        char *tmp;
        *fill = strtoull(s, &tmp, 10);
        if (errno) return NULL;
        s = tmp - 1;
        if (fill == &h) break;
        fill = &h;
    }

    char *rulestring = NULL;
    while (*s != '\n') {
        if (*s == '=') {
            do ++s; while (*s != '\n' && isspace(*s));
            if (*s == '\n') break;
            const char *start = s;
            const char *end = strchr(s, '\n');
            rulestring = malloc(end - start);
            memcpy(rulestring, start, end - start - 1);
            rulestring[end - start] = '\0';
        }
        ++s;
    }

    enum States {COUNT, TAG};
    enum States state = COUNT;
    for (Sint64 x = 0, y = 0, count = 0; *s && *s != '!'; ++s) {
        if (isspace(*s))
            continue;

        if (state == COUNT) {
            if (isdigit(*s)) {
                int digit = *s - '0';
                count = count * 10 + digit;
            } else {
                count += !count;
                state = TAG;
                --s;
            }
        }

        else /*if (state == TAG)*/ {
            if (*s == 'b') {
                x += count;
            } else if (*s == '$') {
                x = 0;
                y += count;
            } else if (*s == 'o') {
                while (count--) {
                    Square *square = getTinyMemory();
                    square->x = (double) x++;
                    square->y = (double) y;
                    axv.push(squares, square);
                }
            }

            count = 0;
            state = COUNT;
        }
    }

    return rulestring;
}


static int cmpUint8(const void *a, const void *b) {
    const Uint8 x = **(const Uint8 **) a;
    const Uint8 y = **(const Uint8 **) b;
    return (x > y) - (x < y);
}


static void initSingleRule(axvector *nums, struct SingleRule *r) {
    struct args_removeDuplicates argsrd = {axv.getComparator(nums), NULL};
    axv.filter(axv.sort(nums), removeDuplicates, &argsrd);
    if (axv.count(nums, &(Uint8) {9}))
        axv.discard(nums, 1);

    bool isRange = true;
    for (axvsnap q = axv.snapshot(nums); q.i < q.len - 1; ++q.i) {
        Uint8 *x = q.vec[q.i];
        Uint8 *y = q.vec[q.i + 1];
        if (*x + 1 != *y) {
            isRange = false;
            break;
        }
    }

    if (isRange) {
        r->nums[0] = *(Uint8 *) axv.at(nums, 0);
        r->nums[1] = *(Uint8 *) axv.at(nums, -1);
        r->isRange = true;
        r->len = 2;
    } else {
        axv.reverse(nums);
        r->len = axv.len(nums);
        r->isRange = false;
        for (int i = 0; axv.len(nums); ++i, axv.discard(nums, 1))
            r->nums[i] = *(Uint8 *) axv.top(nums);
    }

    axv.clear(nums);
}


static Rules parseRulestring(const char *s) {
    axvector *nums = axv.setDestructor(axv.setComparator(axv.sizedNew(10), cmpUint8), destructTemporary);
    Rules r;

    // currently only doing B/S notation starting at 'B'
    ++s;
    for (int i = 0; isdigit(*s); ++i, ++s) {
        Uint8 *d = getTinyMemory();
        *d = *s - '0';
        axv.push(nums, d);
    }

    if (axv.len(nums)) {
        initSingleRule(nums, &r.birth);
    } else {
        r.birth.nums[0] = 3;
        r.birth.nums[1] = 3;
        r.birth.isRange = true;
        r.birth.len = 2;
    }

    while (*s && *s != 'S' && *s != 's') ++s;

    s += !!*s;
    for (int i = 0; isdigit(*s); ++i, ++s) {
        Uint8 *d = getTinyMemory();
        *d = *s - '0';
        axv.push(nums, d);
    }

    if (axv.len(nums)) {
        initSingleRule(nums, &r.survival);
    } else {
        r.survival.nums[0] = 2;
        r.survival.nums[1] = 3;
        r.survival.isRange = true;
        r.birth.len = 2;
    }

    axv.destroy(nums);
    return r;
}


