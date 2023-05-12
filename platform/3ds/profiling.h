/**
 *  Filename:   profiling.h
 *  Author:     Adrian Padin (padin.adrian@gmail.com)
 *  Date:       2023-04-19
 *  
 *  Helper code for profiling.
 */


#ifndef PROFILING_ZELDA3_H_
#define PROFILING_ZELDA3_H_


/* ===== Includes ===== */
#include <stdlib.h>
#include "3ds.h"


/* ===== Data ===== */

typedef struct {
    double* values;
    size_t size;
    size_t capacity;
} Array;

extern Array profilingArrays[8];


/* ===== Functions ===== */

/**
 * Returns the average of the list of values.
 */
inline double Average(double values[], size_t length) {
    if (length <= 0) { return 0.; }
    double sum = 0.;
    for (size_t i = 0; i < length; ++i) {
        sum += values[i];
    }
    return sum / length;
}

/**
 * Initialize the array.
 */
inline void ArrayInitialize(Array* array, size_t size) {
    array->values = malloc(size * sizeof(double));
    array->capacity = size;
    array->size = 0;
}

/**
 * Push a value to the array.
 */
inline void ArrayPush(Array* array, double value) {
    // If the array has space, add the new element
    // If the array is full, do nothing (not ideal but whatever)
    if (array->size < array->capacity) {
        array->values[array->size] = value;
        array->size++;
    }
}

/**
 * Clear the array.
 */
inline void ArrayClear(Array* array) {
    // Don't actually clear anything, just set the size to zero.
    array->size = 0;
}

/** 
 * Destroy the array. ONLY RUN THIS ON INITIALIZED ARRAYS!
 */
inline void ArrayDestroy(Array* array) {
    free(array->values);
    array->values = NULL;
}

/** 
 * Take the average of the array.
 */
inline double ArrayAverage(Array* array) {
    return Average(array->values, array->size);
}


#endif  // PROFILING_ZELDA3_H_