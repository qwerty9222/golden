#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ftw.h>
#include <dirent.h>
#include <sys/stat.h>
#include "utils.h"
#include "config.h"

void get_output_filename(const char *input, char *output, int max_len, const char *extension) {
    strncpy(output, input, max_len - 1);
    output[max_len - 1] = '\0';

    char *dot = strrchr(output, '.');
    if (dot) {
        *dot = '\0';
    }
    strncat(output, extension, max_len - strlen(output) - 1);
}

static int remove_recursive(const char *path) {
    struct dirent *entry;
    char file_path[512];
    struct stat stat_buf;

    DIR *dir = opendir(path);
    if (!dir) return 0;

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        snprintf(file_path, sizeof(file_path), "%s/%s", path, entry->d_name);

        if (stat(file_path, &stat_buf) == 0) {
            if (S_ISDIR(stat_buf.st_mode)) {
                remove_recursive(file_path);
                rmdir(file_path);
            } else {
                remove(file_path);
            }
        }
    }

    closedir(dir);
    return 0;
}

int clean_build_artifacts(const char *project_dir) {
    printf("Limpiando proyecto '%s'...\n", project_dir);

    if (access(project_dir, F_OK) == -1) {
        fprintf(stderr, "Error: proyecto '%s' no existe\n", project_dir);
        return EXIT_FAILURE;
    }

    // Leer configuración para saber el nombre del proyecto
    ProjectConfig *config = read_project_config(project_dir);
    if (!config) {
        fprintf(stderr, "Error: No se puede leer configuración del proyecto\n");
        return EXIT_FAILURE;
    }

    // Eliminar archivo compilado principal (.blas, .sblas, .dblas)
    const char *extensions[] = {".blas", ".sblas", ".dblas"};
    for (int i = 0; i < 3; i++) {
        char filepath[256];
        snprintf(filepath, sizeof(filepath), "%s/%s%s", project_dir, config->name[0] ? config->name : "output", extensions[i]);
        
        if (access(filepath, F_OK) == 0) {
            if (remove(filepath) == 0) {
                printf("✓ Eliminado: %s\n", filepath);
            }
        }
    }

    // También eliminar cualquier .blas en el directorio raíz del proyecto (archivos viejos)
    DIR *dir = opendir(project_dir);
    if (dir) {
        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL) {
            // Skip archivos del proyecto principal y directorios
            if (strstr(entry->d_name, ".blas") || strstr(entry->d_name, ".sblas") || strstr(entry->d_name, ".dblas")) {
                char full_path[256];
                snprintf(full_path, sizeof(full_path), "%s/%s", project_dir, entry->d_name);
                
                // No eliminar el archivo principal del proyecto
                if (strcmp(entry->d_name, config->name) != 0) {
                    if (remove(full_path) == 0) {
                        printf("✓ Eliminado: %s\n", full_path);
                    }
                }
            }
        }
        closedir(dir);
    }

    printf("✓ Proyecto limpiado\n");
    free_project_config(config);
    return EXIT_SUCCESS;
}
