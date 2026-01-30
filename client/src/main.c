#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "project.h"
#include "compiler.h"
#include "utils.h"

void print_usage(const char *program_name) {
    printf("Usage: %s <command> [arguments]\n", program_name);
    printf("\nAvailable commands:\n");
    printf("  new <name>          Create a new project\n");
    printf("  build <directory>   Compile project to bytecode\n");
    printf("  clean               Clean compiled files\n");
    printf("  --version           Show version\n");
    printf("  --help              Show this help\n");
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    const char *command = argv[1];

    if (strcmp(command, "--help") == 0 || strcmp(command, "-h") == 0) {
        print_usage(argv[0]);
        return EXIT_SUCCESS;
    }

    if (strcmp(command, "--version") == 0 || strcmp(command, "-v") == 0) {
        printf("BLAS Compiler v1.0.0\n");
        return EXIT_SUCCESS;
    }

    if (strcmp(command, "new") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Error: Project name is required\n");
            fprintf(stderr, "Usage: %s new <name>\n", argv[0]);
            return EXIT_FAILURE;
        }
        return create_project(argv[2]);
    }

    if (strcmp(command, "build") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Error: Project directory is required\n");
            fprintf(stderr, "Usage: %s build <directory>\n", argv[0]);
            return EXIT_FAILURE;
        }
        return build_project(argv[2]);
    }

    if (strcmp(command, "clean") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Error: Project directory is required\n");
            fprintf(stderr, "Usage: %s clean <directory>\n", argv[0]);
            return EXIT_FAILURE;
        }
        return clean_build_artifacts(argv[2]);
    }

    fprintf(stderr, "Error: Unknown command '%s'\n", command);
    print_usage(argv[0]);
    return EXIT_FAILURE;
}
