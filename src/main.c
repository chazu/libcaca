/*
 *   ttyvaders     Textmode shoot'em up
 *   Copyright (c) 2002 Sam Hocevar <sam@zoy.org>
 *                 All Rights Reserved
 *
 *   $Id tarass
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <stdio.h>
#include <stdlib.h>

#include <string.h>
#include <unistd.h>

#include "common.h"

static void start_game (game *);

int main (int argc, char **argv)
{
    game *g = malloc(sizeof(game));

    //srand(time(NULL));

    if( init_graphics() )
    {
        return 1;
    }

    /* Initialize our program */
    init_game(g);

    /* Go ! */
    start_game(g);

    /* Clean up */
    end_graphics();

    return 0;
}

static void start_game (game *g)
{
    int i;
    int quit = 0;
    int poz = 0;
    int skip = 0;
    int purcompteur = 0;

    g->sf = malloc(sizeof(starfield));
    g->wp = malloc(sizeof(weapons));
    g->ex = malloc(sizeof(explosions));
    g->bo = malloc(sizeof(bonus));
    g->t = create_tunnel( g, g->w, g->h );
    g->p = create_player( g );
    g->al = malloc(sizeof(aliens));

    init_starfield( g, g->sf );
    init_weapons( g, g->wp );
    init_explosions( g, g->ex );
    init_aliens( g, g->al );

    /* Temporary stuff */
    for( i = 0; i < 5; i++ )
    {
        add_alien( g, g->al, rand() % g->w, rand() % g->h / 2, ALIEN_POOLP );
    }

    g->t->w = 25;

    while( !quit )
    {
        char key;

        while( ( key = get_key() ) )
        {
            switch( key )
            {
                case 'q':
                    quit = 1;
                    break;
                case 'p':
                    poz = !poz;
                    break;
                case '\t':
                    ceo_alert();
                    poz = 1;
                    break;
                case 's':
                    skip = 1;
                    break;
                case 'h':
                    g->p->dir = -3;
                    break;
                case 'j':
                    if( g->p->y < g->h - 2 ) g->p->y += 1;
                    break;
                case 'k':
                    if( g->p->y > 1 ) g->p->y -= 1;
                    break;
                case 'l':
                    g->p->dir = 3;
                    break;
                case 'n':
                    if( g->p->nuke == 0 )
                    {
                        g->p->nuke = 40;
                        add_weapon( g, g->wp, (g->p->x + 2) << 4, g->p->y << 4, 0, 0, WEAPON_NUKE );
                    }
                    break;
                case '\r':
                    if( g->p->nuke == 0 )
                    {
                        g->p->nuke = 40;
                        add_weapon( g, g->wp, (g->p->x + 2) << 4, g->p->y << 4, 0, 0, WEAPON_BEAM );
                    }
                    break;
                case 'b':
                    if( g->p->weapon == 0 )
                    {
                        g->p->weapon = 4;
                        add_weapon( g, g->wp, (g->p->x + 2) << 4, g->p->y << 4, 0, -16, WEAPON_BOMB );
                    }
                case ' ':
                    if( g->p->weapon == 0 )
                    {
                        g->p->weapon = 4;
                        add_weapon( g, g->wp, g->p->x << 4, g->p->y << 4, 0, -16, WEAPON_LASER );
                        add_weapon( g, g->wp, (g->p->x + 5) << 4, g->p->y << 4, 0, -16, WEAPON_LASER );
                        /* Extra shtuph */
                        add_weapon( g, g->wp, g->p->x << 4, g->p->y << 4, -24, -16, WEAPON_SEEKER );
                        add_weapon( g, g->wp, (g->p->x + 5) << 4, g->p->y << 4, 24, -16, WEAPON_SEEKER );
                        /* More shtuph */
                        add_weapon( g, g->wp, (g->p->x + 1) << 4, (g->p->y - 1) << 4, 0, -16, WEAPON_LASER );
                        add_weapon( g, g->wp, (g->p->x + 4) << 4, (g->p->y - 1) << 4, 0, -16, WEAPON_LASER );
                        /* Even more shtuph */
                        add_weapon( g, g->wp, (g->p->x + 2) << 4, (g->p->y - 1) << 4, 0, -16, WEAPON_LASER );
                        add_weapon( g, g->wp, (g->p->x + 3) << 4, (g->p->y - 1) << 4, 0, -16, WEAPON_LASER );
                        /* Extra shtuph */
                        add_weapon( g, g->wp, g->p->x << 4, g->p->y << 4, -32, 0, WEAPON_SEEKER );
                        add_weapon( g, g->wp, (g->p->x + 5) << 4, g->p->y << 4, 32, 0, WEAPON_SEEKER );
                    }
                    break;
            }
        }

        usleep(40000);

        if( !poz || skip )
        {
            skip = 0;

            /* XXX: to be removed */
            if( GET_RAND(0,10) == 0 )
            {
                int list[3] = { ALIEN_POOLP, ALIEN_BOOL, ALIEN_BRAH };

                add_alien( g, g->al, 0, rand() % g->h / 2, list[GET_RAND(0,3)] );
            }

            /* Update game rules */
            if( g->t->right[1] - g->t->left[1] == g->t->w )
            {
                g->t->w = 85 - g->t->w;
            }

            /* Scroll and update positions */
            collide_player_tunnel( g, g->p, g->t, g->ex );
            update_player( g, g->p );
            collide_player_tunnel( g, g->p, g->t, g->ex );

            update_starfield( g, g->sf );
            update_bonus( g, g->bo );
            update_aliens( g, g->al );

            collide_weapons_tunnel( g, g->wp, g->t, g->ex );
            collide_weapons_aliens( g, g->wp, g->al, g->ex );
            update_weapons( g, g->wp );
            collide_weapons_tunnel( g, g->wp, g->t, g->ex );
            collide_weapons_aliens( g, g->wp, g->al, g->ex );

            update_explosions( g, g->ex );
            /*if(purcompteur%2)*/ update_tunnel( g, g->t );
        }

        /* Clear screen */
        clear_graphics();

        /* Print starfield, tunnel, aliens, player and explosions */
        draw_starfield( g, g->sf );
        draw_tunnel( g, g->t );
        draw_bonus( g, g->bo );
        draw_aliens( g, g->al );
        draw_player( g, g->p );
        draw_explosions( g, g->ex );
        draw_weapons( g, g->wp );

        /* Refresh */
        refresh_graphics();

        purcompteur++;
    }

#if 0
    free_player( p );
    free_tunnel( g->t );
#endif
}

