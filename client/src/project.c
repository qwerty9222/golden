#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "project.h"
#include "utils.h"

int create_project(const char *project_name) {
    printf("Creating project '%s'...\n", project_name);

    if (mkdir(project_name, 0755) != 0) {
        perror("Error creating project directory");
        return EXIT_FAILURE;
    }

    char path[256];
    snprintf(path, sizeof(path), "%s/src", project_name);
    if (mkdir(path, 0755) != 0) {
        perror("Error creating src directory");
        return EXIT_FAILURE;
    }

    snprintf(path, sizeof(path), "%s/src/main.gsf", project_name);
    FILE *main_file = fopen(path, "w");
    if (!main_file) {
        perror("Error creating main.gsf");
        return EXIT_FAILURE;
    }
    fprintf(main_file, "// Project: %s\n", project_name);
    fprintf(main_file, "#include <stdio.h>\n\n");
    fprintf(main_file, "fn main() {\n");
    fprintf(main_file, "    println(\"Hello from %s!\");\n", project_name);
    fprintf(main_file, "}\n");
    fclose(main_file);

    snprintf(path, sizeof(path), "%s/project.conf", project_name);
    FILE *config_file = fopen(path, "w");
    if (!config_file) {
        perror("Error creating project.conf");
        return EXIT_FAILURE;
    }
    fprintf(config_file, "[project]\n");
    fprintf(config_file, "name = %s\n", project_name);
    fprintf(config_file, "version = 1.0.0\n");
    fprintf(config_file, "\n[graphics]\n");
    fprintf(config_file, "window_title = %s\n", project_name);
    fprintf(config_file, "window_width = 800\n");
    fprintf(config_file, "window_height = 600\n");
    fprintf(config_file, "window_resizable = no\n");
    fprintf(config_file, "window_mode = windowed\n");
    fprintf(config_file, "renderer = opengl\n");
    fprintf(config_file, "fps = 60\n");
    fclose(config_file);

    printf("✓ Project '%s' created successfully\n", project_name);
    printf("  Main file: %s/src/main.gsf\n", project_name);
    printf("  Configuration: %s/project.conf\n", project_name);
    return EXIT_SUCCESS;
}
