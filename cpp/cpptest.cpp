/*
 *  cpptest       libcaca++ rendering test
 *  Copyright (c) 2006 Jean-Yves Lamoureux <jylam@lnxscene.org>
 *                All Rights Reserved
 *
 *  $Id$
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the Do What The Fuck You Want To
 *  Public License, Version 2, as published by Sam Hocevar. See
 *  http://sam.zoy.org/wtfpl/COPYING for more details.
 */

#include <iostream>

#include <cucul++.h>
#include <caca++.h>

using namespace std;


static char const *pig[]= {
    "                             _    ",
    "    _._ _..._ .-',     _.._(`))   ",
    "   '-. `     '  /-._.-'    ',/    ",
    "      )         \            '.   ",
    "     / _    _    |             \\  ",
    "    |  a    a    /              | ",
    "    \   .-.                     ; " ,
    "     '-('' ).-'       ,'       ;  ",
    "        '-;           |      .'   ",
    "           \\           \\    /     ",
    "           | 7  .__  _.-\\   \\     ",
    "           | |  |  ``/  /`  /     ",
    "      jgs /,_|  |   /,_/   /      ",
    "             /,_/      '`-'       ",
 NULL
};

int main(int argc, char *argv[])
{
    Cucul *qq;
    Caca  *kk;
    Caca::Event ev;

    try {
        qq = new Cucul();
    } 
    catch (int e) {
        cerr << "Error while initializing cucul (" << e << ")" << endl;
        return -1;
    }
    
    try {
        kk = new Caca(qq);
    }
    catch(int e) {
        cerr << "Error while attaching cucul to caca (" << e << ")" << endl;
        return -1;
    }

    
    /* Draw pig */
    qq->set_color(CUCUL_COLOR_LIGHTMAGENTA, CUCUL_COLOR_BLACK);
   
    for(int i = 0; pig[i]; i++)
        qq->putstr(0, i, (char*)pig[i]);
    
    /* printf works */
    qq->set_color(CUCUL_COLOR_LIGHTBLUE, CUCUL_COLOR_BLACK);  
    qq->printf(7,15, "Powered by libcaca %s", VERSION);
    

    kk->display();
    kk->get_event(CACA_EVENT_KEY_PRESS, &ev, -1);


    delete kk;
    delete qq;

    return 0;
}
