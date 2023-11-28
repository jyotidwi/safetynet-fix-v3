#include <android/log.h>
#include <unistd.h>
#include <vector>

#include "zygisk.hpp"

using zygisk::Api;
using zygisk::AppSpecializeArgs;
using zygisk::ServerSpecializeArgs;

#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, "SNFix/JNI", __VA_ARGS__)

class SafetyNetFix : public zygisk::ModuleBase {
public:
    void onLoad(Api *api, JNIEnv *env) override {
        this->api = api;
        this->env = env;
    }

    void preAppSpecialize(AppSpecializeArgs *args) override {
        bool isGms = false, isGmsUnstable = false;

        auto proc = env->GetStringUTFChars(args->nice_name, nullptr);

        if (proc) {
            isGms = strncmp(proc, "com.google.android.gms", 22) == 0;
            isGmsUnstable = strcmp(proc, "com.google.android.gms.unstable") == 0;
        }

        env->ReleaseStringUTFChars(args->nice_name, proc);

        if (isGms) api->setOption(zygisk::FORCE_DENYLIST_UNMOUNT);

        if (isGmsUnstable) {
            long size = 0;
            int fd = api->connectCompanion();

            read(fd, &size, sizeof(long));

            if (size > 0) {
                vector.resize(size);
                read(fd, vector.data(), size);
                LOGD("Read %ld bytes from fd!", size);
            } else {
                LOGD("Coulnd't read classes.dex from fd!");
            }

            close(fd);
        }

        api->setOption(zygisk::DLCLOSE_MODULE_LIBRARY);
    }

    void postAppSpecialize(const AppSpecializeArgs *args) override {
        if (vector.empty()) return;

        LOGD("get system classloader");
        auto clClass = env->FindClass("java/lang/ClassLoader");
        auto getSystemClassLoader = env->GetStaticMethodID(clClass, "getSystemClassLoader",
                                                           "()Ljava/lang/ClassLoader;");
        auto systemClassLoader = env->CallStaticObjectMethod(clClass, getSystemClassLoader);

        LOGD("create class loader");
        auto dexClClass = env->FindClass("dalvik/system/InMemoryDexClassLoader");
        auto dexClInit = env->GetMethodID(dexClClass, "<init>",
                                          "(Ljava/nio/ByteBuffer;Ljava/lang/ClassLoader;)V");
        auto buf = env->NewDirectByteBuffer(vector.data(), static_cast<jlong>(vector.size()));
        auto dexCl = env->NewObject(dexClClass, dexClInit, buf, systemClassLoader);

        LOGD("load class");
        auto loadClass = env->GetMethodID(clClass, "loadClass",
                                          "(Ljava/lang/String;)Ljava/lang/Class;");
        auto entryClassName = env->NewStringUTF("es.chiteroman.safetynetfix.EntryPoint");
        auto entryClassObj = env->CallObjectMethod(dexCl, loadClass, entryClassName);

        LOGD("call init");
        auto entryClass = (jclass) entryClassObj;
        auto entryInit = env->GetStaticMethodID(entryClass, "init", "()V");
        env->CallStaticVoidMethod(entryClass, entryInit);

        vector.clear();
    }

    void preServerSpecialize(ServerSpecializeArgs *args) override {
        api->setOption(zygisk::DLCLOSE_MODULE_LIBRARY);
    }

private:
    Api *api = nullptr;
    JNIEnv *env = nullptr;
    std::vector<char> vector;
};

static void companion(int fd) {
    long size = 0;
    std::vector<char> vector;

    FILE *file = fopen("/data/adb/modules/safetynet-fix/classes.dex", "rb");

    if (file) {
        fseek(file, 0, SEEK_END);
        size = ftell(file);
        fseek(file, 0, SEEK_SET);

        vector.resize(size);
        fread(vector.data(), 1, size, file);

        fclose(file);
    }

    write(fd, &size, sizeof(long));
    write(fd, vector.data(), size);
}

REGISTER_ZYGISK_MODULE(SafetyNetFix)

REGISTER_ZYGISK_COMPANION(companion)