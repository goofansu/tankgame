/*
 * Tank Game - Main Entry Point
 */

#include <stdio.h>
#include <stdlib.h>

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    printf("Tank Game - Starting...\n");

#ifdef PZ_DEBUG
    printf("Build: Debug\n");
#elif defined(PZ_DEV)
    printf("Build: Dev\n");
#elif defined(PZ_RELEASE)
    printf("Build: Release\n");
#endif

    // TODO: Initialize engine (M0.2+)
    // TODO: Run game loop
    // TODO: Shutdown

    printf("Tank Game - Exiting.\n");
    return EXIT_SUCCESS;
}
