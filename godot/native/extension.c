#define _POSIX_C_SOURCE 200809L
#include "gdextension_interface.h"
#include "abi_sizes.h"
#include "solvers.h"
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>

/* Opaque engine value sizes come from this Godot executable's API dump. */
typedef union { uint8_t bytes[IK_VARIANT_SIZE]; uint64_t alignment; } Variant;
typedef union { uint8_t bytes[IK_STRING_NAME_SIZE]; uint64_t alignment; } StringName;
static GDExtensionClassLibraryPtr library;
static StringName class_name, parent_name, append_name;
static GDExtensionInterfaceStringNameNewWithLatin1Chars name_new;
static GDExtensionPtrDestructor name_destroy, string_destroy;
static GDExtensionInterfaceStringNewWithUtf8Chars string_new;
static GDExtensionInterfaceClassdbConstructObject construct_object;
static GDExtensionInterfaceObjectSetInstance set_instance;
static GDExtensionInterfaceClassdbRegisterExtensionClass2 register_class;
static GDExtensionInterfaceClassdbUnregisterExtensionClass unregister_class;
static GDExtensionInterfaceClassdbRegisterExtensionClassMethod register_method;
static GDExtensionInterfaceVariantConstruct variant_construct;
static GDExtensionInterfaceVariantCall variant_call;
static GDExtensionInterfaceVariantDestroy variant_destroy;
static GDExtensionInterfaceVariantNewNil variant_nil;
static GDExtensionInterfaceVariantGetType variant_type;
static GDExtensionVariantFromTypeConstructorFunc from_float;
static GDExtensionTypeFromVariantConstructorFunc to_float, to_array;

typedef struct { int unused; } NativeIK;
static GDExtensionObjectPtr create_instance(void *userdata) {
    (void)userdata;
    GDExtensionObjectPtr object = construct_object(&parent_name);
    NativeIK *instance = calloc(1, sizeof(*instance));
    if (!instance) abort();
    set_instance(object, &class_name, instance);
    return object;
}
static void free_instance(void *userdata, GDExtensionClassInstancePtr instance) {
    (void)userdata;
    free(instance);
}

static uint64_t now_ns(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (uint64_t)t.tv_sec * UINT64_C(1000000000) + (uint64_t)t.tv_nsec;
}

/* Solvers live in a separate translation unit, compiled without LTO. Each
 * iteration is a real function call; repeated constant inputs cannot be hoisted.
 * Warmup, engine marshaling, and result allocation are outside the timed loops. */
static void benchmark(const double args[7], GDExtensionVariantPtr output) {
    IKInput input = {(float)args[0], (float)args[1], (float)args[2], (float)args[3], args[4] < 0 ? -1 : 1};
    int count = isfinite(args[5]) ? (int)fmin(100000, fmax(1, args[5])) : 1;
    int first = args[6] != 0;
    IKResult results[2] = {{0}};
    double timings[2] = {0};
    for (int i = 0; i < 16; ++i) {
        results[0] = ik_circles(&input);
        results[1] = ik_cosines(&input);
    }
    for (int offset = 0; offset < 2; ++offset) {
        int method = (first + offset) % 2;
        uint64_t started = now_ns();
        if (method == 0) {
            for (int i = 0; i < count; ++i) results[0] = ik_circles(&input);
        } else {
            for (int i = 0; i < count; ++i) results[1] = ik_cosines(&input);
        }
        timings[method] = (double)(now_ns() - started) / count;
    }
    double values[] = {timings[0], timings[1], results[0].x, results[0].y,
        results[1].x, results[1].y, results[0].status, results[1].status};
    GDExtensionCallError error;
    variant_construct(GDEXTENSION_VARIANT_TYPE_ARRAY, output, NULL, 0, &error);
    for (unsigned i = 0; i < sizeof(values) / sizeof(values[0]); ++i) {
        Variant value, ignored;
        from_float(&value, &values[i]);
        GDExtensionConstVariantPtr arguments[] = {&value};
        variant_call(output, &append_name, arguments, 1, &ignored, &error);
        variant_destroy(&ignored);
        variant_destroy(&value);
    }
}

static void call_benchmark(void *userdata, GDExtensionClassInstancePtr instance,
        const GDExtensionConstVariantPtr *arguments, GDExtensionInt count,
        GDExtensionVariantPtr output, GDExtensionCallError *error) {
    (void)userdata; (void)instance;
    *error = (GDExtensionCallError){.error = GDEXTENSION_CALL_OK};
    if (count != 7) {
        error->error = count < 7 ? GDEXTENSION_CALL_ERROR_TOO_FEW_ARGUMENTS : GDEXTENSION_CALL_ERROR_TOO_MANY_ARGUMENTS;
        error->expected = 7;
        variant_nil(output);
        return;
    }
    double args[7];
    for (int i = 0; i < 7; ++i) {
        GDExtensionVariantType type = variant_type(arguments[i]);
        if (type != GDEXTENSION_VARIANT_TYPE_FLOAT && type != GDEXTENSION_VARIANT_TYPE_INT) {
            error->error = GDEXTENSION_CALL_ERROR_INVALID_ARGUMENT;
            error->argument = i;
            error->expected = GDEXTENSION_VARIANT_TYPE_FLOAT;
            variant_nil(output);
            return;
        }
        to_float(&args[i], (GDExtensionVariantPtr)arguments[i]);
    }
    benchmark(args, output);
}
static void ptrcall_benchmark(void *userdata, GDExtensionClassInstancePtr instance,
        const GDExtensionConstTypePtr *arguments, GDExtensionTypePtr output) {
    (void)userdata; (void)instance;
    double args[7];
    for (int i = 0; i < 7; ++i) args[i] = *(const double *)arguments[i];
    Variant result;
    benchmark(args, &result);
    to_array(output, &result);
    variant_destroy(&result);
}

static void initialize(void *userdata, GDExtensionInitializationLevel level) {
    (void)userdata;
    if (level != GDEXTENSION_INITIALIZATION_SCENE) return;
    name_new(&class_name, "NativeIK", 0);
    name_new(&parent_name, "RefCounted", 0);
    name_new(&append_name, "append", 0);
    GDExtensionClassCreationInfo2 info = {
        .is_exposed = 1, .create_instance_func = create_instance, .free_instance_func = free_instance
    };
    register_class(library, &class_name, &parent_name, &info);
    StringName method_name, names[7], empty;
    union { uint8_t bytes[IK_STRING_SIZE]; uint64_t alignment; } empty_string;
    string_new(&empty_string, "");
    name_new(&empty, "", 0);
    name_new(&method_name, "benchmark", 0);
    const char *labels[] = {"x", "y", "upper", "lower", "orientation", "iterations", "first_solver"};
    GDExtensionPropertyInfo arguments[7];
    GDExtensionClassMethodArgumentMetadata metadata[7];
    for (int i = 0; i < 7; ++i) {
        name_new(&names[i], labels[i], 0);
        arguments[i] = (GDExtensionPropertyInfo){.type = GDEXTENSION_VARIANT_TYPE_FLOAT, .name = &names[i], .class_name = &empty, .hint_string = &empty_string};
        metadata[i] = GDEXTENSION_METHOD_ARGUMENT_METADATA_REAL_IS_DOUBLE;
    }
    GDExtensionPropertyInfo result = {.type = GDEXTENSION_VARIANT_TYPE_ARRAY, .name = &empty, .class_name = &empty, .hint_string = &empty_string};
    GDExtensionClassMethodInfo method = {
        .name = &method_name, .call_func = call_benchmark, .ptrcall_func = ptrcall_benchmark,
        .method_flags = GDEXTENSION_METHOD_FLAG_NORMAL, .has_return_value = 1,
        .return_value_info = &result, .argument_count = 7,
        .arguments_info = arguments, .arguments_metadata = metadata
    };
    register_method(library, &class_name, &method);
    for (int i = 0; i < 7; ++i) name_destroy(&names[i]);
    name_destroy(&method_name);
    name_destroy(&empty);
    string_destroy(&empty_string);
}
static void deinitialize(void *userdata, GDExtensionInitializationLevel level) {
    (void)userdata;
    if (level != GDEXTENSION_INITIALIZATION_SCENE) return;
    unregister_class(library, &class_name);
    name_destroy(&append_name);
    name_destroy(&parent_name);
    name_destroy(&class_name);
}

#define LOAD(type, variable, symbol) variable = (type)(void (*)(void))get_proc(symbol); if (!variable) return 0
__attribute__((visibility("default"))) GDExtensionBool kinetest_library_init(
        GDExtensionInterfaceGetProcAddress get_proc, GDExtensionClassLibraryPtr lib,
        GDExtensionInitialization *initialization) {
    library = lib;
    LOAD(GDExtensionInterfaceStringNameNewWithLatin1Chars, name_new, "string_name_new_with_latin1_chars");
    LOAD(GDExtensionInterfaceClassdbConstructObject, construct_object, "classdb_construct_object");
    LOAD(GDExtensionInterfaceObjectSetInstance, set_instance, "object_set_instance");
    LOAD(GDExtensionInterfaceClassdbRegisterExtensionClass2, register_class, "classdb_register_extension_class2");
    LOAD(GDExtensionInterfaceClassdbUnregisterExtensionClass, unregister_class, "classdb_unregister_extension_class");
    LOAD(GDExtensionInterfaceClassdbRegisterExtensionClassMethod, register_method, "classdb_register_extension_class_method");
    LOAD(GDExtensionInterfaceVariantConstruct, variant_construct, "variant_construct");
    LOAD(GDExtensionInterfaceVariantCall, variant_call, "variant_call");
    LOAD(GDExtensionInterfaceVariantDestroy, variant_destroy, "variant_destroy");
    LOAD(GDExtensionInterfaceVariantNewNil, variant_nil, "variant_new_nil");
    LOAD(GDExtensionInterfaceVariantGetType, variant_type, "variant_get_type");
    GDExtensionInterfaceVariantGetPtrDestructor get_destructor;
    GDExtensionInterfaceGetVariantFromTypeConstructor get_from;
    GDExtensionInterfaceGetVariantToTypeConstructor get_to;
    LOAD(GDExtensionInterfaceVariantGetPtrDestructor, get_destructor, "variant_get_ptr_destructor");
    LOAD(GDExtensionInterfaceGetVariantFromTypeConstructor, get_from, "get_variant_from_type_constructor");
    LOAD(GDExtensionInterfaceGetVariantToTypeConstructor, get_to, "get_variant_to_type_constructor");
    LOAD(GDExtensionInterfaceStringNewWithUtf8Chars, string_new, "string_new_with_utf8_chars");
    string_destroy = get_destructor(GDEXTENSION_VARIANT_TYPE_STRING);
    name_destroy = get_destructor(GDEXTENSION_VARIANT_TYPE_STRING_NAME);
    from_float = get_from(GDEXTENSION_VARIANT_TYPE_FLOAT);
    to_float = get_to(GDEXTENSION_VARIANT_TYPE_FLOAT);
    to_array = get_to(GDEXTENSION_VARIANT_TYPE_ARRAY);
    *initialization = (GDExtensionInitialization){
        .minimum_initialization_level = GDEXTENSION_INITIALIZATION_SCENE,
        .initialize = initialize, .deinitialize = deinitialize
    };
    return 1;
}
