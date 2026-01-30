#ifndef CONFIG_H
#define CONFIG_H

typedef struct {
    char type[32];      // executable, static_lib, dynamic_lib
    char name[256];
    char entry[256];
    char output[256];
    
    // Configuración gráfica
    char window_title[256];
    int window_width;
    int window_height;
    int window_resizable;  // 0 = no, 1 = sí
    char window_mode[32];  // windowed, fullscreen, borderless
    
    // Configuración de renderer
    char renderer[32];     // opengl, none
    int fps;               // Frames por segundo (valor por defecto: 60)
} ProjectConfig;

ProjectConfig* read_project_config(const char *project_dir);
void free_project_config(ProjectConfig *config);

#endif
