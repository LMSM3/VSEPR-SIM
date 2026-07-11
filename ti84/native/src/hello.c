/*
 * hello.c - Minimal TI-83+/84+ test program
 *
 * Commandment 17: Test on minimal binaries first.
 * This does the absolute minimum: clear screen, print text, wait for key.
 *
 * Build: tibuild.ps1 src\hello.c HELLO
 * Test:  Send HELLO.8xp to calc -> run via Ion/MirageOS -> see "HELLO Z80"
 */

#include <stdio.h>

/* z88dk provides stdio for TI calcs — maps to bcalls */

int main(void)
{
    /* Clear the home screen */
    printf("\x1b[2J");      /* ANSI clear — z88dk maps this to ClrHome bcall */

    /* Print test message */
    printf("HELLO Z80\n");
    printf("C ON TI-84+\n");
    printf("\n");
    printf("[PRESS ANY KEY]\n");

    /* Wait for keypress */
    /* z88dk getk() polls the keyboard */
    while (!getk())
        ;

    return 0;
}
