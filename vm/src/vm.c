#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "vm.h"
#include "utils.h"

#include <GLFW/glfw3.h>
#include <GL/gl.h>

#ifdef _WIN32
    #define PLATFORM "windows"
#elif __APPLE__
    #define PLATFORM "macos"
#else
    #define PLATFORM "linux"
#endif

#define OPCODE_PRINT 0x01
#define OPCODE_PRINTLN 0x08
#define OPCODE_PRINTCHR 0x09
#define OPCODE_GET_GLOBAL 0x0A
#define OPCODE_RETURN 0xFF
#define OPCODE_ARRAY_SET 0x0C
#define OPCODE_ARRAY_GET 0x0D
#define OPCODE_ARRAY_NEW 0x0E
#define OPCODE_ARRAY_LEN 0x0F
#define OPCODE_ARRAY_CLEAR 0x10

// Función para resolver el renderer automático según la plataforma
static void resolve_renderer(char *renderer) {
    if (strcmp(renderer, "auto") != 0) {
        return;  // Renderer ya especificado explícitamente
    }
    
    // OpenGL en todas las plataformas
    strcpy(renderer, "opengl");
}

// Función para inicializar OpenGL y crear ventana
static GLFWwindow* init_opengl_window(const WindowConfig *config) {
    if (!glfwInit()) {
        fprintf(stderr, "Error: No se puede inicializar GLFW\n");
        return NULL;
    }
    
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_RESIZABLE, config->window_resizable ? GLFW_TRUE : GLFW_FALSE);
    
    GLFWwindow *window = glfwCreateWindow(
        config->window_width,
        config->window_height,
        config->window_title,
        NULL,
        NULL
    );
    
    if (!window) {
        fprintf(stderr, "Error: No se puede crear ventana GLFW\n");
        glfwTerminate();
        return NULL;
    }
    
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);  // VSync activado
    
    // Configurar OpenGL
    glViewport(0, 0, config->window_width, config->window_height);
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    
    return window;
}

int execute_bytecode(const char *bytecode_file, int debug, const char *override_renderer) {
    FILE *file = fopen(bytecode_file, "rb");
    if (!file) {
        fprintf(stderr, "Error: No se puede abrir '%s'\n", bytecode_file);
        return EXIT_FAILURE;
    }

    // Leer header
    char header[4];
    if (fread(header, 1, 4, file) != 4) {
        fprintf(stderr, "Error: No se puede leer header del bytecode\n");
        fclose(file);
        return EXIT_FAILURE;
    }
    
    if (strncmp(header, "GOLD", 4) != 0) {
        fprintf(stderr, "Error: Bytecode inválido (header incorrecto)\n");
        fclose(file);
        return EXIT_FAILURE;
    }

    // Leer versión
    uint8_t version;
    if (fread(&version, 1, 1, file) != 1) {
        fprintf(stderr, "Error: No se puede leer versión del bytecode\n");
        fclose(file);
        return EXIT_FAILURE;
    }
    
    if (debug) printf("[VM] Bytecode version: %d\n", version);

    // Leer configuración de ventana
    WindowConfig window_config = {0};
    strcpy(window_config.window_title, "Golden Application");
    window_config.window_width = 800;
    window_config.window_height = 600;
    window_config.window_resizable = 0;
    strcpy(window_config.window_mode, "windowed");
    strcpy(window_config.renderer, "auto");
    
    uint8_t window_title_len = 0;
    if (fread(&window_title_len, 1, 1, file) != 1) {
        fprintf(stderr, "Error: No se puede leer longitud del título de ventana\n");
        fclose(file);
        return EXIT_FAILURE;
    }
    if (window_title_len > 0 && window_title_len < 256) {
        if (fread(window_config.window_title, 1, window_title_len, file) != window_title_len) {
            fprintf(stderr, "Error: No se puede leer título de ventana\n");
            fclose(file);
            return EXIT_FAILURE;
        }
        window_config.window_title[window_title_len] = '\0';
    }
    
    if (fread(&window_config.window_width, sizeof(uint16_t), 1, file) != 1) {
        fprintf(stderr, "Error: No se puede leer ancho de ventana\n");
        fclose(file);
        return EXIT_FAILURE;
    }
    
    if (fread(&window_config.window_height, sizeof(uint16_t), 1, file) != 1) {
        fprintf(stderr, "Error: No se puede leer alto de ventana\n");
        fclose(file);
        return EXIT_FAILURE;
    }
    
    if (fread(&window_config.window_resizable, 1, 1, file) != 1) {
        fprintf(stderr, "Error: No se puede leer resizable de ventana\n");
        fclose(file);
        return EXIT_FAILURE;
    }
    
    uint8_t window_mode_len = 0;
    if (fread(&window_mode_len, 1, 1, file) != 1) {
        fprintf(stderr, "Error: No se puede leer longitud del modo de ventana\n");
        fclose(file);
        return EXIT_FAILURE;
    }
    if (window_mode_len > 0 && window_mode_len < 32) {
        if (fread(window_config.window_mode, 1, window_mode_len, file) != window_mode_len) {
            fprintf(stderr, "Error: No se puede leer modo de ventana\n");
            fclose(file);
            return EXIT_FAILURE;
        }
        window_config.window_mode[window_mode_len] = '\0';
    }
    
    // Leer renderer
    uint8_t renderer_len = 0;
    if (fread(&renderer_len, 1, 1, file) != 1) {
        fprintf(stderr, "Error: No se puede leer longitud del renderer\n");
        fclose(file);
        return EXIT_FAILURE;
    }
    if (renderer_len > 0 && renderer_len < 32) {
        if (fread(window_config.renderer, 1, renderer_len, file) != renderer_len) {
            fprintf(stderr, "Error: No se puede leer renderer\n");
            fclose(file);
            return EXIT_FAILURE;
        }
        window_config.renderer[renderer_len] = '\0';
    }
    
    // Leer fps
    uint16_t fps = 60;  // Valor por defecto
    if (fread(&fps, sizeof(uint16_t), 1, file) != 1) {
        fprintf(stderr, "Error: No se puede leer fps\n");
        fclose(file);
        return EXIT_FAILURE;
    }
    if (fps < 1 || fps > 240) {
        fps = 60;  // Validar rango
    }
    window_config.fps = fps;
    
    // Resolver renderer automático según plataforma
    resolve_renderer(window_config.renderer);
    
    // Sobrescribir renderer si se especificó desde línea de comandos
    if (override_renderer && strlen(override_renderer) > 0) {
        strcpy(window_config.renderer, override_renderer);
        if (debug) printf("[VM] Renderer overridden from command line: %s\n", override_renderer);
    }
    
    if (debug) {
        printf("[VM] Window configuration:\n");
        printf("  Title: %s\n", window_config.window_title);
        printf("  Resolution: %d x %d\n", window_config.window_width, window_config.window_height);
        printf("  Resizable: %s\n", window_config.window_resizable ? "Yes" : "No");
        printf("  Mode: %s\n", window_config.window_mode);
        printf("  Renderer: %s\n", window_config.renderer);
        printf("  FPS: %d\n", window_config.fps);
    }

    // Leer cantidad de variables globales
    uint16_t var_count = 0;
    if (fread(&var_count, sizeof(uint16_t), 1, file) != 1) {
        fprintf(stderr, "Error: No se puede leer cantidad de variables\n");
        fclose(file);
        return EXIT_FAILURE;
    }

    if (debug) printf("[VM] Global variables: %d\n", var_count);

    // Leer variables globales (optimizado con tipo)
    Variable *variables = NULL;
    if (var_count > 0) {
        variables = (Variable*)malloc(var_count * sizeof(Variable));
        if (!variables) {
            fprintf(stderr, "Error: No hay memoria suficiente\n");
            fclose(file);
            return EXIT_FAILURE;
        }

        for (int i = 0; i < var_count; i++) {
            uint8_t name_len = 0;
            if (fread(&name_len, 1, 1, file) != 1) {
                fprintf(stderr, "Error: No se puede leer nombre de variable\n");
                fclose(file);
                return EXIT_FAILURE;
            }

            variables[i].name = (char*)malloc(name_len + 1);
            if (!variables[i].name) {
                fprintf(stderr, "Error: No hay memoria suficiente\n");
                fclose(file);
                return EXIT_FAILURE;
            }

            if (fread(variables[i].name, 1, name_len, file) != name_len) {
                fprintf(stderr, "Error: No se puede leer nombre de variable\n");
                fclose(file);
                return EXIT_FAILURE;
            }
            variables[i].name[name_len] = '\0';

            // Leer tipo de variable
            uint8_t var_type = 0;
            if (fread(&var_type, 1, 1, file) != 1) {
                fprintf(stderr, "Error: No se puede leer tipo de variable\n");
                fclose(file);
                return EXIT_FAILURE;
            }
            variables[i].type = var_type;

            // Leer valor según tipo
            if (var_type == 's') {
                // String: leer longitud + contenido
                uint16_t str_len = 0;
                if (fread(&str_len, sizeof(uint16_t), 1, file) != 1) {
                    fprintf(stderr, "Error: No se puede leer longitud de string\n");
                    fclose(file);
                    return EXIT_FAILURE;
                }
                
                variables[i].str_val = (char*)malloc(str_len + 1);
                if (!variables[i].str_val) {
                    fprintf(stderr, "Error: No hay memoria suficiente\n");
                    fclose(file);
                    return EXIT_FAILURE;
                }
                
                if (fread(variables[i].str_val, 1, str_len, file) != str_len) {
                    fprintf(stderr, "Error: No se puede leer contenido de string\n");
                    fclose(file);
                    return EXIT_FAILURE;
                }
                variables[i].str_val[str_len] = '\0';
                variables[i].value = 0;
                
                if (debug) printf("[VM] Variable %d (string): %s = \"%s\"\n", i, variables[i].name, variables[i].str_val);
            } else if (var_type == 'a') {
                // Array estático: leer tipo de elemento y tamaño
                uint8_t element_type = 0;
                int array_size = 0;
                if (fread(&element_type, 1, 1, file) != 1 || 
                    fread(&array_size, sizeof(int), 1, file) != 1) {
                    fprintf(stderr, "Error: No se puede leer información del array\n");
                    fclose(file);
                    return EXIT_FAILURE;
                }
                // Por ahora, almacenar en value como tamaño
                variables[i].value = (double)array_size;
                variables[i].str_val = NULL;
                
                if (debug) printf("[VM] Variable %d (array estático): %s[%d] (tipo: %c)\n", i, variables[i].name, array_size, element_type);
            } else if (var_type == 'b') {
                // Array dinámico: leer tipo de elemento (tamaño será 0)
                uint8_t element_type = 0;
                if (fread(&element_type, 1, 1, file) != 1) {
                    fprintf(stderr, "Error: No se puede leer tipo de array dinámico\n");
                    fclose(file);
                    return EXIT_FAILURE;
                }
                // Almacenar el tipo de elemento en value (como byte)
                variables[i].value = (double)element_type;
                variables[i].str_val = NULL;
                
                if (debug) printf("[VM] Variable %d (array dinámico): %s (tipo elemento: %c)\n", i, variables[i].name, element_type);
            } else {
                // Numeric: leer double
                variables[i].str_val = NULL;
                if (fread(&variables[i].value, sizeof(double), 1, file) != 1) {
                    fprintf(stderr, "Error: No se puede leer valor de variable\n");
                    fclose(file);
                    return EXIT_FAILURE;
                }

                if (debug) printf("[VM] Variable %d (%c): %s = %f\n", i, var_type, variables[i].name, variables[i].value);
            }
        }
    }

    // Leer cantidad de clases
    uint16_t class_count = 0;
    if (fread(&class_count, sizeof(uint16_t), 1, file) != 1) {
        fprintf(stderr, "Error: No se puede leer cantidad de clases\n");
        fclose(file);
        return EXIT_FAILURE;
    }

    if (debug) printf("[VM] Classes: %d\n", class_count);

    // Leer definiciones de clases
    ClassDefinition *classes = NULL;
    if (class_count > 0) {
        classes = (ClassDefinition*)malloc(class_count * sizeof(ClassDefinition));
        if (!classes) {
            fprintf(stderr, "Error: No hay memoria suficiente\n");
            fclose(file);
            return EXIT_FAILURE;
        }

        for (int i = 0; i < class_count; i++) {
            // Leer nombre de clase
            uint8_t name_len = 0;
            if (fread(&name_len, 1, 1, file) != 1) {
                fprintf(stderr, "Error: No se puede leer nombre de clase\n");
                fclose(file);
                return EXIT_FAILURE;
            }

            classes[i].name = (char*)malloc(name_len + 1);
            if (!classes[i].name) {
                fprintf(stderr, "Error: No hay memoria suficiente\n");
                fclose(file);
                return EXIT_FAILURE;
            }

            if (fread(classes[i].name, 1, name_len, file) != name_len) {
                fprintf(stderr, "Error: No se puede leer nombre de clase\n");
                fclose(file);
                return EXIT_FAILURE;
            }
            classes[i].name[name_len] = '\0';

            // Leer cantidad de variables de instancia
            uint8_t ivar_count = 0;
            if (fread(&ivar_count, 1, 1, file) != 1) {
                fprintf(stderr, "Error: No se puede leer cantidad de variables de instancia\n");
                fclose(file);
                return EXIT_FAILURE;
            }

            classes[i].var_count = ivar_count;
            classes[i].var_names = NULL;
            classes[i].var_types = NULL;

            if (ivar_count > 0) {
                classes[i].var_names = (char**)malloc(ivar_count * sizeof(char*));
                classes[i].var_types = (uint8_t*)malloc(ivar_count * sizeof(uint8_t));

                if (!classes[i].var_names || !classes[i].var_types) {
                    fprintf(stderr, "Error: No hay memoria suficiente\n");
                    fclose(file);
                    return EXIT_FAILURE;
                }

                for (int j = 0; j < ivar_count; j++) {
                    uint8_t var_name_len = 0;
                    if (fread(&var_name_len, 1, 1, file) != 1) {
                        fprintf(stderr, "Error: No se puede leer nombre de variable de instancia\n");
                        fclose(file);
                        return EXIT_FAILURE;
                    }

                    classes[i].var_names[j] = (char*)malloc(var_name_len + 1);
                    if (!classes[i].var_names[j]) {
                        fprintf(stderr, "Error: No hay memoria suficiente\n");
                        fclose(file);
                        return EXIT_FAILURE;
                    }

                    if (fread(classes[i].var_names[j], 1, var_name_len, file) != var_name_len) {
                        fprintf(stderr, "Error: No se puede leer nombre de variable de instancia\n");
                        fclose(file);
                        return EXIT_FAILURE;
                    }
                    classes[i].var_names[j][var_name_len] = '\0';

                    if (fread(&classes[i].var_types[j], 1, 1, file) != 1) {
                        fprintf(stderr, "Error: No se puede leer tipo de variable de instancia\n");
                        fclose(file);
                        return EXIT_FAILURE;
                    }

                    if (debug) printf("[VM] Clase '%s' variable %d: %s (tipo: %c)\n", 
                                    classes[i].name, j, classes[i].var_names[j], classes[i].var_types[j]);
                }
            }

            // Leer cantidad de métodos
            uint8_t method_count = 0;
            if (fread(&method_count, 1, 1, file) != 1) {
                fprintf(stderr, "Error: No se puede leer cantidad de métodos\n");
                fclose(file);
                return EXIT_FAILURE;
            }
            
            classes[i].method_count = method_count;
            if (method_count > 0) {
                classes[i].methods = (ClassMethod*)malloc(method_count * sizeof(ClassMethod));
            } else {
                classes[i].methods = NULL;
            }
            
            // Leer métodos
            for (int j = 0; j < method_count; j++) {
                // Leer nombre del método
                uint8_t method_name_len = 0;
                if (fread(&method_name_len, 1, 1, file) != 1) {
                    fprintf(stderr, "Error: No se puede leer longitud de nombre de método\n");
                    fclose(file);
                    return EXIT_FAILURE;
                }
                
                char method_name[256] = {0};
                if (fread(method_name, 1, method_name_len, file) != method_name_len) {
                    fprintf(stderr, "Error: No se puede leer nombre de método\n");
                    fclose(file);
                    return EXIT_FAILURE;
                }
                
                classes[i].methods[j].name = (char*)malloc(method_name_len + 1);
                strncpy(classes[i].methods[j].name, method_name, method_name_len);
                classes[i].methods[j].name[method_name_len] = '\0';
                
                // Leer información del método
                if (fread(&classes[i].methods[j].is_public, 1, 1, file) != 1 ||
                    fread(&classes[i].methods[j].start_instruction, sizeof(int), 1, file) != 1 ||
                    fread(&classes[i].methods[j].instruction_count, sizeof(int), 1, file) != 1 ||
                    fread(&classes[i].methods[j].param_count, 1, 1, file) != 1) {
                    fprintf(stderr, "Error: No se puede leer información del método\n");
                    fclose(file);
                    return EXIT_FAILURE;
                }
                
                if (debug) printf("[VM] Clase '%s' método %d: %s (%s)\n", 
                                classes[i].name, j, classes[i].methods[j].name,
                                classes[i].methods[j].is_public ? "public" : "private");
            }
        }
    }

    // Leer cantidad de strings
    uint16_t string_count = 0;
    if (fread(&string_count, sizeof(uint16_t), 1, file) != 1) {
        fprintf(stderr, "Error: No se puede leer cantidad de strings\n");
        fclose(file);
        return EXIT_FAILURE;
    }

    if (debug) printf("[VM] Strings: %d\n", string_count);

    // Leer strings
    char **strings = NULL;
    if (string_count > 0) {
        strings = (char**)malloc(string_count * sizeof(char*));
        if (!strings) {
            fprintf(stderr, "Error: No hay memoria suficiente\n");
            fclose(file);
            return EXIT_FAILURE;
        }

        for (int i = 0; i < string_count; i++) {
            uint16_t str_len = 0;
            if (fread(&str_len, sizeof(uint16_t), 1, file) != 1) {
                fprintf(stderr, "Error: No se puede leer longitud de string\n");
                fclose(file);
                return EXIT_FAILURE;
            }

            strings[i] = (char*)malloc(str_len + 1);
            if (!strings[i]) {
                fprintf(stderr, "Error: No hay memoria suficiente\n");
                fclose(file);
                return EXIT_FAILURE;
            }

            if (fread(strings[i], 1, str_len, file) != str_len) {
                fprintf(stderr, "Error: No se puede leer string\n");
                fclose(file);
                return EXIT_FAILURE;
            }
            strings[i][str_len] = '\0';

            if (debug) printf("[VM] String %d: '%s'\n", i, strings[i]);
        }
    }

    // Leer instrucciones
    Instruction *instructions = NULL;
    int instruction_count = 0;
    Instruction instr;
    
    while (fread(&instr, sizeof(Instruction), 1, file) == 1) {
        Instruction *temp = realloc(instructions, (instruction_count + 1) * sizeof(Instruction));
        if (!temp) {
            fprintf(stderr, "Error: No hay memoria suficiente\n");
            free(instructions);
            fclose(file);
            return EXIT_FAILURE;
        }
        instructions = temp;
        instructions[instruction_count] = instr;
        instruction_count++;
    }

    fclose(file);

    if (debug) printf("[VM] Executing %d instructions...\n", instruction_count);

    // Crear estado de la VM
    VMState vm;
    vm.instructions = instructions;
    vm.instruction_count = instruction_count;
    vm.pc = 0;
    vm.sp = 0;
    memset(vm.stack, 0, sizeof(vm.stack));
    vm.string_pool.strings = strings;
    vm.string_pool.string_count = string_count;
    vm.variables = variables;
    vm.variable_count = var_count;
    vm.class_pool.classes = classes;
    vm.class_pool.class_count = class_count;
    vm.objects = NULL;
    vm.object_count = 0;
    
    // Almacenar configuración de ventana
    vm.window_config = window_config;
    
    // Inicializar arrays (solo los metadatos, la memoria se asigna dinámicamente)
    vm.arrays = NULL;
    vm.array_count = 0;
    
    // Crear estructura de arrays basada en variables de tipo 'a'
    for (int i = 0; i < var_count; i++) {
        if (variables[i].type == 'a') {
            Array *temp = realloc(vm.arrays, (vm.array_count + 1) * sizeof(Array));
            if (!temp) {
                fprintf(stderr, "Error: No hay memoria para arrays\n");
                return EXIT_FAILURE;
            }
            vm.arrays = temp;
            
            int arr_size = (int)variables[i].value;
            vm.arrays[vm.array_count].name = variables[i].name;
            vm.arrays[vm.array_count].size = arr_size;
            // Tipo del elemento se determina desde variables[i].type será 'a', pero necesitamos guardarlo
            // Por ahora asumimos que todos los arrays son de int
            vm.arrays[vm.array_count].type = 'i';
            vm.arrays[vm.array_count].data = (double*)malloc(arr_size * sizeof(double));
            vm.arrays[vm.array_count].str_data = NULL;
            
            if (!vm.arrays[vm.array_count].data) {
                fprintf(stderr, "Error: No hay memoria para datos del array\n");
                return EXIT_FAILURE;
            }
            
            // Inicializar todos los elementos a 0
            for (int j = 0; j < arr_size; j++) {
                vm.arrays[vm.array_count].data[j] = 0;
            }
            
            vm.array_count++;
        }
    }

    // Variable temporal para almacenar el valor de GET_GLOBAL
    char *last_global_string = NULL;

    // Inicializar OpenGL si el renderer es opengl
    GLFWwindow *window = NULL;
    if (strcmp(vm.window_config.renderer, "opengl") == 0) {
        window = init_opengl_window(&vm.window_config);
        if (window) {
            if (debug) printf("[VM] OpenGL window initialized successfully\n");
        } else {
            if (debug) printf("[VM] Warning: Could not initialize OpenGL, running in console mode\n");
        }
    }

    // Ejecutar instrucciones
    int executed = 0;
    
    // Calcular tiempo por frame según FPS
    double frame_time = 1.0 / vm.window_config.fps;
    double last_frame_time = glfwGetTime();
    
    // Loop de ventana (si OpenGL está activo)
    if (window) {
        // Ejecutar bytecode en el contexto de la ventana
        while (!glfwWindowShouldClose(window) && vm.pc < vm.instruction_count) {
            // Control de frame rate
            double current_time = glfwGetTime();
            double elapsed = current_time - last_frame_time;
            
            if (elapsed >= frame_time) {
                last_frame_time = current_time;
                
                // Limpiar pantalla
                glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
                
                // Ejecutar instrucción
                Instruction current = vm.instructions[vm.pc];

        if (debug) printf("[VM] PC: %d, Opcode: 0x%02x\n", vm.pc, current.opcode);

        switch (current.opcode) {
            case OPCODE_PRINT:
                // current.arg1 es el índice del string
                if (current.arg1 < vm.string_pool.string_count) {
                    printf("%s", vm.string_pool.strings[current.arg1]);
                    if (debug) printf("[VM] PRINT string #%d\n", current.arg1);
                } else {
                    if (debug) printf("[VM] Error: string index fuera de rango\n");
                }
                break;
            
            case OPCODE_PRINTLN:
                // Si hay un valor en el stack (de ARRAY_GET, etc), imprimirlo
                if (vm.sp > 0) {
                    double val = vm.stack[vm.sp - 1];
                    // Determinar si es entero o float
                    if (val == (int)val) {
                        printf("%d\n", (int)val);
                    } else {
                        printf("%f\n", val);
                    }
                    vm.sp--;  // Pop del stack
                    if (debug) printf("[VM] PRINTLN (stack value) = %f\n", val);
                }
                // Si hay un string global pendiente, usarlo
                else if (last_global_string) {
                    printf("%s\n", last_global_string);
                    if (debug) printf("[VM] PRINTLN (global string)\n");
                    last_global_string = NULL;
                } else if (current.arg1 < vm.string_pool.string_count) {
                    printf("%s\n", vm.string_pool.strings[current.arg1]);
                    if (debug) printf("[VM] PRINTLN string #%d\n", current.arg1);
                } else {
                    if (debug) printf("[VM] Error: string index fuera de rango\n");
                }
                break;
            
            case OPCODE_PRINTCHR:
                // current.arg1 es el carácter ASCII a imprimir (optimizado, sin string pool)
                putchar(current.arg1);
                if (debug) printf("[VM] PRINTCHR 0x%02x\n", current.arg1);
                break;
            
            case OPCODE_GET_GLOBAL:
                // current.arg1 es el índice de variable global
                if (current.arg1 < var_count) {
                    Variable *var = &variables[current.arg1];
                    if (var->type == 's' && var->str_val) {
                        last_global_string = var->str_val;
                        if (debug) printf("[VM] GET_GLOBAL %s (string) = \"%s\"\n", var->name, var->str_val);
                    }
                }
                break;
            
            case OPCODE_PUSH_VALUE:
                // current.arg1 es el índice del string en el pool
                // Convertir string a double y pushear al value stack
                if (current.arg1 < vm.string_pool.string_count && vm.sp < 256) {
                    char *str_val = vm.string_pool.strings[current.arg1];
                    double num_val = atof(str_val);
                    vm.stack[vm.sp] = num_val;
                    vm.sp++;
                    if (debug) printf("[VM] PUSH_VALUE string #%d ('%s') as %f\n", 
                                    current.arg1, str_val, num_val);
                } else {
                    if (debug) printf("[VM] Error: PUSH_VALUE invalid state\n");
                }
                break;
            
            case OPCODE_NEW_INSTANCE:
                // current.arg1 es el índice de la clase
                if (current.arg1 < vm.class_pool.class_count) {
                    ClassDefinition *cls = &vm.class_pool.classes[current.arg1];
                    
                    // Crear nueva instancia de objeto
                    ObjectInstance *new_obj = (ObjectInstance*)malloc(sizeof(ObjectInstance));
                    new_obj->class_name = (char*)malloc(strlen(cls->name) + 1);
                    strcpy(new_obj->class_name, cls->name);
                    new_obj->field_count = cls->var_count;
                    new_obj->field_values = (double*)malloc(cls->var_count * sizeof(double));
                    
                    // Inicializar campos a 0
                    for (int i = 0; i < cls->var_count; i++) {
                        new_obj->field_values[i] = 0.0;
                    }
                    
                    // Agregar a lista de objetos
                    ObjectInstance *temp = realloc(vm.objects, (vm.object_count + 1) * sizeof(ObjectInstance));
                    if (temp) {
                        vm.objects = temp;
                        vm.objects[vm.object_count] = *new_obj;
                        
                        // Pushear índice del objeto al stack
                        if (vm.obj_sp < 64) {
                            vm.object_stack[vm.obj_sp] = vm.object_count;
                            vm.obj_sp++;
                        }
                        
                        if (debug) printf("[VM] NEW_INSTANCE '%s' (id: %d)\n", cls->name, vm.object_count);
                        vm.object_count++;
                    }
                    free(new_obj);
                } else {
                    if (debug) printf("[VM] Error: clase index fuera de rango\n");
                }
                break;
            
            case OPCODE_SET_FIELD:
                // current.arg1 = field index, arg2 = object index
                if (vm.obj_sp > 0) {
                    int obj_idx = vm.object_stack[vm.obj_sp - 1];
                    if (obj_idx >= 0 && obj_idx < vm.object_count) {
                        ObjectInstance *obj = &vm.objects[obj_idx];
                        // Usar top del value stack
                        if (current.arg1 < obj->field_count && vm.sp > 0) {
                            obj->field_values[current.arg1] = vm.stack[vm.sp - 1];
                            if (debug) printf("[VM] SET_FIELD object %d, field %d = %f\n", 
                                            obj_idx, current.arg1, vm.stack[vm.sp - 1]);
                            vm.sp--;  // Pop value stack
                        }
                    }
                }
                break;
            
            case OPCODE_GET_FIELD:
                // current.arg1 = field index, arg2 = object index
                if (vm.obj_sp > 0) {
                    int obj_idx = vm.object_stack[vm.obj_sp - 1];
                    if (obj_idx >= 0 && obj_idx < vm.object_count) {
                        ObjectInstance *obj = &vm.objects[obj_idx];
                        if (current.arg1 < obj->field_count && vm.sp < 256) {
                            vm.stack[vm.sp] = obj->field_values[current.arg1];
                            if (debug) printf("[VM] GET_FIELD object %d, field %d = %f\n", 
                                            obj_idx, current.arg1, obj->field_values[current.arg1]);
                            vm.sp++;  // Push to value stack
                        }
                    }
                }
                break;
            
            case OPCODE_RETURN:
                if (debug) printf("[VM] RETURN - terminando ejecución\n");
                vm.pc = vm.instruction_count;  // Salir del loop
                break;
            
            case OPCODE_ARRAY_SET: {
                // arg1 = índice de variable (para ambos estáticos y dinámicos)
                // Top del stack contiene el valor, segundo top contiene el índice
                if (current.arg1 < vm.variable_count && vm.sp >= 2) {
                    int array_index = -1;
                    
                    // Obtener índice del array
                    // Para arrays estáticos, arg1 es el índice directo en vm.arrays
                    // Para arrays dinámicos, arg1 es el índice de variable, y variable->value contiene índice en vm.arrays
                    if (current.arg1 < vm.array_count) {
                        // Es array estático (índice directo)
                        array_index = current.arg1;
                    } else if (current.arg1 < vm.variable_count && vm.variables[current.arg1].value >= 0) {
                        // Es array dinámico (índice desde variable)
                        array_index = (int)vm.variables[current.arg1].value;
                    }
                    
                    if (array_index >= 0 && array_index < vm.array_count) {
                        int index = (int)vm.stack[vm.sp - 2];
                        double value = vm.stack[vm.sp - 1];
                        
                        if (index >= 0 && index < vm.arrays[array_index].size) {
                            vm.arrays[array_index].data[index] = value;
                            if (debug) printf("[VM] ARRAY_SET array %d[%d] = %f\n", array_index, index, value);
                        }
                        
                        vm.sp -= 2;  // Pop index y value
                    }
                }
                break;
            }
            
            case OPCODE_ARRAY_GET: {
                // arg1 = índice de variable (para ambos estáticos y dinámicos)
                // Top del stack contiene el índice
                if (current.arg1 < vm.variable_count && vm.sp >= 1) {
                    int array_index = -1;
                    
                    // Obtener índice del array
                    // Para arrays estáticos, arg1 es el índice directo en vm.arrays
                    // Para arrays dinámicos, arg1 es el índice de variable, y variable->value contiene índice en vm.arrays
                    if (current.arg1 < vm.array_count) {
                        // Es array estático (índice directo)
                        array_index = current.arg1;
                    } else if (current.arg1 < vm.variable_count && vm.variables[current.arg1].value >= 0) {
                        // Es array dinámico (índice desde variable)
                        array_index = (int)vm.variables[current.arg1].value;
                    }
                    
                    if (array_index >= 0 && array_index < vm.array_count) {
                        int index = (int)vm.stack[vm.sp - 1];
                        vm.sp--;  // Pop index
                        
                        if (index >= 0 && index < vm.arrays[array_index].size && vm.sp < 256) {
                            double value = vm.arrays[array_index].data[index];
                            vm.stack[vm.sp] = value;
                            vm.sp++;  // Push value
                            
                            if (debug) printf("[VM] ARRAY_GET array %d[%d] = %f\n", array_index, index, value);
                        }
                    }
                }
                break;
            }
            
            case OPCODE_ARRAY_NEW: {
                // arg1 = índice de variable en var_pool (para almacenar la referencia)
                // arg2 = tipo de elemento
                // Top del stack contiene el tamaño
                if (vm.sp > 0 && current.arg1 < vm.variable_count) {
                    int size = (int)vm.stack[vm.sp - 1];
                    vm.sp--;  // Pop size
                    
                    char element_type = (char)current.arg2;
                    
                    // Crear nuevo array dinámico
                    Array *temp = realloc(vm.arrays, (vm.array_count + 1) * sizeof(Array));
                    if (temp) {
                        vm.arrays = temp;
                        
                        vm.arrays[vm.array_count].name = vm.variables[current.arg1].name;
                        vm.arrays[vm.array_count].type = element_type;
                        vm.arrays[vm.array_count].size = size;
                        vm.arrays[vm.array_count].data = (double*)malloc(size * sizeof(double));
                        vm.arrays[vm.array_count].str_data = NULL;
                        
                        if (vm.arrays[vm.array_count].data) {
                            // Inicializar a 0
                            for (int j = 0; j < size; j++) {
                                vm.arrays[vm.array_count].data[j] = 0;
                            }
                            
                            // Almacenar el índice del array en la variable como referencia
                            vm.variables[current.arg1].value = (double)vm.array_count;
                            
                            if (debug) printf("[VM] ARRAY_NEW variable %d, array %d, size %d, type %c\n", 
                                            current.arg1, vm.array_count, size, element_type);
                            
                            vm.array_count++;
                        }
                    }
                }
                break;
            }
            
            case OPCODE_ARRAY_LEN: {
                // arg1 = índice de variable
                // Pushea la longitud del array al stack
                if (current.arg1 < vm.variable_count && vm.sp < 256) {
                    int array_index = -1;
                    
                    if (current.arg1 < vm.array_count) {
                        array_index = current.arg1;
                    } else if (current.arg1 < vm.variable_count && vm.variables[current.arg1].value >= 0) {
                        array_index = (int)vm.variables[current.arg1].value;
                    }
                    
                    if (array_index >= 0 && array_index < vm.array_count) {
                        int len = vm.arrays[array_index].size;
                        vm.stack[vm.sp] = (double)len;
                        vm.sp++;
                        if (debug) printf("[VM] ARRAY_LEN array %d = %d\n", array_index, len);
                    }
                }
                break;
            }
            
            case OPCODE_ARRAY_CLEAR: {
                // arg1 = índice de variable
                // Limpia todos los elementos del array (los pone en 0)
                if (current.arg1 < vm.variable_count) {
                    int array_index = -1;
                    
                    if (current.arg1 < vm.array_count) {
                        array_index = current.arg1;
                    } else if (current.arg1 < vm.variable_count && vm.variables[current.arg1].value >= 0) {
                        array_index = (int)vm.variables[current.arg1].value;
                    }
                    
                    if (array_index >= 0 && array_index < vm.array_count) {
                        // Limpiar el array
                        for (int i = 0; i < vm.arrays[array_index].size; i++) {
                            vm.arrays[array_index].data[i] = 0;
                        }
                        // Si hay strings, limpiarlos también
                        if (vm.arrays[array_index].str_data) {
                            for (int i = 0; i < vm.arrays[array_index].size; i++) {
                                if (vm.arrays[array_index].str_data[i]) {
                                    free(vm.arrays[array_index].str_data[i]);
                                    vm.arrays[array_index].str_data[i] = NULL;
                                }
                            }
                        }
                        if (debug) printf("[VM] ARRAY_CLEAR array %d\n", array_index);
                    }
                }
                break;
            }
            
            default:
                if (debug) printf("[VM] Instrucción desconocida: 0x%02x\n", current.opcode);
                break;
        }

        vm.pc++;
        executed++;
            } // Fin del if (elapsed >= frame_time)
            
            // Swap de buffers y eventos
            glfwSwapBuffers(window);
            glfwPollEvents();
        }
    }
    
    // Cerrar el if(window)
    else {
        // Modo consola (sin OpenGL)
        while (vm.pc < vm.instruction_count) {
            Instruction current = vm.instructions[vm.pc];

            if (debug) printf("[VM] PC: %d, Opcode: 0x%02x\n", vm.pc, current.opcode);

            switch (current.opcode) {
                case OPCODE_PRINT:
                    // current.arg1 es el índice del string
                    if (current.arg1 < vm.string_pool.string_count) {
                        printf("%s", vm.string_pool.strings[current.arg1]);
                        if (debug) printf("[VM] PRINT string #%d\n", current.arg1);
                    } else {
                        if (debug) printf("[VM] Error: string index fuera de rango\n");
                    }
                    break;
                
                case OPCODE_PRINTLN:
                    // Si hay un valor en el stack (de ARRAY_GET, etc), imprimirlo
                    if (vm.sp > 0) {
                        double val = vm.stack[vm.sp - 1];
                        // Determinar si es entero o float
                        if (val == (int)val) {
                            printf("%d\n", (int)val);
                        } else {
                            printf("%f\n", val);
                        }
                        vm.sp--;  // Pop del stack
                        if (debug) printf("[VM] PRINTLN (stack value) = %f\n", val);
                    }
                    // Si hay un string global pendiente, usarlo
                    else if (last_global_string) {
                        printf("%s\n", last_global_string);
                        if (debug) printf("[VM] PRINTLN (global string)\n");
                        last_global_string = NULL;
                    } else if (current.arg1 < vm.string_pool.string_count) {
                        printf("%s\n", vm.string_pool.strings[current.arg1]);
                        if (debug) printf("[VM] PRINTLN string #%d\n", current.arg1);
                    }
                    break;

                case OPCODE_PRINTCHR:
                    if (vm.sp > 0) {
                        int val = (int)vm.stack[vm.sp - 1];
                        printf("%c", (char)val);
                        vm.sp--;
                        if (debug) printf("[VM] PRINTCHR: %c\n", (char)val);
                    }
                    break;

                case OPCODE_GET_GLOBAL:
                    if (current.arg1 < vm.variable_count) {
                        Variable *var = &vm.variables[current.arg1];
                        if (var->type == 's') {
                            last_global_string = var->str_val;
                            if (debug) printf("[VM] GET_GLOBAL string #%d: %s\n", current.arg1, var->str_val);
                        } else {
                            vm.stack[vm.sp++] = var->value;
                            if (debug) printf("[VM] GET_GLOBAL var #%d = %f\n", current.arg1, var->value);
                        }
                    } else {
                        if (debug) printf("[VM] Error: variable index fuera de rango\n");
                    }
                    break;

                case OPCODE_RETURN:
                    if (debug) printf("[VM] RETURN\n");
                    vm.pc = vm.instruction_count;
                    break;

                case OPCODE_ARRAY_NEW:
                    if (vm.sp >= 2) {
                        double arr_size = vm.stack[--vm.sp];
                        double arr_type_val = vm.stack[--vm.sp];
                        
                        int size = (int)arr_size;
                        if (size > 0 && vm.array_count < 128) {
                            if (debug) printf("[VM] ARRAY_NEW size %d\n", size);
                        }
                    }
                    break;

                case OPCODE_ARRAY_SET:
                    if (vm.sp >= 3) {
                        double val = vm.stack[--vm.sp];
                        double idx = vm.stack[--vm.sp];
                        double arr_idx = vm.stack[--vm.sp];
                        
                        int array_index = (int)arr_idx;
                        int index = (int)idx;
                        
                        if (array_index >= 0 && array_index < vm.array_count) {
                            if (index >= 0 && index < vm.arrays[array_index].size) {
                                vm.arrays[array_index].data[index] = val;
                                if (debug) printf("[VM] ARRAY_SET array %d, index %d = %f\n", array_index, index, val);
                            }
                        }
                    }
                    break;

                case OPCODE_ARRAY_GET:
                    if (current.arg1 >= 0 && current.arg1 < vm.array_count) {
                        if (vm.sp > 0) {
                            int index = (int)vm.stack[vm.sp - 1];
                            if (index >= 0 && index < vm.arrays[current.arg1].size) {
                                vm.stack[vm.sp - 1] = vm.arrays[current.arg1].data[index];
                                if (debug) printf("[VM] ARRAY_GET array %d, index %d = %f\n", current.arg1, index, vm.arrays[current.arg1].data[index]);
                            }
                        }
                    }
                    break;

                case OPCODE_ARRAY_LEN:
                    if (current.arg1 >= 0 && current.arg1 < vm.array_count) {
                        vm.stack[vm.sp++] = vm.arrays[current.arg1].size;
                        if (debug) printf("[VM] ARRAY_LEN array %d = %d\n", current.arg1, vm.arrays[current.arg1].size);
                    }
                    break;

                case OPCODE_ARRAY_CLEAR:
                    if (current.arg1 >= 0 && current.arg1 < vm.array_count) {
                        for (int i = 0; i < vm.arrays[current.arg1].size; i++) {
                            vm.arrays[current.arg1].data[i] = 0;
                        }
                        if (vm.arrays[current.arg1].str_data) {
                            for (int i = 0; i < vm.arrays[current.arg1].size; i++) {
                                if (vm.arrays[current.arg1].str_data[i]) {
                                    free(vm.arrays[current.arg1].str_data[i]);
                                    vm.arrays[current.arg1].str_data[i] = NULL;
                                }
                            }
                        }
                        if (debug) printf("[VM] ARRAY_CLEAR array %d\n", current.arg1);
                    }
                    break;

                default:
                    if (debug) printf("[VM] Instrucción desconocida: 0x%02x\n", current.opcode);
                    break;
            }

            vm.pc++;
            executed++;
        }
    }

    if (debug) {
        printf("[VM] Execution completed\n");
        printf("[VM] Instructions executed: %d\n", executed);
    }
    
    // Cerrar ventana si está abierta
    if (window) {
        glfwDestroyWindow(window);
        glfwTerminate();
    }

    // Liberar memoria
    for (int i = 0; i < string_count; i++) {
        free(strings[i]);
    }
    if (strings) free(strings);
    
    for (int i = 0; i < var_count; i++) {
        free(variables[i].name);
    }
    if (variables) free(variables);
    
    for (int i = 0; i < class_count; i++) {
        free(classes[i].name);
        for (int j = 0; j < classes[i].var_count; j++) {
            free(classes[i].var_names[j]);
        }
        free(classes[i].var_names);
        free(classes[i].var_types);
        
        // Liberar métodos
        if (classes[i].methods) {
            for (int j = 0; j < classes[i].method_count; j++) {
                free(classes[i].methods[j].name);
            }
            free(classes[i].methods);
        }
    }
    if (classes) free(classes);
    
    // Liberar objetos
    for (int i = 0; i < vm.object_count; i++) {
        free(vm.objects[i].class_name);
        free(vm.objects[i].field_values);
    }
    if (vm.objects) free(vm.objects);
    
    free(instructions);

    return EXIT_SUCCESS;
}
