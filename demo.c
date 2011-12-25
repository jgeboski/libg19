/*
 * Copyright 2010-2011 James Geboski <jgeboski@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <ctype.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "libg19.h"

#define print_key(ks, k, ms, m)  if(ks & k) strcat(ms, m " ");

static unsigned char quit;

static void cmd_parse(char * cmd)
{
    int argc, i;
    char * cp;
    char * args[4];
    
    if(!cmd || (cmd[0] == 0))
        return;
    
    cp = strtok(cmd, " ");
    
    for(i = 0; (cp != NULL) && (i < 4); i++) {
        args[i] = cp;
        cp = strtok(NULL, " ");
    }
    
    argc = i;
    
    if(!strncmp(args[0], "backlight", 9)) {
        unsigned char r, g, b;
        
        if(argc < 4) {
            printf("Invalid command syntax, refer to `help`\n");
            return;
        }
        
        r = atoi(args[1]);
        g = atoi(args[2]);
        b = atoi(args[3]);
        
        g19_set_backlight(r, g, b);
    } else if(!strncmp(args[0], "clrscr", 6)) {
        unsigned char data[G19_BMP_DSIZE];
        
        memset(data, 0, G19_BMP_DSIZE);
        g19_update_lcd(data, G19_BMP_DSIZE,
            G19_PREPEND_HDATA | G19_DATA_TYPE_BMP);
    } else if(!strncmp(args[0], "mled", 4)) {
        unsigned int keys;
        
        if(argc < 2) {
            g19_set_mkey_led(0);
            return;
        }
        
        keys = 0;
        cp   = strtok(args[1], "|");
        
        while(cp != NULL) {
            if(strlen(cp) < 2) {
                cp = strtok(NULL, "|");
                continue;
            }
            
            switch(tolower(cp[1])) {
            case '1':
                keys |= G19_KEY_M1;
                break;
            
            case '2':
                keys |= G19_KEY_M2;
                break;
            
            case '3':
                keys |= G19_KEY_M3;
                break;
            
            case 'r':
                keys |= G19_KEY_MR;
                break;
            }
            
            cp = strtok(NULL, "|");
        }
        
        g19_set_mkey_led(keys);
    } else if(!strncmp(cmd, "help", 4)) {
        puts("backlight R G B    - Set the keyboard backlight color");
        puts("clrscr             - Set the LCD to black");
        puts("mled [m1|m2|m3|mr] - Set the M-Key leds on or off");
        puts("help               - Shows this menu");
        puts("quit               - Exit the application");
    } else if(!strncmp(args[0], "quit", 4)) {
        quit = 1;
    } else {
        puts("Type `help` for a list of commands");
    }
}

static void g19_gkeys(unsigned int keys)
{
    char skeys[40];
    
    memset(skeys, 0, 40);
    
    print_key(keys, G19_KEY_G1,  skeys, "G1");
    print_key(keys, G19_KEY_G2,  skeys, "G2");
    print_key(keys, G19_KEY_G3,  skeys, "G3");
    print_key(keys, G19_KEY_G4,  skeys, "G4");
    print_key(keys, G19_KEY_G5,  skeys, "G5");
    print_key(keys, G19_KEY_G6,  skeys, "G6");
    print_key(keys, G19_KEY_G7,  skeys, "G7");
    print_key(keys, G19_KEY_G8,  skeys, "G8");
    print_key(keys, G19_KEY_G9,  skeys, "G9");
    print_key(keys, G19_KEY_G10, skeys, "G10");
    print_key(keys, G19_KEY_G11, skeys, "G11");
    print_key(keys, G19_KEY_G12, skeys, "G12");
    print_key(keys, G19_KEY_M1,  skeys, "M1");
    print_key(keys, G19_KEY_M2,  skeys, "M2");
    print_key(keys, G19_KEY_M3,  skeys, "M3");
    print_key(keys, G19_KEY_MR,  skeys, "MR");
    
    printf("G-Keys: %s\n", skeys);
}

static void g19_lkeys(unsigned short keys)
{
    char skeys[50];
    
    memset(skeys, 0, 50);
    
    print_key(keys, G19_KEY_LHOME,   skeys, "HOME");
    print_key(keys, G19_KEY_LCANCEL, skeys, "CANCEL");
    print_key(keys, G19_KEY_LMENU,   skeys, "MENU");
    print_key(keys, G19_KEY_LOK,     skeys, "OK");
    print_key(keys, G19_KEY_LRIGHT,  skeys, "RIGHT");
    print_key(keys, G19_KEY_LLEFT,   skeys, "LEFT");
    print_key(keys, G19_KEY_LDOWN,   skeys, "DOWN");
    print_key(keys, G19_KEY_LUP,     skeys, "UP");
    
    printf("L-Keys: %s\n", skeys);
}

int main(int argc, char * argv[])
{
    char cmd[100];
    
    g19_init(3);
    
    g19_set_gkeys_cb(g19_gkeys);
    g19_set_lkeys_cb(g19_lkeys);
    
    while(!quit) {
        printf("> ");
        
        memset(cmd, 0, 100);
        fgets(cmd, 100, stdin);
        
        cmd_parse(cmd);
    }
    
    g19_deinit();
    
    return 0;
}
