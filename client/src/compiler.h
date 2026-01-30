#ifndef COMPILER_H
#define COMPILER_H

#include <stdint.h>

typedef struct {
    char *name;
    char type;      // 'i' = int, 'd' = double, 's' = string, 'a' = array estático, 'b' = array dinámico
    double value;   // Para int/double
    char *str_val;  // Para string
    int array_size; // Para arrays: tamaño
    int dynamic_array_size;  // Para arrays dinámicos: tamaño inicial
    char array_element_type; // Para arrays: tipo de elemento
} GlobalVariable;

typedef struct {
    GlobalVariable *vars;
    int count;
} VariablePool;

typedef struct {
    char *name;
    int start_instruction;
    int instruction_count;
    int param_count;
    uint8_t is_public;  // 1 = public, 0 = private
} ClassMethod;

typedef struct {
    char *name;
    ClassMethod *methods;
    int method_count;
    int var_count;  // Number of instance variables
    char **var_names;
    uint8_t *var_types;  // 'd' for double, 'i' for int
} ClassDefinition;

typedef struct {
    ClassDefinition *classes;
    int count;
} ClassPool;

int build_project(const char *project_dir);

#endif
