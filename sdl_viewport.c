//
// Created by easy on 31.12.23.
//

#include "sdl_viewport.h"

static inline double max(double x, double y) {return x > y ? x : y;}

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
 * @return false if the object is out of drawing range, true otherwise; source and destination SDL_Rect structures
 * are unmodified if true is returned
 */
bool sdl_getViewportRects(DRect *view, DRect *pos, SDL_Rect *vdst, SDL_Rect *texrect, SDL_Rect *src, SDL_Rect *dst) {
    typedef struct Point {double x, y;} Point;

    // p - point; v/o - viewport/object; 1/2 - top left corner/bottom right corner
    const Point pv1 = {view->x, view->y},
                pv2 = {view->x + view->w, view->y + view->h};
    const Point po1 = {pos->x, pos->y},
                po2 = {pos->x + pos->w, pos->y + pos->h};

    // leftclip is the area that is out of the left side of the viewport range as a ratio of the drawable object
    // greater or equal to 0; analogous definition for the rest of the clips
    const double leftclip  = max(0, pv1.x - po1.x) / pos->w;
    const double rightclip = max(0, po2.x - pv2.x) / pos->w;
    const double upclip    = max(0, pv1.y - po1.y) / pos->h;
    const double downclip  = max(0, po2.y - pv2.y) / pos->h;

    // if we clip 100% or more of any direction of the object, the object is out of range...
    // beyond this if-statement, we can be sure that at least some part of the object is in range
    if (leftclip + rightclip >= 1 || upclip + downclip >= 1)
        return false;

    // first x and y coordinates of object that are in range
    const double originX = max(po1.x, pv1.x);
    const double originY = max(po1.y, pv1.y);
    // texture needs to be scaled accordingly if viewport and vdst dimensions mismatch;
    // if aspect ratios mismatch, then vscaleW != vscaleH, which means the texture is stretched
    const double vscaleW = vdst->w / view->w;
    const double vscaleH = vdst->h / view->h;

    src->x = (int) (texrect->w *      leftclip  + texrect->x);
    src->y = (int) (texrect->h *      upclip    + texrect->y);
    src->w = (int) (texrect->w * (1 - leftclip) - texrect->w * rightclip);
    src->h = (int) (texrect->h * (1 - upclip  ) - texrect->h * downclip );

    dst->x = (int) (vscaleW * (originX - pv1.x) + vdst->x);
    dst->y = (int) (vscaleH * (originY - pv1.y) + vdst->y);
    dst->w = (int) (vscaleW * (pos->w  - pos->w * (leftclip + rightclip)));
    dst->h = (int) (vscaleH * (pos->h  - pos->h * (upclip   + downclip )));

    return true;   // object is in range
}


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
bool sdl_getViewportFRects(DRect *view, DRect *pos, SDL_Rect *vdst, SDL_Rect *texrect, SDL_Rect *src, SDL_FRect *dst) {
    typedef struct Point {double x, y;} Point;

    // p - point; v/o - viewport/object; 1/2 - top left corner/bottom right corner
    const Point pv1 = {view->x, view->y},
                pv2 = {view->x + view->w, view->y + view->h};
    const Point po1 = {pos->x, pos->y},
                po2 = {pos->x + pos->w, pos->y + pos->h};

    // leftclip is the area that is out of the left side of the viewport range as a ratio of the drawable object
    // greater or equal to 0; analogous definition for the rest of the clips
    const double leftclip  = max(0, pv1.x - po1.x) / pos->w;
    const double rightclip = max(0, po2.x - pv2.x) / pos->w;
    const double upclip    = max(0, pv1.y - po1.y) / pos->h;
    const double downclip  = max(0, po2.y - pv2.y) / pos->h;

    // if we clip 100% or more of any direction of the object, the object is out of range...
    // beyond this if-statement, we can be sure that at least some part of the object is in range
    if (leftclip + rightclip >= 1 || upclip + downclip >= 1)
        return false;

    // first x and y coordinates of object that are in range
    const double originX = max(po1.x, pv1.x);
    const double originY = max(po1.y, pv1.y);
    // texture needs to be scaled accordingly if viewport and vdst dimensions mismatch;
    // if aspect ratios mismatch, then vscaleW != vscaleH, which means the texture is stretched
    const double vscaleW = vdst->w / view->w;
    const double vscaleH = vdst->h / view->h;

    src->x = (int) (texrect->w *      leftclip  + texrect->x);
    src->y = (int) (texrect->h *      upclip    + texrect->y);
    src->w = (int) (texrect->w * (1 - leftclip) - texrect->w * rightclip);
    src->h = (int) (texrect->h * (1 - upclip  ) - texrect->h * downclip );

    dst->x = (float) (vscaleW * (originX - pv1.x) + vdst->x);
    dst->y = (float) (vscaleH * (originY - pv1.y) + vdst->y);
    dst->w = (float) (vscaleW * (pos->w  - pos->w * (leftclip + rightclip)));
    dst->h = (float) (vscaleH * (pos->h  - pos->h * (upclip   + downclip )));

    return true;   // object is in range
}


/**
 * Determine if some object is inside a viewport's boundaries.
 * More generally: Determine if two rectangles overlap.
 * @param view the viewport
 * @param pos position and dimensions of object
 * @return true iff rectangles overlap (this includes overlapping edges)
 */
bool sdl_inViewport(DRect *view, DRect *pos) {
    if (pos->x + pos->w < view->x)
        return false;
    if (pos->x > view->x + view->w)
        return false;
    if (pos->y + pos->h < view->y)
        return false;
    if (pos->y > view->y + view->h)
        return false;
    return true;
}
