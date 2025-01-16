#include "onidriverloader.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

static inline void close_library(lib_handle_t handle) {
    if (handle) {
#ifdef _WIN32
        FreeLibrary(handle);
#else
        dlclose(handle);
#endif
    }
}

static inline lib_handle_t open_library(const char* name)
{
#ifdef _WIN32
    return LoadLibraryEx(name, NULL, LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
#else
    // Clear errors
    dlerror();
    lib_handle_t lib = dlopen(name, RTLD_NOW | RTLD_LOCAL);
#ifndef NDEBUG
    char *e = dlerror();
    if (e != NULL)
        fprintf(stderr, "%s\n", dlerror());
#endif
    return lib;
#endif
}

static inline void* get_driver_function(lib_handle_t handle, const char* function_name)
{
#ifdef _WIN32
    return (void*)GetProcAddress(handle, function_name);
#else
    dlerror();
    void *f = (void*)dlsym(handle, function_name);
#ifndef NDEBUG
    char *e = dlerror();
    if (e != NULL)
        fprintf(stderr, "%s\n", dlerror());
#endif
    return f;
#endif
}

// Macro to load a function a check for error
#define DSTR(x) #x
#define LOAD_FUNCTION(fname) {\
    driver-> fname = ( oni_driver_ ## fname ## _f)get_driver_function(handle,DSTR(oni_driver_ ## fname)); \
    if (!driver-> fname) rc = -1; \
}

int oni_create_driver(const char* lib_name, oni_driver_t* driver)
{
#if defined(_WIN32)
    const char* extension = ".dll";
#elif defined(__APPLE__)
    const char* extension = ".dylib";
#else
    const char* extension = ".so";
#endif
    const char* prefix = "libonidriver_";
    lib_handle_t handle;
    int rc = ONI_ESUCCESS;

    size_t len = strlen(extension) + strlen(lib_name) + strlen(prefix);

    char* full_lib_name = malloc(len + 1);
    snprintf(full_lib_name, len + 1, "%s%s%s", prefix, lib_name, extension);
    handle = open_library(full_lib_name);
    free(full_lib_name);

    if (!handle) {
        fprintf(stderr, "Failed to load driver: %s\n", dlerror());
        return -1;
    }

    LOAD_FUNCTION(create_ctx);
    LOAD_FUNCTION(destroy_ctx);
    LOAD_FUNCTION(init);
    LOAD_FUNCTION(read_stream);
    LOAD_FUNCTION(write_stream);
    LOAD_FUNCTION(read_config);
    LOAD_FUNCTION(write_config);
    LOAD_FUNCTION(set_opt_callback);
    LOAD_FUNCTION(set_opt);
    LOAD_FUNCTION(get_opt);
    LOAD_FUNCTION(info);

    if (!rc) {
        driver->ctx = driver->create_ctx();
        if (!driver->ctx) rc = -1;
    }

    if (rc)
        close_library(handle);
    else
        driver->handle = handle;

    return rc;
}

int oni_destroy_driver(oni_driver_t* driver)
{
    int rc;
    rc = driver->destroy_ctx(driver->ctx);
    if (!rc)
    {
        close_library(driver->handle);
        memset(driver, 0, sizeof(oni_driver_t));
    }
    return rc;
}
