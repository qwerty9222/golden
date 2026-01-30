#ifndef IMPORTS_H
#define IMPORTS_H

typedef struct {
    char **files;
    int count;
} FileList;

FileList* extract_imports(const char *source_file, const char *project_dir);
void free_file_list(FileList *list);

#endif
