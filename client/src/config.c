#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "config.h"

ProjectConfig* read_project_config(const char *project_dir) {
    ProjectConfig *config = (ProjectConfig*)malloc(sizeof(ProjectConfig));
    if (!config) return NULL;

    // Valores por defecto
    strcpy(config->type, "executable");
    strcpy(config->name, "");
    strcpy(config->entry, "src/main.bsf");
    strcpy(config->output, "");
    
    // Valores gráficos por defecto
    strcpy(config->window_title, "Golden Application");
    config->window_width = 800;
    config->window_height = 600;
    config->window_resizable = 0;
    strcpy(config->window_mode, "windowed");
    strcpy(config->renderer, "auto");  // auto = detectar según plataforma
    config->fps = 60;  // Valor por defecto: 60 FPS

    // Leer archivo project.conf
    char conf_path[512];
    snprintf(conf_path, sizeof(conf_path), "%s/project.conf", project_dir);

    FILE *conf = fopen(conf_path, "r");
    if (!conf) {
        return config;
    }

    char line[256];
    while (fgets(line, sizeof(line), conf)) {
        // Eliminar espacios al inicio y salto de línea
        char *key = line;
        while (*key == ' ' || *key == '\t') key++;
        
        if (*key == '[' || *key == '#' || *key == '\n') continue;

        // Buscar "type = ..."
        if (strncmp(key, "type", 4) == 0) {
            char *value = strchr(key, '=');
            if (value) {
                value++;
                while (*value == ' ' || *value == '\t') value++;
                
                int len = strlen(value);
                while (len > 0 && (value[len-1] == '\n' || value[len-1] == ' ')) {
                    len--;
                }
                strncpy(config->type, value, len);
                config->type[len] = '\0';
            }
        }

        // Buscar "name = ..."
        if (strncmp(key, "name", 4) == 0) {
            char *value = strchr(key, '=');
            if (value) {
                value++;
                while (*value == ' ' || *value == '\t') value++;
                
                int len = strlen(value);
                while (len > 0 && (value[len-1] == '\n' || value[len-1] == ' ')) {
                    len--;
                }
                strncpy(config->name, value, len);
                config->name[len] = '\0';
            }
        }
        
        // Buscar window_title
        if (strncmp(key, "window_title", 12) == 0) {
            char *value = strchr(key, '=');
            if (value) {
                value++;
                while (*value == ' ' || *value == '\t') value++;
                
                int len = strlen(value);
                while (len > 0 && (value[len-1] == '\n' || value[len-1] == ' ')) {
                    len--;
                }
                strncpy(config->window_title, value, len);
                config->window_title[len] = '\0';
            }
        }
        
        // Buscar window_width
        if (strncmp(key, "window_width", 12) == 0) {
            char *value = strchr(key, '=');
            if (value) {
                config->window_width = atoi(value + 1);
            }
        }
        
        // Buscar window_height
        if (strncmp(key, "window_height", 13) == 0) {
            char *value = strchr(key, '=');
            if (value) {
                config->window_height = atoi(value + 1);
            }
        }
        
        // Buscar window_resizable
        if (strncmp(key, "window_resizable", 16) == 0) {
            char *value = strchr(key, '=');
            if (value) {
                value++;
                while (*value == ' ' || *value == '\t') value++;
                if (*value == 'y' || *value == 'Y' || *value == '1') {
                    config->window_resizable = 1;
                }
            }
        }
        
        // Buscar window_mode
        if (strncmp(key, "window_mode", 11) == 0) {
            char *value = strchr(key, '=');
            if (value) {
                value++;
                while (*value == ' ' || *value == '\t') value++;
                
                int len = strlen(value);
                while (len > 0 && (value[len-1] == '\n' || value[len-1] == ' ')) {
                    len--;
                }
                strncpy(config->window_mode, value, len);
                config->window_mode[len] = '\0';
            }
        }
        
        // Buscar renderer
        if (strncmp(key, "renderer", 8) == 0) {
            char *value = strchr(key, '=');
            if (value) {
                value++;
                while (*value == ' ' || *value == '\t') value++;
                
                int len = strlen(value);
                while (len > 0 && (value[len-1] == '\n' || value[len-1] == ' ')) {
                    len--;
                }
                strncpy(config->renderer, value, len);
                config->renderer[len] = '\0';
            }
        }
        
        // Buscar fps
        if (strncmp(key, "fps", 3) == 0) {
            char *value = strchr(key, '=');
            if (value) {
                config->fps = atoi(value + 1);
                if (config->fps < 1 || config->fps > 240) {
                    config->fps = 60;  // Restricción: entre 1 y 240 FPS
                }
            }
        }
    }

    fclose(conf);
    return config;
}

void free_project_config(ProjectConfig *config) {
    if (config) {
        free(config);
    }
}
