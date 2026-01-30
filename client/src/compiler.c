#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <dirent.h>
#include <sys/stat.h>
#include "compiler.h"
#include "utils.h"
#include "imports.h"
#include "config.h"

typedef struct {
    uint8_t opcode;
    uint8_t arg1;
    uint8_t arg2;
} Instruction;

typedef struct {
    char **strings;
    int count;
} StringPool;

static char* extract_string_from_printf(const char *line) {
    const char *start = strchr(line, '"');
    if (!start) return NULL;
    start++;

    const char *end = strchr(start, '"');
    if (!end) return NULL;

    int len = end - start;
    char *result = (char*)malloc(len + 1);
    if (!result) return NULL;

    strncpy(result, start, len);
    result[len] = '\0';

    return result;
}

static char* process_escape_sequences(const char *str) {
    int len = strlen(str);
    char *result = (char*)malloc(len + 1);
    if (!result) return NULL;

    int j = 0;
    for (int i = 0; i < len; i++) {
        if (str[i] == '\\' && i + 1 < len) {
            switch (str[i + 1]) {
                case 'n':
                    result[j++] = '\n';
                    i++;
                    break;
                case 't':
                    result[j++] = '\t';
                    i++;
                    break;
                case 'r':
                    result[j++] = '\r';
                    i++;
                    break;
                case '\\':
                    result[j++] = '\\';
                    i++;
                    break;
                case '"':
                    result[j++] = '"';
                    i++;
                    break;
                default:
                    result[j++] = str[i];
                    break;
            }
        } else {
            result[j++] = str[i];
        }
    }
    result[j] = '\0';
    return result;
}

static int add_string_to_pool(StringPool *pool, const char *str) {
    char **temp = realloc(pool->strings, (pool->count + 1) * sizeof(char*));
    if (!temp) return -1;

    pool->strings = temp;
    pool->strings[pool->count] = (char*)malloc(strlen(str) + 1);
    if (!pool->strings[pool->count]) return -1;

    strcpy(pool->strings[pool->count], str);
    return pool->count++;
}

static int add_variable_to_pool(VariablePool *pool, const char *name, char type, double value, const char *str_val) {
    GlobalVariable *temp = realloc(pool->vars, (pool->count + 1) * sizeof(GlobalVariable));
    if (!temp) return -1;

    pool->vars = temp;
    pool->vars[pool->count].name = (char*)malloc(strlen(name) + 1);
    if (!pool->vars[pool->count].name) return -1;

    strcpy(pool->vars[pool->count].name, name);
    pool->vars[pool->count].type = type;
    pool->vars[pool->count].value = value;
    pool->vars[pool->count].array_size = 0;
    pool->vars[pool->count].dynamic_array_size = 0;
    pool->vars[pool->count].array_element_type = '\0';
    
    if (type == 's' && str_val) {
        pool->vars[pool->count].str_val = (char*)malloc(strlen(str_val) + 1);
        if (!pool->vars[pool->count].str_val) return -1;
        strcpy(pool->vars[pool->count].str_val, str_val);
    } else {
        pool->vars[pool->count].str_val = NULL;
    }
    
    return pool->count++;
}

static int add_array_to_pool(VariablePool *pool, const char *name, char element_type, int size) {
    GlobalVariable *temp = realloc(pool->vars, (pool->count + 1) * sizeof(GlobalVariable));
    if (!temp) return -1;

    pool->vars = temp;
    pool->vars[pool->count].name = (char*)malloc(strlen(name) + 1);
    if (!pool->vars[pool->count].name) return -1;

    strcpy(pool->vars[pool->count].name, name);
    pool->vars[pool->count].type = 'a';  // 'a' para array
    pool->vars[pool->count].array_element_type = element_type;
    pool->vars[pool->count].array_size = size;
    pool->vars[pool->count].dynamic_array_size = 0;
    pool->vars[pool->count].value = 0;
    pool->vars[pool->count].str_val = NULL;
    
    return pool->count++;
}

static int extract_global_variables(const char *source_file, VariablePool *var_pool) {
    FILE *src = fopen(source_file, "r");
    if (!src) return 0;

    char line[512];
    int in_function = 0;
    
    while (fgets(line, sizeof(line), src)) {
        // Trim leading whitespace
        char *trimmed = line;
        while (*trimmed && (*trimmed == ' ' || *trimmed == '\t')) trimmed++;
        
        // Detectar entrada a función (int main() o similar)
        if (strstr(trimmed, "int main(") || strstr(trimmed, "int ") && strchr(trimmed, '{')) {
            in_function = 1;
            break;  // Dejar de procesar después de main()
        }
        
        // Buscar arrays dinámicos: int[] arr = new int[5]; PRIMERO (antes de arrays estáticos)
        if (strstr(line, "[] ") && strstr(line, "new ") && strstr(line, "[")) {
            char var_name[256] = {0};
            char size_str[256] = {0};
            
            // Extraer tipo: int, double, etc
            char element_type = 'i';
            if (strstr(line, "int[]")) element_type = 'i';
            else if (strstr(line, "double[]")) element_type = 'd';
            else if (strstr(line, "float[]")) element_type = 'd';
            
            // Extraer nombre: entre "[] " y " ="
            char *bracket_end = strstr(line, "[]");
            if (bracket_end) {
                bracket_end += 2;  // Skip "[]"
                while (*bracket_end && (*bracket_end == ' ' || *bracket_end == '\t')) bracket_end++;
                
                char *eq = strchr(bracket_end, '=');
                if (eq && eq > bracket_end) {
                    int name_len = eq - bracket_end;
                    while (name_len > 0 && (bracket_end[name_len-1] == ' ' || bracket_end[name_len-1] == '\t')) {
                        name_len--;
                    }
                    if (name_len > 0 && name_len < sizeof(var_name)) {
                        strncpy(var_name, bracket_end, name_len);
                        var_name[name_len] = '\0';
                        
                        // Extraer tamaño: entre [ y ]
                        char *size_bracket_open = strchr(eq, '[');
                        int dynamic_size = 5;  // Default
                        if (size_bracket_open) {
                            char *size_bracket_close = strchr(size_bracket_open, ']');
                            if (size_bracket_close && size_bracket_close > size_bracket_open) {
                                char *size_start = size_bracket_open + 1;
                                int size_len = size_bracket_close - size_start;
                                if (size_len > 0 && size_len < sizeof(size_str)) {
                                    strncpy(size_str, size_start, size_len);
                                    size_str[size_len] = '\0';
                                    dynamic_size = atoi(size_str);
                                }
                            }
                        }
                        
                        // Agregar variable dinámica al pool
                        GlobalVariable *temp = realloc(var_pool->vars, (var_pool->count + 1) * sizeof(GlobalVariable));
                        if (temp) {
                            var_pool->vars = temp;
                            var_pool->vars[var_pool->count].name = (char*)malloc(strlen(var_name) + 1);
                            if (var_pool->vars[var_pool->count].name) {
                                strcpy(var_pool->vars[var_pool->count].name, var_name);
                                var_pool->vars[var_pool->count].type = 'b';  // 'b' para array dinámico
                                var_pool->vars[var_pool->count].array_element_type = element_type;
                                var_pool->vars[var_pool->count].array_size = 0;  // Size en runtime
                                var_pool->vars[var_pool->count].dynamic_array_size = dynamic_size;  // Tamaño inicial
                                var_pool->vars[var_pool->count].value = 0;
                                var_pool->vars[var_pool->count].str_val = NULL;
                                var_pool->count++;
                            }
                        }
                    }
                }
            }
        }
        // Buscar arrays estáticos: int arr[10]; o double arr[5]; etc (SOLO antes de main)
        else if (strchr(line, '[') && strchr(line, ']') && strstr(line, ";") && !strstr(line, "new")) {
            char type_name[256];
            char arr_name[256];
            int arr_size = 0;
            
            // Determinar tipo - debe haber "int ", "double ", "float " al inicio
            char element_type = 'd';  // Default
            if (strstr(line, "int ")) element_type = 'i';
            else if (strstr(line, "double ")) element_type = 'd';
            else if (strstr(line, "float ")) element_type = 'd';
            else {
                // No es una declaración de tipo, saltar
                continue;
            }
            
            // Extraer nombre del array: entre espacio y [
            char *start = strchr(line, ' ');
            if (start) {
                start++;  // Skip espacio
                char *bracket = strchr(start, '[');
                if (bracket && bracket > start) {
                    int name_len = bracket - start;
                    if (name_len > 0 && name_len < sizeof(arr_name)) {
                        strncpy(arr_name, start, name_len);
                        arr_name[name_len] = '\0';
                        
                        // Extraer tamaño: entre [ y ]
                        char *size_start = bracket + 1;
                        char *size_end = strchr(size_start, ']');
                        if (size_end && size_end > size_start) {
                            int size_len = size_end - size_start;
                            char size_str[256];
                            strncpy(size_str, size_start, size_len);
                            size_str[size_len] = '\0';
                            arr_size = atoi(size_str);
                            
                            if (arr_size > 0) {
                                add_array_to_pool(var_pool, arr_name, element_type, arr_size);
                            }
                        }
                    }
                }
            }
        }
        // Buscar string = "valor";
        else if (strstr(line, "string") && strstr(line, "=") && strstr(line, "\"") && strstr(line, ";")) {
            char var_name[256];
            char str_val[512];
            
            // Extraer nombre: después de "string"
            char *start = strstr(line, "string");
            if (start) {
                start += 6;  // Skip "string"
                while (*start && (*start == ' ' || *start == '\t')) start++;
                
                char *end = strchr(start, '=');
                if (end && end > start) {
                    int len = end - start;
                    while (len > 0 && (start[len-1] == ' ' || start[len-1] == '\t')) len--;
                    if (len > 0 && len < sizeof(var_name)) {
                        strncpy(var_name, start, len);
                        var_name[len] = '\0';
                        
                        // Extraer string entre comillas
                        char *quote1 = strchr(line, '"');
                        if (quote1) {
                            quote1++;
                            char *quote2 = strchr(quote1, '"');
                            if (quote2 && quote2 > quote1) {
                                int str_len = quote2 - quote1;
                                if (str_len < sizeof(str_val)) {
                                    strncpy(str_val, quote1, str_len);
                                    str_val[str_len] = '\0';
                                    add_variable_to_pool(var_pool, var_name, 's', 0, str_val);
                                }
                            }
                        }
                    }
                }
            }
        }
        // Buscar líneas como: double PI = 3.14159...;
        else if ((strstr(line, "double") || strstr(line, "int") || strstr(line, "float")) && 
            strstr(line, "=") && strstr(line, ";")) {
            
            char var_name[256];
            double var_value = 0;
            char type_char = 'd';  // Default to double
            
            if (strstr(line, "int")) type_char = 'i';
            else if (strstr(line, "double")) type_char = 'd';
            else if (strstr(line, "float")) type_char = 'd';
            
            // Simple parsing: extraer nombre entre espacios y =
            char *start = strchr(line, ' ');
            if (start) {
                start++;  // Skip espacio
                char *end = strchr(start, ' ');
                if (!end) end = strchr(start, '=');
                
                if (end && end > start) {
                    int len = end - start;
                    if (len < sizeof(var_name)) {
                        strncpy(var_name, start, len);
                        var_name[len] = '\0';
                        
                        // Extraer valor
                        char *val_start = strchr(line, '=');
                        if (val_start) {
                            val_start++;  // Skip =
                            var_value = strtod(val_start, NULL);
                            add_variable_to_pool(var_pool, var_name, type_char, var_value, NULL);
                        }
                    }
                }
            }
        }
    }

    fclose(src);
    return var_pool->count;
}

static int get_class_index(const ClassPool *class_pool, const char *class_name) {
    for (int i = 0; i < class_pool->count; i++) {
        if (strcmp(class_pool->classes[i].name, class_name) == 0) {
            return i;
        }
    }
    return -1;
}

static int get_field_index(const ClassDefinition *cls, const char *field_name) {
    for (int i = 0; i < cls->var_count; i++) {
        if (strcmp(cls->var_names[i], field_name) == 0) {
            return i;
        }
    }
    return -1;
}

static int extract_classes(const char *source_file, ClassPool *class_pool) {
    FILE *src = fopen(source_file, "r");
    if (!src) return 0;

    char line[512];
    int in_class = 0;
    char class_name[256] = {0};
    
    while (fgets(line, sizeof(line), src)) {
        // Buscar definición de clase
        if (strstr(line, "class ") && strchr(line, '{')) {
            in_class = 1;
            // Extraer nombre de la clase
            char *start = strstr(line, "class ");
            if (start) {
                start += 6;  // Skip "class "
                char *end = strchr(start, '{');
                if (end) {
                    int len = end - start;
                    strncpy(class_name, start, len);
                    class_name[len] = '\0';
                    
                    // Trim whitespace
                    for (int i = 0; i < len; i++) {
                        if (class_name[i] == ' ' || class_name[i] == '\t') {
                            class_name[i] = '\0';
                            break;
                        }
                    }
                    
                    // Agregar clase
                    if (class_pool->count >= 256) continue;
                    
                    class_pool->classes = realloc(class_pool->classes, (class_pool->count + 1) * sizeof(ClassDefinition));
                    ClassDefinition *cls = &class_pool->classes[class_pool->count];
                    cls->name = malloc(strlen(class_name) + 1);
                    strcpy(cls->name, class_name);
                    cls->methods = NULL;
                    cls->method_count = 0;
                    cls->var_names = NULL;
                    cls->var_types = NULL;
                    cls->var_count = 0;
                    class_pool->count++;
                }
            }
        }
        else if (in_class && strchr(line, '}')) {
            in_class = 0;
        }
        else if (in_class && (strstr(line, "double") || strstr(line, "uint") || strstr(line, "int") || strstr(line, "char")) && !strstr(line, "(")) {
            // Agregar variable de instancia (no es método)
            if (class_pool->count > 0) {
                ClassDefinition *cls = &class_pool->classes[class_pool->count - 1];
                char var_name[256] = {0};
                
                char *start = strchr(line, ' ');
                if (start) {
                    start++;
                    char *end = strchr(start, ';');
                    if (!end) end = strchr(start, '=');
                    if (end) {
                        int len = end - start;
                        strncpy(var_name, start, len);
                        var_name[len] = '\0';
                        
                        cls->var_names = realloc(cls->var_names, (cls->var_count + 1) * sizeof(char*));
                        cls->var_types = realloc(cls->var_types, (cls->var_count + 1) * sizeof(uint8_t));
                        
                        cls->var_names[cls->var_count] = malloc(strlen(var_name) + 1);
                        strcpy(cls->var_names[cls->var_count], var_name);
                        
                        // Determinar tipo: primero verificar uint, luego int
                        if (strstr(line, "double")) {
                            cls->var_types[cls->var_count] = 'd';  // double
                        } else if (strstr(line, "uint")) {
                            cls->var_types[cls->var_count] = 'u';  // unsigned int
                        } else if (strstr(line, "int")) {
                            cls->var_types[cls->var_count] = 'i';  // signed int
                        } else {
                            cls->var_types[cls->var_count] = 'c';  // char
                        }
                        cls->var_count++;
                    }
                }
            }
        }
        else if (in_class && (strstr(line, "public") || strstr(line, "private")) && strchr(line, '(')) {
            // Agregar método de clase
            if (class_pool->count > 0) {
                ClassDefinition *cls = &class_pool->classes[class_pool->count - 1];
                char method_name[256] = {0};
                
                // Determinar si es público o privado
                uint8_t is_public = strstr(line, "public") ? 1 : 0;
                
                // Extraer nombre del método (entre nombre_tipo y paréntesis)
                char *paren = strchr(line, '(');
                if (paren) {
                    char *start = paren - 1;
                    // Retroceder para encontrar el inicio del nombre
                    while (start > line && *start == ' ') start--;
                    while (start > line && *start != ' ' && *start != '\t') start--;
                    if (*start == ' ' || *start == '\t') start++;
                    
                    int len = paren - start;
                    if (len > 0 && len < 256) {
                        strncpy(method_name, start, len);
                        method_name[len] = '\0';
                        
                        cls->methods = realloc(cls->methods, (cls->method_count + 1) * sizeof(ClassMethod));
                        ClassMethod *method = &cls->methods[cls->method_count];
                        method->name = malloc(strlen(method_name) + 1);
                        strcpy(method->name, method_name);
                        method->start_instruction = 0;  // Se asignará después
                        method->instruction_count = 0;
                        method->param_count = 0;  // Simplificado por ahora
                        method->is_public = is_public;
                        cls->method_count++;
                    }
                }
            }
        }
    }

    fclose(src);
    return class_pool->count;
}

static int compile_file_internal(const char *source_file, const char *project_dir, FILE *out, StringPool *string_pool, VariablePool *var_pool, int *instruction_count) {
    // Verificar si es un archivo .slibgld (librería compilada)
    if (strstr(source_file, ".slibgld")) {
        FILE *lib = fopen(source_file, "rb");
        if (!lib) {
            fprintf(stderr, "Error: No se puede abrir librería '%s'\n", source_file);
            return EXIT_FAILURE;
        }
        
        // Leer el archivo .slibgld y copiarlo al output
        unsigned char buffer[4096];
        size_t bytes;
        while ((bytes = fread(buffer, 1, sizeof(buffer), lib)) > 0) {
            fwrite(buffer, 1, bytes, out);
        }
        
        fclose(lib);
        return EXIT_SUCCESS;
    }

    FILE *src = fopen(source_file, "r");
    if (!src) {
        fprintf(stderr, "Error: No se puede abrir '%s'\n", source_file);
        return EXIT_FAILURE;
    }

    char line[512];

    // Procesar imports primero
    FileList *imports = extract_imports(source_file, project_dir);
    if (imports && imports->count > 0) {
        printf("  (importando %d libreria(s))\n", imports->count);
        for (int i = 0; i < imports->count; i++) {
            compile_file_internal(imports->files[i], project_dir, out, string_pool, var_pool, instruction_count);
        }
        free_file_list(imports);
    }

    rewind(src);
    StringPool temp_pool = {NULL, 0};
    
    while (fgets(line, sizeof(line), src)) {
        if ((strstr(line, "println") || strstr(line, "print") || strstr(line, "printf")) && !strstr(line, "import")) {
            char *str = extract_string_from_printf(line);
            if (str) {
                char *processed = process_escape_sequences(str);
                if (processed) {
                    add_string_to_pool(&temp_pool, processed);
                    free(processed);
                }
                free(str);
            }
        }
    }

    for (int i = 0; i < temp_pool.count; i++) {
        add_string_to_pool(string_pool, temp_pool.strings[i]);
    }

    rewind(src);
    int printf_index = string_pool->count - temp_pool.count;
    int local_printf_count = 0;
    int emitted_dynamic_arrays = 0;  // Bandera para emitir ARRAY_NEW solo una vez

    while (fgets(line, sizeof(line), src)) {
        // Trimear espacios iniciales
        char *trimmed = line;
        while (*trimmed && (*trimmed == ' ' || *trimmed == '\t')) trimmed++;
        
        // Omitir comentarios, líneas vacías e imports
        if (trimmed[0] == '/' || trimmed[0] == '\n' || strstr(trimmed, "import") || strstr(trimmed, "class ")) continue;

        // Emitir ARRAY_NEW para arrays dinámicos justo al inicio de main()
        if (!emitted_dynamic_arrays && var_pool != NULL && (strstr(trimmed, "int main(") || strstr(trimmed, "return"))) {
            emitted_dynamic_arrays = 1;
            
            // Emitir ARRAY_NEW para cada variable dinámica
            for (int i = 0; i < var_pool->count; i++) {
                if (var_pool->vars[i].type == 'b') {  // 'b' = dynamic array
                    Instruction instr = {0, 0, 0};
                    
                    // Emitir PUSH_VALUE con el tamaño almacenado
                    instr.opcode = 0x06;  // OPCODE_PUSH_VALUE
                    uint16_t size_pool_idx = string_pool->count;
                    if (string_pool->count < 1024) {
                        string_pool->strings = (char**)realloc(string_pool->strings, 
                                                                (string_pool->count + 1) * sizeof(char*));
                        string_pool->strings[string_pool->count] = malloc(32);
                        snprintf(string_pool->strings[string_pool->count], 32, "%d", var_pool->vars[i].dynamic_array_size);
                        string_pool->count++;
                    }
                    instr.arg1 = size_pool_idx;
                    instr.arg2 = 0;
                    (*instruction_count)++;
                    fwrite(&instr, sizeof(Instruction), 1, out);
                    
                    // ARRAY_NEW
                    instr.opcode = 0x0E;  // OPCODE_ARRAY_NEW
                    instr.arg1 = i;  // Índice en var_pool
                    instr.arg2 = var_pool->vars[i].array_element_type;  // Tipo de elemento
                    (*instruction_count)++;
                    fwrite(&instr, sizeof(Instruction), 1, out);
                }
            }
        }

        Instruction instr = {0, 0, 0};
        
        // Detectar arr.len (acceso a propiedad length del array) - pero NO dentro de println
        if (strchr(trimmed, '.') && strstr(trimmed, ".len") && !strstr(trimmed, "=") && !strstr(trimmed, "println")) {
            char arr_name[256] = {0};
            char *dot = strchr(trimmed, '.');
            if (dot && dot > trimmed) {
                int arr_len = dot - trimmed;
                if (arr_len > 0 && arr_len < sizeof(arr_name)) {
                    strncpy(arr_name, trimmed, arr_len);
                    arr_name[arr_len] = '\0';
                    
                    // Buscar el array en var_pool
                    int arr_idx = -1;
                    for (int i = 0; i < var_pool->count; i++) {
                        if (strcmp(var_pool->vars[i].name, arr_name) == 0 && 
                            (var_pool->vars[i].type == 'a' || var_pool->vars[i].type == 'b')) {
                            arr_idx = i;
                            break;
                        }
                    }
                    
                    if (arr_idx >= 0) {
                        // Emitir ARRAY_LEN
                        instr.opcode = 0x0F;  // OPCODE_ARRAY_LEN
                        instr.arg1 = arr_idx;  // Índice del array en var_pool
                        instr.arg2 = 0;
                        (*instruction_count)++;
                        fwrite(&instr, sizeof(Instruction), 1, out);
                    }
                }
            }
        }
        // Detectar arr.clear() (limpiar array)
        else if (strchr(trimmed, '.') && strstr(trimmed, ".clear()") && !strstr(trimmed, "println")) {
            char arr_name[256] = {0};
            char *dot = strchr(trimmed, '.');
            if (dot && dot > trimmed) {
                int arr_len = dot - trimmed;
                if (arr_len > 0 && arr_len < sizeof(arr_name)) {
                    strncpy(arr_name, trimmed, arr_len);
                    arr_name[arr_len] = '\0';
                    
                    // Buscar el array en var_pool
                    int arr_idx = -1;
                    for (int i = 0; i < var_pool->count; i++) {
                        if (strcmp(var_pool->vars[i].name, arr_name) == 0 && 
                            (var_pool->vars[i].type == 'a' || var_pool->vars[i].type == 'b')) {
                            arr_idx = i;
                            break;
                        }
                    }
                    
                    if (arr_idx >= 0) {
                        // Emitir ARRAY_CLEAR
                        instr.opcode = 0x10;  // OPCODE_ARRAY_CLEAR
                        instr.arg1 = arr_idx;  // Índice del array en var_pool
                        instr.arg2 = 0;
                        (*instruction_count)++;
                        fwrite(&instr, sizeof(Instruction), 1, out);
                    }
                }
            }
        }
        // Detectar acceso a campos: obj.field = valor
        else if (strchr(trimmed, '.') && strstr(trimmed, "=")) {
            // Parsear: objName.fieldName = valor;
            char obj_name[256] = {0};
            char field_name[256] = {0};
            char value_str[256] = {0};
            char *dot = strchr(trimmed, '.');
            if (dot) {
                // Extraer nombre del objeto (antes del punto)
                int obj_len = dot - trimmed;
                if (obj_len > 0 && obj_len < 256) {
                    strncpy(obj_name, trimmed, obj_len);
                    obj_name[obj_len] = '\0';
                    
                    // Extraer nombre del campo (después del punto)
                    dot++;
                    char *space_or_eq = strchr(dot, ' ');
                    if (!space_or_eq) space_or_eq = strchr(dot, '=');
                    if (space_or_eq) {
                        int field_len = space_or_eq - dot;
                        if (field_len > 0 && field_len < 256) {
                            strncpy(field_name, dot, field_len);
                            field_name[field_len] = '\0';
                            
                            // Extraer el valor después del =
                            char *eq = strchr(trimmed, '=');
                            if (eq) {
                                eq++;  // Skip '='
                                // Skip whitespace
                                while (*eq && (*eq == ' ' || *eq == '\t')) eq++;
                                
                                // Extraer valor hasta ; o fin de línea
                                char *val_end = eq;
                                while (*val_end && *val_end != ';' && *val_end != '\n') val_end++;
                                
                                // Trim trailing whitespace from value
                                val_end--;
                                while (val_end > eq && (*val_end == ' ' || *val_end == '\t')) val_end--;
                                val_end++;
                                
                                int val_len = val_end - eq;
                                if (val_len > 0 && val_len < 256) {
                                    strncpy(value_str, eq, val_len);
                                    value_str[val_len] = '\0';
                                    
                                    // Si es una cadena entre comillas, remover las comillas
                                    char processed_value[256] = {0};
                                    int proc_len = val_len;
                                    
                                    if ((value_str[0] == '"' && value_str[val_len-1] == '"') ||
                                        (value_str[0] == '\'' && value_str[val_len-1] == '\'')) {
                                        // Es un string literal, remover comillas
                                        strncpy(processed_value, value_str + 1, val_len - 2);
                                        processed_value[val_len - 2] = '\0';
                                        proc_len = val_len - 2;
                                    } else {
                                        // No es string, usar como está
                                        strcpy(processed_value, value_str);
                                    }
                                    
                                    // Generar PUSH_VALUE con el valor procesado
                                    uint16_t string_idx = string_pool->count;
                                    if (string_pool->count < 1024) {
                                        string_pool->strings = (char**)realloc(string_pool->strings, 
                                                                                (string_pool->count + 1) * sizeof(char*));
                                        string_pool->strings[string_pool->count] = malloc(proc_len + 1);
                                        strcpy(string_pool->strings[string_pool->count], processed_value);
                                        string_pool->count++;
                                    }
                                    
                                    instr.opcode = 0x06;  // OPCODE_PUSH_VALUE
                                    instr.arg1 = string_idx;
                                    instr.arg2 = 0;
                                    (*instruction_count)++;
                                    fwrite(&instr, sizeof(Instruction), 1, out);
                                    
                                    // Generar instrucción SET_FIELD
                                    instr.opcode = 0x05;  // OPCODE_SET_FIELD
                                    instr.arg1 = 0;  // field index (simplificado)
                                    instr.arg2 = 0;  // object index (simplificado)
                                    (*instruction_count)++;
                                    fwrite(&instr, sizeof(Instruction), 1, out);
                                }
                            }
                        }
                    }
                }
            }
        } else if (strchr(trimmed, '[') && strchr(trimmed, ']') && strstr(trimmed, "=")) {
            // Parsear asignación a array: arr[index] = value;
            char arr_name[256] = {0};
            char index_str[256] = {0};
            char value_str[256] = {0};
            
            char *bracket_open = strchr(trimmed, '[');
            if (bracket_open && bracket_open > trimmed) {
                int arr_name_len = bracket_open - trimmed;
                if (arr_name_len > 0 && arr_name_len < sizeof(arr_name)) {
                    strncpy(arr_name, trimmed, arr_name_len);
                    arr_name[arr_name_len] = '\0';
                    
                    // Extraer índice entre [ y ]
                    char *bracket_close = strchr(bracket_open, ']');
                    if (bracket_close && bracket_close > bracket_open) {
                        char *index_start = bracket_open + 1;
                        int index_len = bracket_close - index_start;
                        if (index_len > 0 && index_len < sizeof(index_str)) {
                            strncpy(index_str, index_start, index_len);
                            index_str[index_len] = '\0';
                            
                            // Extraer valor después del =
                            char *eq = strchr(bracket_close, '=');
                            if (eq) {
                                eq++;  // Skip '='
                                while (*eq && (*eq == ' ' || *eq == '\t')) eq++;
                                
                                char *val_end = eq;
                                while (*val_end && *val_end != ';' && *val_end != '\n') val_end++;
                                val_end--;
                                while (val_end > eq && (*val_end == ' ' || *val_end == '\t')) val_end--;
                                val_end++;
                                
                                int val_len = val_end - eq;
                                if (val_len > 0 && val_len < sizeof(value_str)) {
                                    strncpy(value_str, eq, val_len);
                                    value_str[val_len] = '\0';
                                    
                                    // Buscar el array en el pool
                                    int arr_idx = -1;
                                    for (int i = 0; i < var_pool->count; i++) {
                                        if (strcmp(var_pool->vars[i].name, arr_name) == 0 && 
                                            (var_pool->vars[i].type == 'a' || var_pool->vars[i].type == 'b')) {
                                            arr_idx = i;
                                            break;
                                        }
                                    }
                                    
                                    if (arr_idx >= 0) {
                                        // Emitir PUSH_VALUE con el índice
                                        instr.opcode = 0x06;  // OPCODE_PUSH_VALUE
                                        // Convertir index_str a índice
                                        int idx_val = atoi(index_str);
                                        // Almacenar en string pool para reutilizar PUSH_VALUE
                                        uint16_t idx_pool = string_pool->count;
                                        if (string_pool->count < 1024) {
                                            string_pool->strings = (char**)realloc(string_pool->strings, 
                                                                                    (string_pool->count + 1) * sizeof(char*));
                                            string_pool->strings[string_pool->count] = malloc(strlen(index_str) + 1);
                                            strcpy(string_pool->strings[string_pool->count], index_str);
                                            string_pool->count++;
                                        }
                                        instr.arg1 = idx_pool;
                                        instr.arg2 = 0;
                                        (*instruction_count)++;
                                        fwrite(&instr, sizeof(Instruction), 1, out);
                                        
                                        // Emitir PUSH_VALUE con el valor
                                        instr.opcode = 0x06;  // OPCODE_PUSH_VALUE
                                        uint16_t val_pool = string_pool->count;
                                        if (string_pool->count < 1024) {
                                            string_pool->strings = (char**)realloc(string_pool->strings, 
                                                                                    (string_pool->count + 1) * sizeof(char*));
                                            string_pool->strings[string_pool->count] = malloc(strlen(value_str) + 1);
                                            strcpy(string_pool->strings[string_pool->count], value_str);
                                            string_pool->count++;
                                        }
                                        instr.arg1 = val_pool;
                                        instr.arg2 = 0;
                                        (*instruction_count)++;
                                        fwrite(&instr, sizeof(Instruction), 1, out);
                                        
                                        // Emitir ARRAY_SET
                                        instr.opcode = 0x0C;  // OPCODE_ARRAY_SET
                                        instr.arg1 = arr_idx;  // Índice del array en var_pool
                                        instr.arg2 = 0;
                                        (*instruction_count)++;
                                        fwrite(&instr, sizeof(Instruction), 1, out);
                                    }
                                }
                            }
                        }
                    }
                }
            }
        } else if (strchr(trimmed, '[') && strchr(trimmed, ']') && !strstr(trimmed, "=") && !strstr(trimmed, "println") && !strstr(trimmed, "print")) {
            // Parsear lectura de array: arr[index] sin asignación (pero NO si es println/print)
            // Por ahora, generar ARRAY_GET
            char arr_name[256] = {0};
            char index_str[256] = {0};
            
            char *bracket_open = strchr(trimmed, '[');
            if (bracket_open && bracket_open > trimmed) {
                int arr_name_len = bracket_open - trimmed;
                if (arr_name_len > 0 && arr_name_len < sizeof(arr_name)) {
                    strncpy(arr_name, trimmed, arr_name_len);
                    arr_name[arr_name_len] = '\0';
                    
                    char *bracket_close = strchr(bracket_open, ']');
                    if (bracket_close && bracket_close > bracket_open) {
                        char *index_start = bracket_open + 1;
                        int index_len = bracket_close - index_start;
                        if (index_len > 0 && index_len < sizeof(index_str)) {
                            strncpy(index_str, index_start, index_len);
                            index_str[index_len] = '\0';
                            
                            int arr_idx = -1;
                            for (int i = 0; i < var_pool->count; i++) {
                                if (strcmp(var_pool->vars[i].name, arr_name) == 0 && 
                                    (var_pool->vars[i].type == 'a' || var_pool->vars[i].type == 'b')) {
                                    arr_idx = i;
                                    break;
                                }
                            }
                            
                            if (arr_idx >= 0) {
                                // Emitir PUSH_VALUE con índice
                                instr.opcode = 0x06;  // OPCODE_PUSH_VALUE
                                uint16_t idx_pool = string_pool->count;
                                if (string_pool->count < 1024) {
                                    string_pool->strings = (char**)realloc(string_pool->strings, 
                                                                            (string_pool->count + 1) * sizeof(char*));
                                    string_pool->strings[string_pool->count] = malloc(strlen(index_str) + 1);
                                    strcpy(string_pool->strings[string_pool->count], index_str);
                                    string_pool->count++;
                                }
                                instr.arg1 = idx_pool;
                                instr.arg2 = 0;
                                (*instruction_count)++;
                                fwrite(&instr, sizeof(Instruction), 1, out);
                                
                                // Emitir ARRAY_GET
                                instr.opcode = 0x0D;  // OPCODE_ARRAY_GET
                                instr.arg1 = arr_idx;
                                instr.arg2 = 0;
                                (*instruction_count)++;
                                fwrite(&instr, sizeof(Instruction), 1, out);
                            }
                        }
                    }
                }
            }
        } else if (strstr(trimmed, "printchr(")) {
            // Optimizado: printchr(char) - imprime un carácter directamente sin pasar por string pool
            instr.opcode = 0x09;
            char *start = strstr(trimmed, "printchr(");
            if (start) {
                start += 9;  // Skip "printchr("
                // Buscar el carácter entre comillas
                char *quote1 = strchr(start, '\'');
                if (quote1) {
                    quote1++;
                    char c = *quote1;
                    instr.arg1 = (uint8_t)c;  // Almacenar el ASCII directamente en arg1
                    (*instruction_count)++;
                    fwrite(&instr, sizeof(Instruction), 1, out);
                }
            }
        } else if (strstr(trimmed, "println")) {
            // Detectar si es println(variable) o println("string")
            char *start = strstr(trimmed, "println(");
            if (start) {
                start += 8;
                char *end = strchr(start, ')');
                if (end) {
                    int arg_len = end - start;
                    char arg[256] = {0};
                    strncpy(arg, start, arg_len);
                    arg[arg_len] = '\0';
                    
                    // Trimear espacios
                    char *trimmed_arg = arg;
                    while (*trimmed_arg && (*trimmed_arg == ' ' || *trimmed_arg == '\t')) trimmed_arg++;
                    
                    // Detectar si es acceso a array: arr[index]
                    if (strchr(trimmed_arg, '[') && strchr(trimmed_arg, ']')) {
                        // Parsear acceso a array
                        char arr_name[256] = {0};
                        char index_str[256] = {0};
                        
                        char *bracket_open = strchr(trimmed_arg, '[');
                        if (bracket_open && bracket_open > trimmed_arg) {
                            int arr_name_len = bracket_open - trimmed_arg;
                            if (arr_name_len > 0 && arr_name_len < sizeof(arr_name)) {
                                strncpy(arr_name, trimmed_arg, arr_name_len);
                                arr_name[arr_name_len] = '\0';
                                
                                char *bracket_close = strchr(bracket_open, ']');
                                if (bracket_close && bracket_close > bracket_open) {
                                    char *index_start = bracket_open + 1;
                                    int index_len = bracket_close - index_start;
                                    if (index_len > 0 && index_len < sizeof(index_str)) {
                                        strncpy(index_str, index_start, index_len);
                                        index_str[index_len] = '\0';
                                        
                                        int arr_idx = -1;
                                        for (int i = 0; i < var_pool->count; i++) {
                                            if (strcmp(var_pool->vars[i].name, arr_name) == 0 && 
                                                (var_pool->vars[i].type == 'a' || var_pool->vars[i].type == 'b')) {
                                                arr_idx = i;
                                                break;
                                            }
                                        }
                                        
                                        if (arr_idx >= 0) {
                                            // Emitir PUSH_VALUE con índice
                                            instr.opcode = 0x06;  // OPCODE_PUSH_VALUE
                                            uint16_t idx_pool = string_pool->count;
                                            if (string_pool->count < 1024) {
                                                string_pool->strings = (char**)realloc(string_pool->strings, 
                                                                                        (string_pool->count + 1) * sizeof(char*));
                                                string_pool->strings[string_pool->count] = malloc(strlen(index_str) + 1);
                                                strcpy(string_pool->strings[string_pool->count], index_str);
                                                string_pool->count++;
                                            }
                                            instr.arg1 = idx_pool;
                                            instr.arg2 = 0;
                                            (*instruction_count)++;
                                            fwrite(&instr, sizeof(Instruction), 1, out);
                                            
                                            // Emitir ARRAY_GET
                                            instr.opcode = 0x0D;  // OPCODE_ARRAY_GET
                                            instr.arg1 = arr_idx;
                                            instr.arg2 = 0;
                                            (*instruction_count)++;
                                            fwrite(&instr, sizeof(Instruction), 1, out);
                                            
                                            // Emitir PRINTLN (que imprimirá el valor del top del stack)
                                            instr.opcode = 0x08;  // PRINTLN
                                            instr.arg1 = 0;
                                            (*instruction_count)++;
                                            fwrite(&instr, sizeof(Instruction), 1, out);
                                            
                                            // Ya procesamos este array, saltar al final
                                            goto println_done;
                                        }
                                    }
                                }
                            }
                        }
                    } else if (strchr(trimmed_arg, '.') && strstr(trimmed_arg, ".len")) {
                        // Detectar si es arr.len (acceso a propiedad length)
                        char arr_name[256] = {0};
                        char *dot = strchr(trimmed_arg, '.');
                        if (dot && dot > trimmed_arg) {
                            int arr_len = dot - trimmed_arg;
                            if (arr_len > 0 && arr_len < sizeof(arr_name)) {
                                strncpy(arr_name, trimmed_arg, arr_len);
                                arr_name[arr_len] = '\0';
                                
                                // Buscar el array en var_pool
                                int arr_idx = -1;
                                for (int i = 0; i < var_pool->count; i++) {
                                    if (strcmp(var_pool->vars[i].name, arr_name) == 0 && 
                                        (var_pool->vars[i].type == 'a' || var_pool->vars[i].type == 'b')) {
                                        arr_idx = i;
                                        break;
                                    }
                                }
                                
                                if (arr_idx >= 0) {
                                    // Emitir ARRAY_LEN
                                    instr.opcode = 0x0F;  // OPCODE_ARRAY_LEN
                                    instr.arg1 = arr_idx;
                                    instr.arg2 = 0;
                                    (*instruction_count)++;
                                    fwrite(&instr, sizeof(Instruction), 1, out);
                                    
                                    // Emitir PRINTLN
                                    instr.opcode = 0x08;  // PRINTLN
                                    instr.arg1 = 0;
                                    (*instruction_count)++;
                                    fwrite(&instr, sizeof(Instruction), 1, out);
                                    
                                    goto println_done;
                                }
                            }
                        }
                    }
                    
                    // Si es variable (no empieza con comilla)
                    if (var_pool && trimmed_arg[0] != '"' && trimmed_arg[0] != '\'') {
                        // Buscar en variables globales
                        int var_idx = -1;
                        for (int i = 0; i < var_pool->count; i++) {
                            if (strncmp(var_pool->vars[i].name, trimmed_arg, strlen(trimmed_arg)) == 0) {
                                var_idx = i;
                                break;
                            }
                        }
                        
                        if (var_idx >= 0 && var_pool->vars[var_idx].type == 's') {
                            // Emitir GET_GLOBAL seguido de PRINTLN
                            instr.opcode = 0x0A;  // GET_GLOBAL
                            instr.arg1 = var_idx;
                            (*instruction_count)++;
                            fwrite(&instr, sizeof(Instruction), 1, out);
                            
                            instr.opcode = 0x08;  // PRINTLN
                            instr.arg1 = 0;  // Dummy, se ignora
                            (*instruction_count)++;
                            fwrite(&instr, sizeof(Instruction), 1, out);
                        } else {
                            // Variable no encontrada o no es string, emitir normal
                            instr.opcode = 0x08;
                            instr.arg1 = printf_index + local_printf_count;
                            local_printf_count++;
                            (*instruction_count)++;
                            fwrite(&instr, sizeof(Instruction), 1, out);
                        }
                    } else {
                        // Es un string literal
                        instr.opcode = 0x08;
                        instr.arg1 = printf_index + local_printf_count;
                        local_printf_count++;
                        (*instruction_count)++;
                        fwrite(&instr, sizeof(Instruction), 1, out);
                    }
                    println_done:
                } else {
                    instr.opcode = 0x08;
                    instr.arg1 = printf_index + local_printf_count;
                    local_printf_count++;
                    (*instruction_count)++;
                    fwrite(&instr, sizeof(Instruction), 1, out);
                }
            } else {
                instr.opcode = 0x08;
                instr.arg1 = printf_index + local_printf_count;
                local_printf_count++;
                (*instruction_count)++;
                fwrite(&instr, sizeof(Instruction), 1, out);
            }
        } else if (strstr(trimmed, "print") || strstr(trimmed, "printf")) {
            instr.opcode = 0x01;
            instr.arg1 = printf_index + local_printf_count;
            local_printf_count++;
            (*instruction_count)++;
            fwrite(&instr, sizeof(Instruction), 1, out);
        } else if (strstr(trimmed, "[] ") && strstr(trimmed, "new ") && strstr(trimmed, "[")) {
            // Parsear asignación de array dinámico: int[] arr = new int[size];
            char var_name[256] = {0};
            char elem_type[256] = {0};
            char size_str[256] = {0};
            
            // Extraer tipo: int, double, etc
            char element_type = 'i';
            if (strstr(trimmed, "int[]")) element_type = 'i';
            else if (strstr(trimmed, "double[]")) element_type = 'd';
            else if (strstr(trimmed, "float[]")) element_type = 'd';
            
            // Extraer nombre: entre "[] " y " ="
            char *bracket_end = strstr(trimmed, "[]");
            if (bracket_end) {
                bracket_end += 2;  // Skip "[]"
                while (*bracket_end && (*bracket_end == ' ' || *bracket_end == '\t')) bracket_end++;
                
                char *eq = strchr(bracket_end, '=');
                if (eq && eq > bracket_end) {
                    int name_len = eq - bracket_end;
                    while (name_len > 0 && (bracket_end[name_len-1] == ' ' || bracket_end[name_len-1] == '\t')) {
                        name_len--;
                    }
                    if (name_len > 0 && name_len < sizeof(var_name)) {
                        strncpy(var_name, bracket_end, name_len);
                        var_name[name_len] = '\0';
                        
                        // Extraer tamaño: entre [ y ]
                        char *size_bracket_open = strchr(eq, '[');
                        if (size_bracket_open) {
                            char *size_bracket_close = strchr(size_bracket_open, ']');
                            if (size_bracket_close && size_bracket_close > size_bracket_open) {
                                char *size_start = size_bracket_open + 1;
                                int size_len = size_bracket_close - size_start;
                                if (size_len > 0 && size_len < sizeof(size_str)) {
                                    strncpy(size_str, size_start, size_len);
                                    size_str[size_len] = '\0';
                                    
                                    // Agregar la variable dinámica al pool
                                    GlobalVariable *temp = realloc(var_pool->vars, (var_pool->count + 1) * sizeof(GlobalVariable));
                                    if (temp) {
                                        var_pool->vars = temp;
                                        var_pool->vars[var_pool->count].name = (char*)malloc(strlen(var_name) + 1);
                                        if (var_pool->vars[var_pool->count].name) {
                                            strcpy(var_pool->vars[var_pool->count].name, var_name);
                                            var_pool->vars[var_pool->count].type = 'b';  // 'b' para array dinámico
                                            var_pool->vars[var_pool->count].array_element_type = element_type;
                                            var_pool->vars[var_pool->count].array_size = 0;  // Size será determinado en runtime
                                            var_pool->vars[var_pool->count].value = 0;
                                            var_pool->vars[var_pool->count].str_val = NULL;
                                            int var_idx = var_pool->count;
                                            var_pool->count++;
                                            
                                            // Emitir PUSH_VALUE con el tamaño
                                            instr.opcode = 0x06;  // OPCODE_PUSH_VALUE
                                            uint16_t size_pool_idx = string_pool->count;
                                            if (string_pool->count < 1024) {
                                                string_pool->strings = (char**)realloc(string_pool->strings, 
                                                                                        (string_pool->count + 1) * sizeof(char*));
                                                string_pool->strings[string_pool->count] = malloc(strlen(size_str) + 1);
                                                strcpy(string_pool->strings[string_pool->count], size_str);
                                                string_pool->count++;
                                            }
                                            instr.arg1 = size_pool_idx;
                                            instr.arg2 = 0;
                                            (*instruction_count)++;
                                            fwrite(&instr, sizeof(Instruction), 1, out);
                                            
                                            // Emitir ARRAY_NEW con el índice de la variable
                                            instr.opcode = 0x0E;  // OPCODE_ARRAY_NEW
                                            instr.arg1 = var_idx;  // Índice en var_pool
                                            instr.arg2 = element_type;  // Tipo de elemento
                                            (*instruction_count)++;
                                            fwrite(&instr, sizeof(Instruction), 1, out);
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        } else if (strstr(trimmed, "new ")) {
            // Parsear: ClassName obj = new ClassName();
            char class_name[256] = {0};
            char *start = strstr(trimmed, "new ");
            if (start) {
                start += 4;  // Skip "new "
                // Skip whitespace
                while (*start && (*start == ' ' || *start == '\t')) start++;
                
                char *end = start;
                while (*end && *end != '(' && *end != ';' && *end != ' ') end++;
                
                if (end > start) {
                    int len = end - start;
                    strncpy(class_name, start, len);
                    class_name[len] = '\0';
                    
                    // Generar instrucción NEW_INSTANCE
                    instr.opcode = 0x02;  // OPCODE_NEW_INSTANCE
                    // arg1 será el índice de la clase (0 por ahora, se resolvería mejor)
                    (*instruction_count)++;
                    fwrite(&instr, sizeof(Instruction), 1, out);
                }
            }
        } else if (strstr(trimmed, "return")) {
            instr.opcode = 0xFF;
            (*instruction_count)++;
            fwrite(&instr, sizeof(Instruction), 1, out);
        }
    }

    fclose(src);

    for (int i = 0; i < temp_pool.count; i++) {
        free(temp_pool.strings[i]);
    }
    free(temp_pool.strings);

    return EXIT_SUCCESS;
}

static int compile_file(const char *source_file, const char *project_dir, StringPool *string_pool) {
    // Leer configuración del proyecto
    ProjectConfig *config = read_project_config(project_dir);
    if (!config) {
        fprintf(stderr, "Error: No se puede leer configuración del proyecto\n");
        return EXIT_FAILURE;
    }

    char filename_only[256];
    const char *base = strrchr(source_file, '/');
    if (base) {
        base++;
    } else {
        base = source_file;
    }
    strncpy(filename_only, base, sizeof(filename_only) - 1);
    
    char *dot = strrchr(filename_only, '.');
    if (dot) {
        *dot = '\0';
    }
    
    // Determinar extensión según tipo de proyecto
    const char *extension = ".gld";  // Por defecto para ejecutables
    if (strcmp(config->type, "static_lib") == 0) {
        extension = ".slibgld";
    } else if (strcmp(config->type, "dynamic_lib") == 0) {
        extension = ".dlibgld";
    }
    
    char output_file[256];
    snprintf(output_file, sizeof(output_file), "%s/%s%s", project_dir, filename_only, extension);

    // Primero recopilar todo
    StringPool temp_pool = {NULL, 0};
    int instruction_count = 0;

    FILE *temp = tmpfile();
    if (!temp) {
        fprintf(stderr, "Error: No se puede crear archivo temporal\n");
        free_project_config(config);
        return EXIT_FAILURE;
    }

    // Compilar archivo con imports recursivos a archivo temporal
    compile_file_internal(source_file, project_dir, temp, &temp_pool, NULL, &instruction_count);

    // Ahora escribir correctamente al archivo final
    FILE *out = fopen(output_file, "wb");
    if (!out) {
        fprintf(stderr, "Error: No se puede crear '%s'\n", output_file);
        fclose(temp);
        free_project_config(config);
        return EXIT_FAILURE;
    }

    // Header
    fprintf(out, "GOLD");
    uint8_t version = 1;
    fwrite(&version, 1, 1, out);

    // Cantidad de strings
    uint16_t string_count = temp_pool.count;
    fwrite(&string_count, sizeof(uint16_t), 1, out);

    // Strings
    for (int i = 0; i < temp_pool.count; i++) {
        uint16_t str_len = strlen(temp_pool.strings[i]);
        fwrite(&str_len, sizeof(uint16_t), 1, out);
        fwrite(temp_pool.strings[i], 1, str_len, out);
    }

    // Instrucciones desde archivo temporal
    rewind(temp);
    char buffer[sizeof(Instruction)];
    while (fread(buffer, sizeof(Instruction), 1, temp)) {
        fwrite(buffer, sizeof(Instruction), 1, out);
    }

    fclose(temp);
    fclose(out);

    printf("  ✓ %s -> %s (%d instrucciones, %d strings)\n", source_file, output_file, instruction_count, string_count);

    for (int i = 0; i < temp_pool.count; i++) {
        free(temp_pool.strings[i]);
    }
    free(temp_pool.strings);
    free_project_config(config);

    return EXIT_SUCCESS;
}

int build_project(const char *project_dir) {
    printf("Compiling project '%s'...\n", project_dir);

    // Leer configuración del proyecto
    ProjectConfig *config = read_project_config(project_dir);
    if (!config) {
        fprintf(stderr, "Error: No se puede leer configuración del proyecto\n");
        return EXIT_FAILURE;
    }

    char src_dir[256];
    snprintf(src_dir, sizeof(src_dir), "%s/src", project_dir);

    DIR *dir = opendir(src_dir);
    if (!dir) {
        fprintf(stderr, "Error: No se puede abrir directorio '%s'\n", src_dir);
        free_project_config(config);
        return EXIT_FAILURE;
    }

    // Recopilar todos los archivos .gsf y variables globales
    struct dirent *entry;
    int file_count = 0;
    char **files = NULL;
    char *main_file = NULL;
    VariablePool var_pool = {NULL, 0};
    ClassPool class_pool = {NULL, 0};

    while ((entry = readdir(dir)) != NULL) {
        if (strstr(entry->d_name, ".gsf")) {
            char full_path[256];
            snprintf(full_path, sizeof(full_path), "%s/%s", src_dir, entry->d_name);
            
            // Extraer variables globales
            extract_global_variables(full_path, &var_pool);
            
            // Extraer clases
            extract_classes(full_path, &class_pool);
            
            char **temp = realloc(files, (file_count + 1) * sizeof(char*));
            if (!temp) continue;
            
            files = temp;
            files[file_count] = (char*)malloc(strlen(full_path) + 1);
            if (files[file_count]) {
                strcpy(files[file_count], full_path);
                
                // Identificar archivo main
                if (strcmp(entry->d_name, "main.gsf") == 0) {
                    main_file = files[file_count];
                }
                file_count++;
            }
        }
    }
    closedir(dir);

    if (file_count == 0) {
        fprintf(stderr, "✗ No se encontraron archivos .gsf\n");
        free_project_config(config);
        return EXIT_FAILURE;
    }

    // Compilar todos los archivos a un único bytecode
    StringPool combined_pool = {NULL, 0};
    int total_instructions = 0;
    FILE *temp = tmpfile();
    if (!temp) {
        fprintf(stderr, "Error: No se puede crear archivo temporal\n");
        free_project_config(config);
        return EXIT_FAILURE;
    }

    // Inyectar strings globales en el pool (optimización para variables string nativas)
    for (int i = 0; i < var_pool.count; i++) {
        if (var_pool.vars[i].type == 's' && var_pool.vars[i].str_val) {
            add_string_to_pool(&combined_pool, var_pool.vars[i].str_val);
        }
    }

    // Empezar por main.gsf si existe
    if (main_file) {
        compile_file_internal(main_file, project_dir, temp, &combined_pool, &var_pool, &total_instructions);
    }

    // Compilar el resto
    for (int i = 0; i < file_count; i++) {
        if (files[i] != main_file) {
            compile_file_internal(files[i], project_dir, temp, &combined_pool, &var_pool, &total_instructions);
        }
    }

    // Determinar extensión según tipo de proyecto
    const char *extension = ".gld";  // Por defecto para ejecutables
    if (strcmp(config->type, "static_lib") == 0) {
        extension = ".slibgld";
    } else if (strcmp(config->type, "dynamic_lib") == 0) {
        extension = ".dlibgld";
    }

    char output_file[256];
    snprintf(output_file, sizeof(output_file), "%s/%s%s", project_dir, config->name[0] ? config->name : "output", extension);

    // Escribir archivo final
    FILE *out = fopen(output_file, "wb");
    if (!out) {
        fprintf(stderr, "Error: No se puede crear '%s'\n", output_file);
        fclose(temp);
        free_project_config(config);
        return EXIT_FAILURE;
    }

    // Header
    fprintf(out, "GOLD");
    uint8_t version = 1;
    fwrite(&version, 1, 1, out);

    // Escribir configuración de ventana
    uint8_t window_title_len = strlen(config->window_title);
    fwrite(&window_title_len, 1, 1, out);
    fwrite(config->window_title, 1, window_title_len, out);
    
    uint16_t window_width = config->window_width;
    fwrite(&window_width, sizeof(uint16_t), 1, out);
    
    uint16_t window_height = config->window_height;
    fwrite(&window_height, sizeof(uint16_t), 1, out);
    
    uint8_t window_resizable = config->window_resizable;
    fwrite(&window_resizable, 1, 1, out);
    
    uint8_t window_mode_len = strlen(config->window_mode);
    fwrite(&window_mode_len, 1, 1, out);
    fwrite(config->window_mode, 1, window_mode_len, out);

    // Escribir renderer
    uint8_t renderer_len = strlen(config->renderer);
    fwrite(&renderer_len, 1, 1, out);
    fwrite(config->renderer, 1, renderer_len, out);

    // Escribir fps
    uint16_t fps = config->fps;
    fwrite(&fps, sizeof(uint16_t), 1, out);

    // Cantidad de variables globales
    uint16_t var_count = var_pool.count;
    fwrite(&var_count, sizeof(uint16_t), 1, out);

    // Variables globales (optimizado con tipo)
    for (int i = 0; i < var_pool.count; i++) {
        uint8_t name_len = strlen(var_pool.vars[i].name);
        fwrite(&name_len, 1, 1, out);
        fwrite(var_pool.vars[i].name, 1, name_len, out);
        
        // Escribir tipo de variable
        uint8_t var_type = var_pool.vars[i].type;
        fwrite(&var_type, 1, 1, out);
        
        // Escribir valor según tipo
        if (var_type == 's') {
            // String: escribir longitud + contenido
            uint16_t str_len = strlen(var_pool.vars[i].str_val);
            fwrite(&str_len, sizeof(uint16_t), 1, out);
            fwrite(var_pool.vars[i].str_val, 1, str_len, out);
        } else if (var_type == 'a') {
            // Array estático: escribir tipo de elemento y tamaño
            fwrite(&var_pool.vars[i].array_element_type, 1, 1, out);
            fwrite(&var_pool.vars[i].array_size, sizeof(int), 1, out);
        } else if (var_type == 'b') {
            // Array dinámico: escribir tipo de elemento (tamaño es 0)
            fwrite(&var_pool.vars[i].array_element_type, 1, 1, out);
        } else {
            // Numeric: escribir double
            fwrite(&var_pool.vars[i].value, sizeof(double), 1, out);
        }
    }

    // Cantidad de clases
    uint16_t class_count = class_pool.count;
    fwrite(&class_count, sizeof(uint16_t), 1, out);

    // Escribir clases
    for (int i = 0; i < class_pool.count; i++) {
        ClassDefinition *cls = &class_pool.classes[i];
        
        // Nombre de clase
        uint8_t name_len = strlen(cls->name);
        fwrite(&name_len, 1, 1, out);
        fwrite(cls->name, 1, name_len, out);
        
        // Cantidad de variables de instancia
        uint8_t ivar_count = cls->var_count;
        fwrite(&ivar_count, 1, 1, out);
        
        // Variables de instancia
        for (int j = 0; j < cls->var_count; j++) {
            uint8_t var_name_len = strlen(cls->var_names[j]);
            fwrite(&var_name_len, 1, 1, out);
            fwrite(cls->var_names[j], 1, var_name_len, out);
            fwrite(&cls->var_types[j], 1, 1, out);
        }
        
        // Cantidad de métodos
        uint8_t method_count = cls->method_count;
        fwrite(&method_count, 1, 1, out);
        
        // Métodos
        for (int j = 0; j < cls->method_count; j++) {
            ClassMethod *method = &cls->methods[j];
            
            // Nombre del método
            uint8_t method_name_len = strlen(method->name);
            fwrite(&method_name_len, 1, 1, out);
            fwrite(method->name, 1, method_name_len, out);
            
            // Información del método
            fwrite(&method->is_public, 1, 1, out);
            fwrite(&method->start_instruction, sizeof(int), 1, out);
            fwrite(&method->instruction_count, sizeof(int), 1, out);
            fwrite(&method->param_count, 1, 1, out);
        }
    }

    // Cantidad de strings
    uint16_t string_count = combined_pool.count;
    fwrite(&string_count, sizeof(uint16_t), 1, out);

    // Strings
    for (int i = 0; i < combined_pool.count; i++) {
        uint16_t str_len = strlen(combined_pool.strings[i]);
        fwrite(&str_len, sizeof(uint16_t), 1, out);
        fwrite(combined_pool.strings[i], 1, str_len, out);
    }

    // Instrucciones desde archivo temporal
    rewind(temp);
    char buffer[sizeof(Instruction)];
    while (fread(buffer, sizeof(Instruction), 1, temp)) {
        fwrite(buffer, sizeof(Instruction), 1, out);
    }

    fclose(temp);
    fclose(out);

    printf("✓ Compilation completed successfully\n");
    printf("  Output: %s\n", output_file);
    printf("  Files compiled: %d\n", file_count);
    printf("  Instructions: %d\n", total_instructions);
    printf("  Strings: %d\n", string_count);
    printf("  Global variables: %d\n", var_count);
    printf("  Classes: %d\n", class_count);

    // Liberar memoria
    for (int i = 0; i < combined_pool.count; i++) {
        free(combined_pool.strings[i]);
    }
    free(combined_pool.strings);

    for (int i = 0; i < var_pool.count; i++) {
        free(var_pool.vars[i].name);
    }
    free(var_pool.vars);

    for (int i = 0; i < class_pool.count; i++) {
        free(class_pool.classes[i].name);
        for (int j = 0; j < class_pool.classes[i].var_count; j++) {
            free(class_pool.classes[i].var_names[j]);
        }
        free(class_pool.classes[i].var_names);
        free(class_pool.classes[i].var_types);
        free(class_pool.classes[i].methods);
    }
    free(class_pool.classes);

    for (int i = 0; i < file_count; i++) {
        free(files[i]);
    }
    free(files);
    free_project_config(config);

    return EXIT_SUCCESS;
}
