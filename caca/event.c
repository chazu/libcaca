/*
 *  libcaca       ASCII-Art library
 *  Copyright (c) 2002-2006 Sam Hocevar <sam@zoy.org>
 *                All Rights Reserved
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the Do What The Fuck You Want To
 *  Public License, Version 2, as published by Sam Hocevar. See
 *  http://sam.zoy.org/wtfpl/COPYING for more details.
 */

/** \file event.c
 *  \version \$Id$
 *  \author Sam Hocevar <sam@zoy.org>
 *  \brief Event handling
 *
 *  This file contains event handling functions for keyboard and mouse input.
 */

#include "config.h"

#if defined(USE_SLANG)
#   if defined(HAVE_SLANG_SLANG_H)
#       include <slang/slang.h>
#   else
#       include <slang.h>
#   endif
#endif
#if defined(USE_NCURSES)
#   if defined(HAVE_NCURSES_H)
#       include <ncurses.h>
#   else
#       include <curses.h>
#   endif
#endif
#if defined(USE_CONIO)
#   include <conio.h>
#endif
#if defined(USE_X11)
#   include <X11/Xlib.h>
#   include <X11/Xutil.h>
#   include <X11/keysym.h>
#endif
#if defined(USE_WIN32)
#   include <windows.h>
#endif
#if defined(USE_GL)
#include <GL/gl.h>
#include <GL/glut.h>
#include <GL/freeglut_ext.h>
#endif

#include "cucul.h"
#include "cucul_internals.h"
#include "caca.h"
#include "caca_internals.h"

static unsigned int _get_next_event(caca_t *);
static unsigned int _lowlevel_event(caca_t *);
#if defined(USE_SLANG) || defined(USE_NCURSES) || defined(USE_CONIO)
static void _push_event(caca_t *, unsigned int);
static unsigned int _pop_event(caca_t *);
#endif

#if !defined(_DOXYGEN_SKIP_ME)
/* If no new key was pressed after AUTOREPEAT_THRESHOLD usec, assume the
 * key was released */
#define AUTOREPEAT_THRESHOLD 200000
/* Start repeating key after AUTOREPEAT_TRIGGER usec and send keypress
 * events every AUTOREPEAT_RATE usec. */
#define AUTOREPEAT_TRIGGER 300000
#define AUTOREPEAT_RATE 100000
#endif

/** \brief Get the next mouse or keyboard input event.
 *
 *  This function polls the event queue for mouse or keyboard events matching
 *  the event mask and returns the first matching event. Non-matching events
 *  are discarded. \c event_mask must have a non-zero value. This function is
 *  non-blocking and returns zero if no more events are pending in the queue.
 *  See also caca_wait_event() for a blocking version of this function.
 *
 * \param event_mask Bitmask of requested events.
 * \return The next matching event in the queue, or 0 if no event is pending.
 */
unsigned int caca_get_event(caca_t *kk, unsigned int event_mask)
{
    if(!event_mask)
        return CACA_EVENT_NONE;

    for( ; ; )
    {
        unsigned int event = _get_next_event(kk);

        if(!event || event & event_mask)
            return event;
    }
}

/** \brief Wait for the next mouse or keyboard input event.
 *
 *  This function returns the first mouse or keyboard event in the queue
 *  that matches the event mask. If no event is pending, it blocks until a
 *  matching event is received. \c event_mask must have a non-zero value.
 *  See also caca_get_event() for a non-blocking version of this function.
 *
 *  \param event_mask Bitmask of requested events.
 *  \return The next event in the queue.
 */
unsigned int caca_wait_event(caca_t *kk, unsigned int event_mask)
{
    if(!event_mask)
        return CACA_EVENT_NONE;

    for( ; ; )
    {
        unsigned int event = _get_next_event(kk);

        if(event & event_mask)
            return event;

        _caca_sleep(10000);
    }
}

/** \brief Return the X mouse coordinate.
 *
 *  This function returns the X coordinate of the mouse position last time
 *  it was detected. This function is not reliable if the ncurses or S-Lang
 *  drivers are being used, because mouse position is only detected when
 *  the mouse is clicked. Other drivers such as X11 work well.
 *
 *  \return The X mouse coordinate.
 */
unsigned int caca_get_mouse_x(caca_t *kk)
{
    if(kk->mouse_x >= kk->qq->width)
        kk->mouse_x = kk->qq->width - 1;

    return kk->mouse_x;
}

/** \brief Return the Y mouse coordinate.
 *
 *  This function returns the Y coordinate of the mouse position last time
 *  it was detected. This function is not reliable if the ncurses or S-Lang
 *  drivers are being used, because mouse position is only detected when
 *  the mouse is clicked. Other drivers such as X11 work well.
 *
 *  \return The Y mouse coordinate.
 */
unsigned int caca_get_mouse_y(caca_t *kk)
{
    if(kk->mouse_y >= kk->qq->height)
        kk->mouse_y = kk->qq->height - 1;

    return kk->mouse_y;
}

/*
 * XXX: The following functions are local.
 */

static unsigned int _get_next_event(caca_t *kk)
{
#if defined(USE_SLANG) || defined(USE_NCURSES)
    unsigned int ticks;
#endif
    unsigned int event;

    event = _lowlevel_event(kk);

#if defined(USE_SLANG)
    if(kk->driver.driver != CACA_DRIVER_SLANG)
#endif
#if defined(USE_NCURSES)
    if(kk->driver.driver != CACA_DRIVER_NCURSES)
#endif
    return event;

#if defined(USE_SLANG) || defined(USE_NCURSES)
    /* Simulate long keypresses using autorepeat features */
    ticks = _caca_getticks(&kk->events.key_timer);
    kk->events.last_key_ticks += ticks;
    kk->events.autorepeat_ticks += ticks;

    /* Handle autorepeat */
    if(kk->events.last_key
           && kk->events.autorepeat_ticks > AUTOREPEAT_TRIGGER
           && kk->events.autorepeat_ticks > AUTOREPEAT_THRESHOLD
           && kk->events.autorepeat_ticks > AUTOREPEAT_RATE)
    {
        _push_event(kk, event);
        kk->events.autorepeat_ticks -= AUTOREPEAT_RATE;
        return CACA_EVENT_KEY_PRESS | kk->events.last_key;
    }

    /* We are in autorepeat mode and the same key was just pressed, ignore
     * this event and return the next one by calling ourselves. */
    if(event == (CACA_EVENT_KEY_PRESS | kk->events.last_key))
    {
        kk->events.last_key_ticks = 0;
        return _get_next_event(kk);
    }

    /* We are in autorepeat mode, but key has expired or a new key was
     * pressed - store our event and return a key release event first */
    if(kk->events.last_key
          && (kk->events.last_key_ticks > AUTOREPEAT_THRESHOLD
               || (event & CACA_EVENT_KEY_PRESS)))
    {
        _push_event(kk, event);
        event = CACA_EVENT_KEY_RELEASE | kk->events.last_key;
        kk->events.last_key = 0;
        return event;
    }

    /* A new key was pressed, enter autorepeat mode */
    if(event & CACA_EVENT_KEY_PRESS)
    {
        kk->events.last_key_ticks = 0;
        kk->events.autorepeat_ticks = 0;
        kk->events.last_key = event & 0x00ffffff;
    }

    return event;
#endif
}

static unsigned int _lowlevel_event(caca_t *kk)
{
    unsigned int event;

#if defined(USE_SLANG) || defined(USE_NCURSES) || defined(USE_CONIO)
    event = _pop_event(kk);

    if(event)
        return event;
#endif

#if defined(USE_X11)
    /* The X11 event check routine */
    if(kk->driver.driver == CACA_DRIVER_X11)
    {
        XEvent xevent;
        char key;

        while(XCheckWindowEvent(kk->x11.dpy, kk->x11.window, kk->x11.event_mask, &xevent)
               == True)
        {
            KeySym keysym;

            /* Expose event */
            if(xevent.type == Expose)
            {
                XCopyArea(kk->x11.dpy, kk->x11.pixmap,
                          kk->x11.window, kk->x11.gc, 0, 0,
                          kk->qq->width * kk->x11.font_width,
                          kk->qq->height * kk->x11.font_height, 0, 0);
                continue;
            }

            /* Resize event */
            if(xevent.type == ConfigureNotify)
            {
                unsigned int w, h;

                w = (xevent.xconfigure.width + kk->x11.font_width / 3)
                      / kk->x11.font_width;
                h = (xevent.xconfigure.height + kk->x11.font_height / 3)
                      / kk->x11.font_height;

                if(!w || !h || (w == kk->qq->width && h == kk->qq->height))
                    continue;

                kk->x11.new_width = w;
                kk->x11.new_height = h;

                /* If we are already resizing, ignore the new signal */
                if(kk->resize)
                    continue;

                kk->resize = 1;

                return CACA_EVENT_RESIZE;
            }

            /* Check for mouse motion events */
            if(xevent.type == MotionNotify)
            {
                unsigned int newx = xevent.xmotion.x / kk->x11.font_width;
                unsigned int newy = xevent.xmotion.y / kk->x11.font_height;

                if(newx >= kk->qq->width)
                    newx = kk->qq->width - 1;
                if(newy >= kk->qq->height)
                    newy = kk->qq->height - 1;

                if(kk->mouse_x == newx && kk->mouse_y == newy)
                    continue;

                kk->mouse_x = newx;
                kk->mouse_y = newy;

                return CACA_EVENT_MOUSE_MOTION | (kk->mouse_x << 12) | kk->mouse_y;
            }

            /* Check for mouse press and release events */
            if(xevent.type == ButtonPress)
                return CACA_EVENT_MOUSE_PRESS
                        | ((XButtonEvent *)&xevent)->button;

            if(xevent.type == ButtonRelease)
                return CACA_EVENT_MOUSE_RELEASE
                        | ((XButtonEvent *)&xevent)->button;

            /* Check for key press and release events */
            if(xevent.type == KeyPress)
                event |= CACA_EVENT_KEY_PRESS;
            else if(xevent.type == KeyRelease)
                event |= CACA_EVENT_KEY_RELEASE;
            else
                continue;

            if(XLookupString(&xevent.xkey, &key, 1, NULL, NULL))
                return event | key;

            keysym = XKeycodeToKeysym(kk->x11.dpy, xevent.xkey.keycode, 0);
            switch(keysym)
            {
            case XK_F1:    return event | CACA_KEY_F1;
            case XK_F2:    return event | CACA_KEY_F2;
            case XK_F3:    return event | CACA_KEY_F3;
            case XK_F4:    return event | CACA_KEY_F4;
            case XK_F5:    return event | CACA_KEY_F5;
            case XK_F6:    return event | CACA_KEY_F6;
            case XK_F7:    return event | CACA_KEY_F7;
            case XK_F8:    return event | CACA_KEY_F8;
            case XK_F9:    return event | CACA_KEY_F9;
            case XK_F10:   return event | CACA_KEY_F10;
            case XK_F11:   return event | CACA_KEY_F11;
            case XK_F12:   return event | CACA_KEY_F12;
            case XK_F13:   return event | CACA_KEY_F13;
            case XK_F14:   return event | CACA_KEY_F14;
            case XK_F15:   return event | CACA_KEY_F15;
            case XK_Left:  return event | CACA_KEY_LEFT;
            case XK_Right: return event | CACA_KEY_RIGHT;
            case XK_Up:    return event | CACA_KEY_UP;
            case XK_Down:  return event | CACA_KEY_DOWN;
            default:       return CACA_EVENT_NONE;
            }
        }

        return CACA_EVENT_NONE;
    }
    else
#endif
#if defined(USE_NCURSES)
    if(kk->driver.driver == CACA_DRIVER_NCURSES)
    {
        int intkey;

        if(kk->resize_event)
        {
            kk->resize_event = 0;
            kk->resize = 1;
            return CACA_EVENT_RESIZE;
        }

        intkey = getch();
        if(intkey == ERR)
            return CACA_EVENT_NONE;

        if(intkey < 0x100)
        {
            return CACA_EVENT_KEY_PRESS | intkey;
        }

        if(intkey == KEY_MOUSE)
        {
            MEVENT mevent;
            getmouse(&mevent);

            switch(mevent.bstate)
            {
                case BUTTON1_PRESSED:
                    _push_event(kk, CACA_EVENT_MOUSE_PRESS | 1);
                    break;
                case BUTTON1_RELEASED:
                    _push_event(kk, CACA_EVENT_MOUSE_RELEASE | 1);
                    break;
                case BUTTON1_CLICKED:
                    _push_event(kk, CACA_EVENT_MOUSE_PRESS | 1);
                    _push_event(kk, CACA_EVENT_MOUSE_RELEASE | 1);
                    break;
                case BUTTON1_DOUBLE_CLICKED:
                    _push_event(kk, CACA_EVENT_MOUSE_PRESS | 1);
                    _push_event(kk, CACA_EVENT_MOUSE_RELEASE | 1);
                    _push_event(kk, CACA_EVENT_MOUSE_PRESS | 1);
                    _push_event(kk, CACA_EVENT_MOUSE_RELEASE | 1);
                    break;
                case BUTTON1_TRIPLE_CLICKED:
                    _push_event(kk, CACA_EVENT_MOUSE_PRESS | 1);
                    _push_event(kk, CACA_EVENT_MOUSE_RELEASE | 1);
                    _push_event(kk, CACA_EVENT_MOUSE_PRESS | 1);
                    _push_event(kk, CACA_EVENT_MOUSE_RELEASE | 1);
                    _push_event(kk, CACA_EVENT_MOUSE_PRESS | 1);
                    _push_event(kk, CACA_EVENT_MOUSE_RELEASE | 1);
                    break;
                case BUTTON1_RESERVED_EVENT:
                    break;

                case BUTTON2_PRESSED:
                    _push_event(kk, CACA_EVENT_MOUSE_PRESS | 2);
                    break;
                case BUTTON2_RELEASED:
                    _push_event(kk, CACA_EVENT_MOUSE_RELEASE | 2);
                    break;
                case BUTTON2_CLICKED:
                    _push_event(kk, CACA_EVENT_MOUSE_PRESS | 2);
                    _push_event(kk, CACA_EVENT_MOUSE_RELEASE | 2);
                    break;
                case BUTTON2_DOUBLE_CLICKED:
                    _push_event(kk, CACA_EVENT_MOUSE_PRESS | 2);
                    _push_event(kk, CACA_EVENT_MOUSE_RELEASE | 2);
                    _push_event(kk, CACA_EVENT_MOUSE_PRESS | 2);
                    _push_event(kk, CACA_EVENT_MOUSE_RELEASE | 2);
                    break;
                case BUTTON2_TRIPLE_CLICKED:
                    _push_event(kk, CACA_EVENT_MOUSE_PRESS | 2);
                    _push_event(kk, CACA_EVENT_MOUSE_RELEASE | 2);
                    _push_event(kk, CACA_EVENT_MOUSE_PRESS | 2);
                    _push_event(kk, CACA_EVENT_MOUSE_RELEASE | 2);
                    _push_event(kk, CACA_EVENT_MOUSE_PRESS | 2);
                    _push_event(kk, CACA_EVENT_MOUSE_RELEASE | 2);
                    break;
                case BUTTON2_RESERVED_EVENT:
                    break;

                case BUTTON3_PRESSED:
                    _push_event(kk, CACA_EVENT_MOUSE_PRESS | 3);
                    break;
                case BUTTON3_RELEASED:
                    _push_event(kk, CACA_EVENT_MOUSE_RELEASE | 3);
                    break;
                case BUTTON3_CLICKED:
                    _push_event(kk, CACA_EVENT_MOUSE_PRESS | 3);
                    _push_event(kk, CACA_EVENT_MOUSE_RELEASE | 3);
                    break;
                case BUTTON3_DOUBLE_CLICKED:
                    _push_event(kk, CACA_EVENT_MOUSE_PRESS | 3);
                    _push_event(kk, CACA_EVENT_MOUSE_RELEASE | 3);
                    _push_event(kk, CACA_EVENT_MOUSE_PRESS | 3);
                    _push_event(kk, CACA_EVENT_MOUSE_RELEASE | 3);
                    break;
                case BUTTON3_TRIPLE_CLICKED:
                    _push_event(kk, CACA_EVENT_MOUSE_PRESS | 3);
                    _push_event(kk, CACA_EVENT_MOUSE_RELEASE | 3);
                    _push_event(kk, CACA_EVENT_MOUSE_PRESS | 3);
                    _push_event(kk, CACA_EVENT_MOUSE_RELEASE | 3);
                    _push_event(kk, CACA_EVENT_MOUSE_PRESS | 3);
                    _push_event(kk, CACA_EVENT_MOUSE_RELEASE | 3);
                    break;
                case BUTTON3_RESERVED_EVENT:
                    break;

                case BUTTON4_PRESSED:
                    _push_event(kk, CACA_EVENT_MOUSE_PRESS | 4);
                    break;
                case BUTTON4_RELEASED:
                    _push_event(kk, CACA_EVENT_MOUSE_RELEASE | 4);
                    break;
                case BUTTON4_CLICKED:
                    _push_event(kk, CACA_EVENT_MOUSE_PRESS | 4);
                    _push_event(kk, CACA_EVENT_MOUSE_RELEASE | 4);
                    break;
                case BUTTON4_DOUBLE_CLICKED:
                    _push_event(kk, CACA_EVENT_MOUSE_PRESS | 4);
                    _push_event(kk, CACA_EVENT_MOUSE_RELEASE | 4);
                    _push_event(kk, CACA_EVENT_MOUSE_PRESS | 4);
                    _push_event(kk, CACA_EVENT_MOUSE_RELEASE | 4);
                    break;
                case BUTTON4_TRIPLE_CLICKED:
                    _push_event(kk, CACA_EVENT_MOUSE_PRESS | 4);
                    _push_event(kk, CACA_EVENT_MOUSE_RELEASE | 4);
                    _push_event(kk, CACA_EVENT_MOUSE_PRESS | 4);
                    _push_event(kk, CACA_EVENT_MOUSE_RELEASE | 4);
                    _push_event(kk, CACA_EVENT_MOUSE_PRESS | 4);
                    _push_event(kk, CACA_EVENT_MOUSE_RELEASE | 4);
                    break;
                case BUTTON4_RESERVED_EVENT:
                    break;

                default:
                    break;
            }

            if(kk->mouse_x == (unsigned int)mevent.x &&
               kk->mouse_y == (unsigned int)mevent.y)
                return _pop_event(kk);

            kk->mouse_x = mevent.x;
            kk->mouse_y = mevent.y;

            return CACA_EVENT_MOUSE_MOTION | (kk->mouse_x << 12) | kk->mouse_y;
        }

        event = CACA_EVENT_KEY_PRESS;

        switch(intkey)
        {
            case KEY_UP: return event | CACA_KEY_UP;
            case KEY_DOWN: return event | CACA_KEY_DOWN;
            case KEY_LEFT: return event | CACA_KEY_LEFT;
            case KEY_RIGHT: return event | CACA_KEY_RIGHT;

            case KEY_IC: return event | CACA_KEY_INSERT;
            case KEY_DC: return event | CACA_KEY_DELETE;
            case KEY_HOME: return event | CACA_KEY_HOME;
            case KEY_END: return event | CACA_KEY_END;
            case KEY_PPAGE: return event | CACA_KEY_PAGEUP;
            case KEY_NPAGE: return event | CACA_KEY_PAGEDOWN;

            case KEY_F(1): return event | CACA_KEY_F1;
            case KEY_F(2): return event | CACA_KEY_F2;
            case KEY_F(3): return event | CACA_KEY_F3;
            case KEY_F(4): return event | CACA_KEY_F4;
            case KEY_F(5): return event | CACA_KEY_F5;
            case KEY_F(6): return event | CACA_KEY_F6;
            case KEY_F(7): return event | CACA_KEY_F7;
            case KEY_F(8): return event | CACA_KEY_F8;
            case KEY_F(9): return event | CACA_KEY_F9;
            case KEY_F(10): return event | CACA_KEY_F10;
            case KEY_F(11): return event | CACA_KEY_F11;
            case KEY_F(12): return event | CACA_KEY_F12;
        }

        return CACA_EVENT_NONE;
    }
    else
#endif
#if defined(USE_SLANG)
    if(kk->driver.driver == CACA_DRIVER_SLANG)
    {
        int intkey;

        if(kk->resize_event)
        {
            kk->resize_event = 0;
            kk->resize = 1;
            return CACA_EVENT_RESIZE;
        }

        if(!SLang_input_pending(0))
            return CACA_EVENT_NONE;

        /* We first use SLang_getkey() to see whether Esc was pressed
         * alone, then (if it wasn't) we unget the key and use SLkp_getkey()
         * instead, so that escape sequences are interpreted. */
        intkey = SLang_getkey();

        if(intkey != 0x1b /* Esc */ || SLang_input_pending(0))
        {
            SLang_ungetkey(intkey);
            intkey = SLkp_getkey();
        }

        /* If the key was ASCII, return it immediately */
        if(intkey < 0x100)
        {
            return CACA_EVENT_KEY_PRESS | intkey;
        }

        if(intkey == 0x3e9)
        {
            int button = (SLang_getkey() - ' ' + 1) & 0xf;
            unsigned int x = SLang_getkey() - '!';
            unsigned int y = SLang_getkey() - '!';
            _push_event(kk, CACA_EVENT_MOUSE_PRESS | button);
            _push_event(kk, CACA_EVENT_MOUSE_RELEASE | button);

            if(kk->mouse_x == x && kk->mouse_y == y)
                return _pop_event(kk);

            kk->mouse_x = x;
            kk->mouse_y = y;

            return CACA_EVENT_MOUSE_MOTION | (kk->mouse_x << 12) | kk->mouse_y;
        }

        event = CACA_EVENT_KEY_PRESS;

        switch(intkey)
        {
            case SL_KEY_UP: return event | CACA_KEY_UP;
            case SL_KEY_DOWN: return event | CACA_KEY_DOWN;
            case SL_KEY_LEFT: return event | CACA_KEY_LEFT;
            case SL_KEY_RIGHT: return event | CACA_KEY_RIGHT;

            case SL_KEY_IC: return event | CACA_KEY_INSERT;
            case SL_KEY_DELETE: return event | CACA_KEY_DELETE;
            case SL_KEY_HOME: return event | CACA_KEY_HOME;
            case SL_KEY_END: return event | CACA_KEY_END;
            case SL_KEY_PPAGE: return event | CACA_KEY_PAGEUP;
            case SL_KEY_NPAGE: return event | CACA_KEY_PAGEDOWN;

            case SL_KEY_F(1): return event | CACA_KEY_F1;
            case SL_KEY_F(2): return event | CACA_KEY_F2;
            case SL_KEY_F(3): return event | CACA_KEY_F3;
            case SL_KEY_F(4): return event | CACA_KEY_F4;
            case SL_KEY_F(5): return event | CACA_KEY_F5;
            case SL_KEY_F(6): return event | CACA_KEY_F6;
            case SL_KEY_F(7): return event | CACA_KEY_F7;
            case SL_KEY_F(8): return event | CACA_KEY_F8;
            case SL_KEY_F(9): return event | CACA_KEY_F9;
            case SL_KEY_F(10): return event | CACA_KEY_F10;
            case SL_KEY_F(11): return event | CACA_KEY_F11;
            case SL_KEY_F(12): return event | CACA_KEY_F12;
        }

        return CACA_EVENT_NONE;
    }
    else
#endif
#if defined(USE_CONIO)
    if(kk->driver.driver == CACA_DRIVER_CONIO)
    {
        if(!_conio_kbhit())
            return CACA_EVENT_NONE;

        event = getch();
        _push_event(kk, CACA_EVENT_KEY_RELEASE | event);
        return CACA_EVENT_KEY_PRESS | event;
    }
    else
#endif
#if defined(USE_WIN32)
    if(kk->driver.driver == CACA_DRIVER_WIN32)
    {
        INPUT_RECORD rec;
        DWORD num;

        for( ; ; )
        {
            GetNumberOfConsoleInputEvents(kk->win32.hin, &num);
            if(num == 0)
                break;

            ReadConsoleInput(kk->win32.hin, &rec, 1, &num);
            if(rec.EventType == KEY_EVENT)
            {
                if(rec.Event.KeyEvent.bKeyDown)
                    event = CACA_EVENT_KEY_PRESS;
                else
                    event = CACA_EVENT_KEY_RELEASE;

                if(rec.Event.KeyEvent.uChar.AsciiChar)
                    return event | rec.Event.KeyEvent.uChar.AsciiChar;
            }

            if(rec.EventType == MOUSE_EVENT)
            {
                if(rec.Event.MouseEvent.dwEventFlags == 0)
                {
                    if(rec.Event.MouseEvent.dwButtonState & 0x01)
                        return CACA_EVENT_MOUSE_PRESS | 0x000001;

                    if(rec.Event.MouseEvent.dwButtonState & 0x02)
                        return CACA_EVENT_MOUSE_PRESS | 0x000002;
                }
                else if(rec.Event.MouseEvent.dwEventFlags == MOUSE_MOVED)
                {
                    COORD pos = rec.Event.MouseEvent.dwMousePosition;

                    if(kk->mouse_x == (unsigned int)pos.X &&
                       kk->mouse_y == (unsigned int)pos.Y)
                        continue;

                    kk->mouse_x = pos.X;
                    kk->mouse_y = pos.Y;

                    return CACA_EVENT_MOUSE_MOTION | (kk->mouse_x << 12) | kk->mouse_y;
                }
#if 0
                else if(rec.Event.MouseEvent.dwEventFlags == DOUBLE_CLICK)
                {
                    cout << rec.Event.MouseEvent.dwMousePosition.X << "," <<
                            rec.Event.MouseEvent.dwMousePosition.Y << "  " << flush;
                }
                else if(rec.Event.MouseEvent.dwEventFlags == MOUSE_WHEELED)
                {
                    SetConsoleCursorPosition(hOut,
                                             WheelWhere);
                    if(rec.Event.MouseEvent.dwButtonState & 0xFF000000)
                        cout << "Down" << flush;
                    else
                        cout << "Up  " << flush;
                }
#endif
            }

            /* Unknown event */
            return CACA_EVENT_NONE;
        }

        /* No event */
        return CACA_EVENT_NONE;
    }
    else
#endif
#if defined(USE_GL)
    if(kk->driver.driver == CACA_DRIVER_GL)
    {
        glutMainLoopEvent();

        if(kk->gl.resized && !kk->resize)
        {
            kk->resize = 1;
            kk->gl.resized = 0;
            return CACA_EVENT_RESIZE;
        }

        if(kk->gl.mouse_changed)
        {
            if(kk->gl.mouse_clicked)
            {
                event|= CACA_EVENT_MOUSE_PRESS | kk->gl.mouse_button;
                kk->gl.mouse_clicked=0;
            }
            kk->mouse_x = kk->gl.mouse_x;
            kk->mouse_y = kk->gl.mouse_y;
            event |= CACA_EVENT_MOUSE_MOTION | (kk->mouse_x << 12) | kk->mouse_y;
            kk->gl.mouse_changed = 0;
        }

        if(kk->gl.key != 0)
        {
            event |= CACA_EVENT_KEY_PRESS;
            event |= kk->gl.key;
            kk->gl.key = 0;
	    return event;
        }

        if(kk->gl.special_key != 0)
        {
            event |= CACA_EVENT_KEY_PRESS;
     
            switch(kk->gl.special_key)
            {
                case GLUT_KEY_F1 : kk->gl.special_key = 0; return event | CACA_KEY_F1;
                case GLUT_KEY_F2 : kk->gl.special_key = 0; return event | CACA_KEY_F2;
                case GLUT_KEY_F3 : kk->gl.special_key = 0; return event | CACA_KEY_F3;
                case GLUT_KEY_F4 : kk->gl.special_key = 0; return event | CACA_KEY_F4;
                case GLUT_KEY_F5 : kk->gl.special_key = 0; return event | CACA_KEY_F5;
                case GLUT_KEY_F6 : kk->gl.special_key = 0; return event | CACA_KEY_F6;
                case GLUT_KEY_F7 : kk->gl.special_key = 0; return event | CACA_KEY_F7;
                case GLUT_KEY_F8 : kk->gl.special_key = 0; return event | CACA_KEY_F8;
                case GLUT_KEY_F9 : kk->gl.special_key = 0; return event | CACA_KEY_F9;
                case GLUT_KEY_F10: kk->gl.special_key = 0; return event | CACA_KEY_F10;
                case GLUT_KEY_F11: kk->gl.special_key = 0; return event | CACA_KEY_F11;
                case GLUT_KEY_F12: kk->gl.special_key = 0; return event | CACA_KEY_F12;
                case GLUT_KEY_LEFT : kk->gl.special_key = 0; return event | CACA_KEY_LEFT;
                case GLUT_KEY_RIGHT: kk->gl.special_key = 0; return event | CACA_KEY_RIGHT;
                case GLUT_KEY_UP   : kk->gl.special_key = 0; return event | CACA_KEY_UP;
                case GLUT_KEY_DOWN : kk->gl.special_key = 0; return event | CACA_KEY_DOWN;
                default: return CACA_EVENT_NONE;
            }
        }
        return CACA_EVENT_NONE;
    }
    else
#endif
    {
        /* Dummy */
    }

    return CACA_EVENT_NONE;
}

#if defined(USE_SLANG) || defined(USE_NCURSES) || defined(USE_CONIO)
static void _push_event(caca_t *kk, unsigned int event)
{
    if(!event || kk->events.queue == EVENTBUF_LEN)
        return;
    kk->events.buf[kk->events.queue] = event;
    kk->events.queue++;
}

static unsigned int _pop_event(caca_t *kk)
{
    int i;
    unsigned int event;

    if(kk->events.queue == 0)
        return CACA_EVENT_NONE;

    event = kk->events.buf[0];
    for(i = 1; i < kk->events.queue; i++)
        kk->events.buf[i - 1] = kk->events.buf[i];
    kk->events.queue--;

    return event;
}
#endif
