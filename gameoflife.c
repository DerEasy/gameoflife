//
// Created by easy on 30.01.24.
//

#include "gameoflife.h"
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>
#include <axvector.h>
#include <axqueue.h>
#include <axstack.h>
#include <SDL.h>
#include <SDL_image.h>
#include "sdl_viewport.h"

#define MAX(a, b) ((a) > (b) ? (a) : (b))

typedef enum InputType {
    ZOOM, CAMERA_VERTICAL, CAMERA_HORIZONTAL, SQUARE_PLACE, SQUARE_DELETE, PAUSE, GENOCIDE
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
static bool filterEqualSquares(const void *, void *);
static bool removeDuplicates(const void *, void *);
static int compareSquares(const void *, const void *);
static void processInputs(void);
static void processLife(void);


static SDL_Window *window;
static SDL_Renderer *renderer;
static SDL_Texture *texsq;
static axstack *tinyPool;
static axvector *squares;
static axqueue *inputs;
static DRect camera;
static DRect defaultCamera;
static double zoom;
static MouseTracker mouseleft;
static MouseTracker mouseright;
static Uint64 updateAccumulator;
static Uint64 updatesPerSec;
static Uint64 tickcount;
static Uint64 displayHz;
static bool paused;


void gameOfLife(int w, int h, uint64_t updates) {
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER);
    double displayRatio = (double) w / (double) h;
    SDL_DisplayMode dm;
    defaultCamera = camera = (DRect) {0, 0, displayRatio * 120, 120};
    paused = true;
    zoom = 1. / (1 << 3);
    tickcount = 0;
    window = SDL_CreateWindow("Game of Life", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, w, h, 0);
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    texsq = IMG_LoadTexture(renderer, "square2.png");
    SDL_SetRenderDrawColor(renderer, 0xFF, 0xFF, 0xFF, SDL_ALPHA_OPAQUE);
    SDL_GetCurrentDisplayMode(SDL_GetWindowDisplayIndex(window), &dm);
    updatesPerSec = updates ? updates : (uint64_t) dm.refresh_rate;
    displayHz = dm.refresh_rate;
    tinyPool = axs.setDestructor(axs.new(), free);
    squares = axv.setDestructor(axv.setComparator(axv.new(), compareSquares), destructSquare);
    inputs = axq.setDestructor(axq.new(), free);
    
    Input *initZoom = getTinyMemory();        // generate artificial initial zoom-in
    initZoom->type = ZOOM;
    initZoom->magnitude = 0;
    axq.enqueue(inputs, initZoom);

    while (tick());

    axv.destroy(squares);
    axq.destroy(inputs);
    axs.destroy(tinyPool);
    SDL_DestroyTexture(texsq);
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

    Uint64 endtime = SDL_GetPerformanceCounter();
    updateAccumulator += endtime - starttime;
    return true;
}


static void update(void) {
    const Uint64 updateDuration = SDL_GetPerformanceFrequency() / displayHz;

    int limit = 6;
    while (updateAccumulator >= updateDuration && limit-- > 0) {
        processInputs();
        if (!paused && tickcount % MAX((displayHz * 10) / updatesPerSec, 1) == 0)
            processLife();
        updateAccumulator -= updateDuration;
        ++tickcount;
    }
}


// insertion sort the last n items into the vector (obvious pre-condition: rest of vector is sorted)
static void insertionSortTail(axvector *v, long n) {
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
    long taillen = 0;
    int neighbours = 0;

    // removing duplicates is almost as bad as just doing linear search
    //struct args_removeDuplicates argsrd = {axv.getComparator(potentials), NULL};
    //axv.filter(axv.sort(potentials), removeDuplicates, &argsrd);
    // this little shit hogs up 55,43% of cpu time
    //axv.sort(potentials);
    // should definitely consider using some inherently ordered data structure like rb-trees

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

    if (neighbours == 2 || neighbours == 3)
        axv.push(survivors, s);

    return true;
}


static bool determineSpawning(const void *square, void *_) {
    (void) _;
    const Square *s = square;
    int neighbours = 0;

    for (double offsetX = -1; offsetX <= +1; ++offsetX) {
        for (double offsetY = -1; offsetY <= +1; ++offsetY) {
            if (offsetX == 0 && offsetY == 0)
                continue;

            Square ns = {s->x + offsetX, s->y + offsetY};
            long i = axv.binarySearch(squares, &ns);
            neighbours += i != -1;

            if (neighbours > 3)
                return false;
        }
    }

    return neighbours == 3;
}


static bool keepIdenticalSquares(const void *square, void *arg) {
    const Square *s = square;
    axvector *alive = arg;

    if (s == axv.top(alive)) {
        axv.pop(alive);
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
    for (Input *input; axq.len(inputs); axs.push(tinyPool, input)) {
        input = axq.dequeue(inputs);

        switch (input->type) {
        case CAMERA_VERTICAL: {
            if (input->usedMouse)
                camera.y += input->magnitude * zoom / 5;
            else
                camera.y += input->magnitude;
            break;
        }
        case CAMERA_HORIZONTAL: {
            if (input->usedMouse)
                camera.x += input->magnitude * zoom / 5;
            else
                camera.x += input->magnitude;
            break;
        }
        case ZOOM: {
            double resetZoom = zoom;
            double resetW = camera.w;
            double resetH = camera.h;
            zoom -= input->magnitude * (1. / (1 << 6));
            camera.w = defaultCamera.w * zoom;
            camera.h = defaultCamera.h * zoom;
            if (camera.w == 0 || camera.h == 0) {
                zoom = resetZoom;
                camera.w = resetW;
                camera.h = resetH;
            }
            break;
        }
        case SQUARE_PLACE: {
            Square *square = getTinyMemory();
            int rw;
            SDL_GetRendererOutputSize(renderer, &rw, NULL);
            double ratio = rw / camera.w;
            square->x = floor(camera.x + (double) input->x / ratio);
            square->y = floor(camera.y + (double) input->y / ratio);
            axv.push(squares, square);
            break;
        }
        case SQUARE_DELETE: {
            int rw;
            SDL_GetRendererOutputSize(renderer, &rw, NULL);
            double cameraRatio = rw / camera.w;
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
            SDL_RenderCopyF(renderer, texsq, NULL, &dst);
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
