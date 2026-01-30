#ifndef VM_H
#define VM_H

#include <stdint.h>

#define OPCODE_PRINT        0x01
#define OPCODE_PRINTLN      0x08
#define OPCODE_PRINTCHR     0x09
#define OPCODE_GET_GLOBAL   0x0A
#define OPCODE_RETURN       0xFF
#define OPCODE_NEW_INSTANCE 0x02
#define OPCODE_CALL_METHOD  0x03
#define OPCODE_GET_FIELD    0x04
#define OPCODE_SET_FIELD    0x05
#define OPCODE_PUSH_VALUE   0x06
#define OPCODE_POP_VALUE    0x07
#define OPCODE_ARRAY_DECL   0x0B
#define OPCODE_ARRAY_SET    0x0C
#define OPCODE_ARRAY_GET    0x0D
#define OPCODE_ARRAY_NEW    0x0E
#define OPCODE_ARRAY_LEN    0x0F
#define OPCODE_ARRAY_CLEAR  0x10

typedef struct {
    uint8_t opcode;
    uint8_t arg1;
    uint8_t arg2;
} Instruction;

typedef struct {
    char **strings;
    int string_count;
} StringPool;

typedef struct {
    char *name;
    char type;      // 'i' = int, 'd' = double, 's' = string
    double value;   // Para int/double
    char *str_val;  // Para string
} Variable;

typedef struct {
    char *name;
    char type;      // 'i' = int, 'd' = double, 's' = string
    int size;       // Número de elementos
    double *data;   // Array de valores (para int/double)
    char **str_data;// Array de strings
} Array;

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
    char **var_names;
    uint8_t *var_types;
    int var_count;
} ClassDefinition;

typedef struct {
    ClassDefinition *classes;
    int class_count;
} ClassPool;

typedef struct {
    char window_title[256];
    uint16_t window_width;
    uint16_t window_height;
    uint8_t window_resizable;
    char window_mode[32];
    char renderer[32];     // Renderizador: opengl, none
    uint16_t fps;          // Frames por segundo (valor por defecto: 60)
} WindowConfig;

typedef struct {
    char *class_name;
    double *field_values;  // Instance variable values
    int field_count;
} ObjectInstance;

typedef struct {
    Instruction *instructions;
    int instruction_count;
    int pc;  // Program counter
    
    // Stack para valores
    double stack[256];
    int sp;  // Stack pointer
    
    // Stack para objetos (almacenar índices)
    int object_stack[64];
    int obj_sp;
    
    StringPool string_pool;
    Variable *variables;
    int variable_count;
    Array *arrays;
    int array_count;
    ClassPool class_pool;
    ObjectInstance *objects;
    int object_count;
    
    // Configuración de ventana
    WindowConfig window_config;
} VMState;

int execute_bytecode(const char *bytecode_file, int debug, const char *override_renderer);

#endif
