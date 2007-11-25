/*
 *  cacaplay      caca file player
 *  Copyright (c) 2006 Sam Hocevar <sam@zoy.org>
 *                All Rights Reserved
 *
 *  $Id$
 *
 *  This program is free software. It comes without any warranty, to
 *  the extent permitted by applicable law. You can redistribute it
 *  and/or modify it under the terms of the Do What The Fuck You Want
 *  To Public License, Version 2, as published by Sam Hocevar. See
 *  http://sam.zoy.org/wtfpl/COPYING for more details.
 */

#include "config.h"
#include "common.h"

#if !defined(__KERNEL__)
#   include <stdio.h>
#   include <stdlib.h>
#   include <sys/types.h>
#   include <sys/stat.h>
#   include <fcntl.h>
#   include <string.h>
#   include <unistd.h>
#endif

#include "cucul.h"
#include "caca.h"

int main(int argc, char **argv)
{
    cucul_canvas_t *cv, *app;
    caca_display_t *dp;
    unsigned char *buf = NULL;
    long int bytes = 0, total = 0;
    int fd;

    if(argc < 2 || !strcmp(argv[1], "-"))
    {
        fd = 0; /* use stdin */
    }
    else
    {
        fd = open(argv[1], O_RDONLY);
        if(fd < 0)
        {
            fprintf(stderr, "%s: could not open `%s'.\n", argv[0], argv[1]);
            return 1;
        }
    }

    cv = cucul_create_canvas(0, 0);
    app = cucul_create_canvas(0, 0);
    if(cv == NULL || app == NULL)
    {
        printf("Can't created canvas\n");
        return -1;
    }
    dp = caca_create_display(cv);
    if(dp == NULL)
    {
        printf("Can't create display\n");
        return -1;
    }


    for(;;)
    {
        caca_event_t ev;
        int ret = caca_get_event(dp, CACA_EVENT_ANY, &ev, 0);
        int eof = 0;

        if(ret && caca_get_event_type(&ev) & CACA_EVENT_KEY_PRESS)
            break;

        if(bytes == 0)
        {
            ssize_t n;
            buf = realloc(buf, total + 1);
            n = read(fd, buf + total, 1);
            if(n < 0)
            {
                fprintf(stderr, "%s: read error\n", argv[0]);
                return -1;
            }
            else if(n == 0)
            {
                eof = 1;
            }
            total += n;
        }

        bytes = cucul_import_memory(app, buf, total, "caca");

        if(bytes > 0)
        {
            total -= bytes;
            memmove(buf, buf + bytes, total);

            cucul_blit(cv, 0, 0, app, NULL);
            caca_refresh_display(dp);
        }
        else if(bytes < 0)
        {
            fprintf(stderr, "%s: corrupted caca file\n", argv[0]);
            break;
        }

        if(eof)
            break;
    }

    caca_get_event(dp, CACA_EVENT_KEY_PRESS, NULL, -1);

    /* Clean up */
    close(fd);

    caca_free_display(dp);
    cucul_free_canvas(cv);

    return 0;
}

