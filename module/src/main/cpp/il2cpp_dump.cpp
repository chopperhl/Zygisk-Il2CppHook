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


void il2cpp_dump();

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

 Il2CppObject * getFieldVal(Il2CppObject * obj,char * name){
     auto field = il2cpp_class_get_field_from_name(obj->klass, name);
     return  il2cpp_field_get_value_object(field,obj);
}



static const MethodInfo * getData;
static const MethodInfo * hook1;
static const MethodInfo * hook2;

install_hook_name(func1,uint8_t,void * p){
    auto skillData = il2cpp_runtime_invoke(getData,p, nullptr, nullptr);
    auto level = getFieldVal(skillData,"level");
    int * val = static_cast<int32_t *>(il2cpp_object_unbox(level));
    if (*val > 6){
        return 1;
    }
    return 0;
}


install_hook_name(func2,uint8_t,void * p){
    return fake_func1(p);
}



install_hook_name(enemy,int32_t,Il2CppObject * p){
    return 0;
}

void il2cpp_hook() {
    il2cpp_dump();
    install_hook_func1(reinterpret_cast<void *>(hook1->methodPointer));
    install_hook_func2(reinterpret_cast<void *>(hook2->methodPointer));
}


void dump_class(Il2CppClass *klass) {
    auto classNamespace = il2cpp_class_get_namespace(klass);
    auto className = il2cpp_class_get_name(klass);
    if (strcmp("BasicSkill",className) == 0 && strcmp("Torappu.Battle",classNamespace) == 0){
        LOGI("dump class %s",className);
        getData = il2cpp_class_get_method_from_name(klass,"get_data",0);
        hook1 = il2cpp_class_get_method_from_name(klass,"get_canSkipReduceSp",0);
        hook2 = il2cpp_class_get_method_from_name(klass,"get_canCastWithNoSp",0);
    }
    if (strcmp("Enemy",className) == 0  && strcmp("Torappu.Battle",classNamespace) == 0){
        LOGI("dump class %s",className);
        auto  lifeReduce = il2cpp_class_get_method_from_name(klass,"get_lifePointReduce",0);
        install_hook_enemy(reinterpret_cast<void *>(lifeReduce->methodPointer));
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
