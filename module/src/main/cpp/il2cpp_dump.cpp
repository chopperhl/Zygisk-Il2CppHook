//
// Created by Perfare on 2020/7/4.
//

#include "il2cpp_dump.h"
#include <dlfcn.h>
#include <cstdlib>
#include <cstring>
#include <cinttypes>
#include <string>
#include <vector>
#include <sstream>
#include <fstream>
#include <unistd.h>
#include "xdl.h"
#include "log.h"
#include "il2cpp-tabledefs.h"
#include "il2cpp-class.h"


#define DO_API(r, n, p) r (*n) p

#include "il2cpp-api-functions.h"

#undef DO_API


#include "dobby.h"
static uint64_t il2cpp_base = 0;


void init_il2cpp_api(void *handle) {
#define DO_API(r, n, p) {                      \
    n = (r (*) p)xdl_sym(handle, #n, nullptr); \
    if(!n) {                                   \
        LOGW("api not found %s", #n);          \
    }                                          \
}

#include "il2cpp-api-functions.h"

#undef DO_API
}

void il2cpp_dump();




void il2cpp_api_init(void *handle) {
    LOGI("il2cpp_handle: %p", handle);
    init_il2cpp_api(handle);
    if (il2cpp_domain_get_assemblies) {
        Dl_info dlInfo;
        if (dladdr((void *) il2cpp_domain_get_assemblies, &dlInfo)) {
            il2cpp_base = reinterpret_cast<uint64_t>(dlInfo.dli_fbase);
        }
        LOGI("il2cpp_base: %" PRIx64"", il2cpp_base);
    } else {
        LOGE("Failed to initialize il2cpp api.");
        return;
    }
    while (!il2cpp_is_vm_thread(nullptr)) {
        LOGI("Waiting for il2cpp_init...");
        sleep(1);
    }
    auto domain = il2cpp_domain_get();
    il2cpp_thread_attach(domain);
}
FieldInfo* get_field(Il2CppClass *klass,const char * name) {
    void *iter = nullptr;
    while (auto field = il2cpp_class_get_fields(klass, &iter)) {
        auto fieldName = il2cpp_field_get_name(field);
        if (strcmp(fieldName,name) == 0) {
            return field;
        }
    }
    return nullptr;
}


static const MethodInfo * getData;
static const MethodInfo * hook1;
static const MethodInfo * hook2;

uint8_t (*func1)(void *);
uint8_t func1_impl(void * p){
    auto skillData = il2cpp_runtime_invoke(getData,p, nullptr, nullptr);
    auto levelField = get_field(skillData->klass,"level");
    auto level = il2cpp_field_get_value_object(levelField,skillData);
    int * val = static_cast<int32_t *>(il2cpp_object_unbox(level));
    if (*val > 6){
        return 1;
    }
    return 0;
}

uint8_t (*func2)(void *);
uint8_t func2_impl(void * p){
    return func1_impl(p);
}

void il2cpp_hook() {
    il2cpp_dump();
    DobbyHook(reinterpret_cast<void *>(hook1->methodPointer), reinterpret_cast<dobby_dummy_func_t>(func1_impl),reinterpret_cast<dobby_dummy_func_t *>(&func1));
    DobbyHook(reinterpret_cast<void *>(hook2->methodPointer), reinterpret_cast<dobby_dummy_func_t>(func2_impl),reinterpret_cast<dobby_dummy_func_t *>(&func2));
}


void dump_class(Il2CppClass *klass) {
    auto classNamespace = il2cpp_class_get_namespace(klass);
    auto className = il2cpp_class_get_name(klass);
    if (strcmp("BattleController",className) == 0 && strcmp("Torappu.Battle",classNamespace) == 0){
        LOGI("dump class %s",className);
    }
    if (strcmp("BasicSkill",className) == 0 && strcmp("Torappu.Battle",classNamespace) == 0){
        LOGI("dump class %s",className);
        getData = il2cpp_class_get_method_from_name(klass,"get_data",0);
        hook1 = il2cpp_class_get_method_from_name(klass,"get_canSkipReduceSp",0);
        hook2 = il2cpp_class_get_method_from_name(klass,"get_canCastWithNoSp",0);
    }
}

void il2cpp_dump() {
    size_t size;
    auto domain = il2cpp_domain_get();
    auto assemblies = il2cpp_domain_get_assemblies(domain, &size);
    if (il2cpp_image_get_class) {
        LOGI("Version more than 2018.3");
        //使用il2cpp_image_get_class
        for (int i = 0; i < size; ++i) {
            auto image = il2cpp_assembly_get_image(assemblies[i]);
            auto classCount = il2cpp_image_get_class_count(image);
            for (int j = 0; j < classCount; ++j) {
                auto klass = il2cpp_image_get_class(image, j);
                auto type = il2cpp_class_get_type(const_cast<Il2CppClass *>(klass));
                dump_class(il2cpp_class_from_type(type));
            }
        }
    } else {
        LOGI("Version less than 2018.3");
        //使用反射
        auto corlib = il2cpp_get_corlib();
        auto assemblyClass = il2cpp_class_from_name(corlib, "System.Reflection", "Assembly");
        auto assemblyLoad = il2cpp_class_get_method_from_name(assemblyClass, "Load", 1);
        auto assemblyGetTypes = il2cpp_class_get_method_from_name(assemblyClass, "GetTypes", 0);
        if (assemblyLoad && assemblyLoad->methodPointer) {
            LOGI("Assembly::Load: %p", assemblyLoad->methodPointer);
        } else {
            LOGI("miss Assembly::Load");
            return;
        }
        if (assemblyGetTypes && assemblyGetTypes->methodPointer) {
            LOGI("Assembly::GetTypes: %p", assemblyGetTypes->methodPointer);
        } else {
            LOGI("miss Assembly::GetTypes");
            return;
        }
        typedef void *(*Assembly_Load_ftn)(void *, Il2CppString *, void *);
        typedef Il2CppArray *(*Assembly_GetTypes_ftn)(void *, void *);
        for (int i = 0; i < size; ++i) {
            auto image = il2cpp_assembly_get_image(assemblies[i]);
            auto image_name = il2cpp_image_get_name(image);
            auto imageName = std::string(image_name);
            auto pos = imageName.rfind('.');
            auto imageNameNoExt = imageName.substr(0, pos);
            auto assemblyFileName = il2cpp_string_new(imageNameNoExt.data());
            auto reflectionAssembly = ((Assembly_Load_ftn) assemblyLoad->methodPointer)(nullptr,
                                                                                        assemblyFileName,
                                                                                        nullptr);
            auto reflectionTypes = ((Assembly_GetTypes_ftn) assemblyGetTypes->methodPointer)(
                    reflectionAssembly, nullptr);
            auto items = reflectionTypes->vector;
            for (int j = 0; j < reflectionTypes->max_length; ++j) {
                auto klass = il2cpp_class_from_system_type((Il2CppReflectionType *) items[j]);
                dump_class(klass);
            }
        }
    }
}
