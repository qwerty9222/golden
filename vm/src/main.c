#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "vm.h"

void print_usage(const char *program_name) {
    printf("Usage: %s <command> [arguments]\n", program_name);
    printf("\nAvailable commands:\n");
    printf("  run <file.gld>          Run bytecode\n");
    printf("\nOptions:\n");
    printf("  --debug                 Run with debug information\n");
    printf("  --renderer <type>       Specify renderer (opengl, none)\n");
    printf("  --version               Show version\n");
    printf("  --help                  Show this help\n");
    printf("\nSupported renderers:\n");
    printf("  opengl                  OpenGL (default renderer)\n");
    printf("  none                    Console (no graphics)\n");
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    const char *command = argv[1];
    int debug = 0;
    const char *override_renderer = NULL;

    // Find --debug and --renderer flags
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--debug") == 0) {
            debug = 1;
        } else if (strcmp(argv[i], "--renderer") == 0 && i + 1 < argc) {
            override_renderer = argv[i + 1];
            i++;  // Skip next argument
        }
    }

    if (strcmp(command, "--help") == 0 || strcmp(command, "-h") == 0) {
        print_usage(argv[0]);
        return EXIT_SUCCESS;
    }

    if (strcmp(command, "--version") == 0 || strcmp(command, "-v") == 0) {
        printf("Golden VM version 1.0.0\n");
        return EXIT_SUCCESS;
    }

    if (strcmp(command, "run") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Error: Bytecode file is required\n");
            fprintf(stderr, "Usage: %s run <file.gld> [--debug] [--renderer <type>]\n", argv[0]);
            return EXIT_FAILURE;
        }
        return execute_bytecode(argv[2], debug, override_renderer);
    }

    fprintf(stderr, "Error: Unknown command '%s'\n", command);
    print_usage(argv[0]);
    return EXIT_FAILURE;
}
