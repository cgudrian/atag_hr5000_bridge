#ifndef UNITY_CONFIG_H
#define UNITY_CONFIG_H

// Unity configuration for ATAG HR5000 Bridge tests

// Use standard types
#define UNITY_INCLUDE_SETUP_STUBS
#define UNITY_INCLUDE_TEARDOWN_STUBS

// Enable float comparison support
#define UNITY_INCLUDE_FLOAT
#define UNITY_FLOAT_PRECISION 0.00001f

// Enable additional output formats
#define UNITY_INCLUDE_PRINT_FORMATTED

// Use standard output
#include <stdio.h>
#define UNITY_OUTPUT_CHAR(a)    putchar(a)
#define UNITY_OUTPUT_FLUSH()    fflush(stdout)
#define UNITY_OUTPUT_START()    
#define UNITY_OUTPUT_COMPLETE() 

// Memory allocation support
#include <stdlib.h>
#define UNITY_EXCLUDE_SETJMP_H

#endif // UNITY_CONFIG_H