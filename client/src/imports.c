#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "imports.h"

FileList* extract_imports(const char *source_file, const char *project_dir) {
    FileList *list = (FileList*)malloc(sizeof(FileList));
    if (!list) return NULL;

    list->files = NULL;
    list->count = 0;

    FILE *src = fopen(source_file, "r");
    if (!src) {
        return list;
    }

    char line[512];
    while (fgets(line, sizeof(line), src)) {
        // Buscar líneas que comiencen con "import"
        if (strncmp(line, "import", 6) == 0) {
            // Extraer nombre del archivo entre comillas
            const char *start = strchr(line, '"');
            if (!start) continue;
            start++;

            const char *end = strchr(start, '"');
            if (!end) continue;

            int len = end - start;
            char *import_file = (char*)malloc(len + 1);
            if (!import_file) continue;

            strncpy(import_file, start, len);
            import_file[len] = '\0';

            char full_path[512];
            char cwd[512];
            getcwd(cwd, sizeof(cwd));

            // Intentar primero en project_dir/src/
            snprintf(full_path, sizeof(full_path), "%s/src/%s", project_dir, import_file);
            
            if (access(full_path, F_OK) == -1) {
                // Intentar con .bsf en stdlib/
                char stdlib_path[512];
                snprintf(stdlib_path, sizeof(stdlib_path), "%s/stdlib/%s", cwd, import_file);
                
                if (access(stdlib_path, F_OK) == 0) {
                    strcpy(full_path, stdlib_path);
                } else {
                    // Intentar con .sblas en stdlib/ (si el import no especifica extensión)
                    char basename[256];
                    strcpy(basename, import_file);
                    
                    // Si no tiene extensión, intentar .sblas
                    if (!strstr(basename, ".bsf") && !strstr(basename, ".sblas")) {
                        snprintf(stdlib_path, sizeof(stdlib_path), "%s/stdlib/%s.sblas", cwd, basename);
                        if (access(stdlib_path, F_OK) == 0) {
                            strcpy(full_path, stdlib_path);
                        } else {
                            // Si no existe, mantener ruta original
                            snprintf(full_path, sizeof(full_path), "%s/src/%s", project_dir, import_file);
                        }
                    }
                }
            }

            // Agregar a la lista
            char **temp = realloc(list->files, (list->count + 1) * sizeof(char*));
            if (!temp) {
                free(import_file);
                continue;
            }

            list->files = temp;
            list->files[list->count] = (char*)malloc(strlen(full_path) + 1);
            if (list->files[list->count]) {
                strcpy(list->files[list->count], full_path);
                list->count++;
            }

            free(import_file);
        }
    }

    fclose(src);
    return list;
}

void free_file_list(FileList *list) {
    if (!list) return;
    for (int i = 0; i < list->count; i++) {
        free(list->files[i]);
    }
    free(list->files);
    free(list);
}
