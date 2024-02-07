//
// Created by easy on 30.01.24.
//

#ifndef GAMEOFLIFE_GAMEOFLIFE_H
#define GAMEOFLIFE_GAMEOFLIFE_H

enum {
    GOLdefaultWindowWidth = 1024,
    GOLdefaultWindowHeight = 768,
    GOLdefaultUpdateRate = 6
};

void gameOfLife(int w, int h, unsigned updates);

#endif //GAMEOFLIFE_GAMEOFLIFE_H
