#ifndef PREPROCESSOR_H
#define PREPROCESSOR_H

// Preprocessor function to expand #include directives
// Returns a malloc'd string with all includes expanded
// The caller is responsible for freeing the returned string
char* preprocess_includes(const char* filename);

#endif // PREPROCESSOR_H