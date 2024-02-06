//
// Created by easy on 04.01.24.
//

#ifndef INDEPENDENTVIEW_SDL_VIEWPORT_H
#define INDEPENDENTVIEW_SDL_VIEWPORT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <SDL.h>

#ifndef STRUCT_DRECT_EXISTS
#define STRUCT_DRECT_EXISTS
// Like SDL_FRect, but with doubles. You may disable this struct declaration by defining STRUCT_DRECT_EXISTS
typedef struct DRect {
    double x, y, w, h;
} DRect;
#endif


/**
 * Fill source and destination rectangles for any object and viewport if that object is in drawing range.
 * This function expects positive (> 0) values for width and height of the object and the viewport or else
 * garbage values will be returned.
 * @param view the viewport
 * @param pos position and dimensions of drawable object
 * @param vdst the SDL_Rect structure describing where to draw the viewport to
 * @param texrect the SDL_Rect structure containing information about the texture used
 * @param src source SDL_Rect structure to be filled
 * @param dst destination SDL_Rect structure to be filled
 * @return true if the object is out of drawing range, false otherwise; source and destination SDL_Rect structures
 * are unmodified if false is returned
 */
bool sdl_getViewportRects(DRect *view, DRect *pos, SDL_Rect *vdst, SDL_Rect *texrect, SDL_Rect *src, SDL_Rect *dst);

/**
 * Fill source and destination rectangles for any object and viewport if that object is in drawing range.
 * This function expects positive (> 0) values for width and height of the object and the viewport or else
 * garbage values will be returned.
 * @param view the viewport
 * @param pos position and dimensions of drawable object
 * @param vdst the SDL_Rect structure describing where to draw the viewport to
 * @param texrect the SDL_Rect structure containing information about the texture used
 * @param src source SDL_Rect structure to be filled
 * @param dst destination SDL_FRect structure to be filled
 * @return false if the object is out of drawing range, true otherwise; source and destination SDL_Rect structures
 * are unmodified if true is returned
 */
bool sdl_getViewportFRects(DRect *view, DRect *pos, SDL_Rect *vdst, SDL_Rect *texrect, SDL_Rect *src, SDL_FRect *dst);

/**
 * Fill only destination rectangle for any object and viewport if that object is in drawing range.
 * This function expects positive (> 0) values for width and height of the object and the viewport or else
 * garbage values will be returned.
 * @param view the viewport
 * @param pos position and dimensions of drawable object
 * @param vdst the SDL_Rect structure describing where to draw the viewport to
 * @param dst destination SDL_Rect structure to be filled
 * @return false if the object is out of drawing range, true otherwise;
 * destination is unmodified if true is returned
 */
bool sdl_getViewportDstRect(DRect *view, DRect *pos, SDL_Rect *vdst, SDL_Rect *dst);

/**
 * Fill only destination rectangle for any object and viewport if that object is in drawing range.
 * This function expects positive (> 0) values for width and height of the object and the viewport or else
 * garbage values will be returned.
 * @param view the viewport
 * @param pos position and dimensions of drawable object
 * @param vdst the SDL_Rect structure describing where to draw the viewport to
 * @param dst destination SDL_FRect structure to be filled
 * @return false if the object is out of drawing range, true otherwise;
 * destination is unmodified if true is returned
 */
bool sdl_getViewportDstFRect(DRect *view, DRect *pos, SDL_Rect *vdst, SDL_FRect *dst);

/**
 * Determine if some object is inside a viewport's boundaries.
 * More generally: Determine if two rectangles overlap.
 * @param view the viewport
 * @param pos position and dimensions of object
 * @return true iff rectangles overlap (this includes overlapping edges)
 */
bool sdl_inViewport(DRect *view, DRect *pos);

#ifdef __cplusplus
};
#endif

#endif //INDEPENDENTVIEW_SDL_VIEWPORT_H
