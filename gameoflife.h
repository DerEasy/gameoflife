//
// Created by easy on 30.01.24.
//

#ifndef GAMEOFLIFE_GAMEOFLIFE_H
#define GAMEOFLIFE_GAMEOFLIFE_H

#include <stdbool.h>

enum {
    GOL_defaultWindowWidth = 1024,
    GOL_defaultWindowHeight = 768,
    GOL_defaultTickRate = 6
};

enum GOL_PatternType {
    GOL_NOTYPE,
    GOL_PLAINTEXT,
    GOL_RLE,
    GOL_INDETERMINATE
};

struct GOL_Pattern {
    const char *pattern;
    enum GOL_PatternType type;
    bool freeString;
};

/*
 * Start an instance of the Game of Life.
 * Supply custom window dimensions and an initial game tick rate or just use the defaults.
 * You may pass a pattern or set it to NULL if no pattern shall be loaded.
 *
 * Controls:
 * ENTER / P                - Pause or resume the game. The game is paused at start.
 * UP / W                   - Move camera up by one cell width.
 * DOWN / S                 - Move camera down by one cell width.
 * LEFT / A                 - Move camera left by one cell width.
 * RIGHT / D                - Move camera right by one cell width.
 * PLUS / WHEEL UP          - Zoom into the world. Hold CTRL to accelerate.
 * MINUS / WHEEL DOWN       - Zoom out of the world. Hold CTRL to accelerate.
 * LEFT CLICK               - Place a new cell in the world.
 * RIGHT CLICK              - Remove an existing cell from the world.
 * CLICK + DRAG             - Move camera in any direction with fine control.
 * BACKSPACE                - Clear the world.
 * Q                        - Decrease tick rate by 1; 10 when holding SHIFT, 100 when holding CTRL.
 * E                        - Increase tick rate by 1; 10 when holding SHIFT, 100 when holding CTRL.
 * B                        - Store a snapshot of the game state.
 * R                        - Restore the most recently stored game state snapshot.
 * Number keys              - Switch between available cell textures.
 * ESCAPE                   - Exit game.
 */
void gameOfLife(int w, int h, unsigned tickrate, struct GOL_Pattern patinfo);

#endif //GAMEOFLIFE_GAMEOFLIFE_H
