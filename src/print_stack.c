#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <jvmti.h>
#include <jni.h>

#include "print_stack.h"

static jniNativeInterface* originalJniFunctions;
static jniNativeInterface* replacedJniFunctions;
static jvmtiEnv* jvmti;

// caller should free the returned jthread
jthread* getCurrentThread()
{
    jthread* currentThread = malloc(sizeof(jthread));
    jvmtiError errorCode = (*jvmti)->GetCurrentThread(jvmti, currentThread);
    if (errorCode != JVMTI_ERROR_NONE) {
        logError("Error getting the current thread [%d]", errorCode);
        exit(1);
    }
    return currentThread;
}

// print the stack trace of the given thread
void printStackTrace(jthread* thread)
{
    jvmtiError errorCode;
    jint maxFrameCount = 30;
    jvmtiFrameInfo* frames = malloc(sizeof(jvmtiFrameInfo) * maxFrameCount);
    jint count = 0;
    char* methodName = NULL;
    char* declaringClassName = NULL;
    jclass declaringClass;
    
    errorCode = (*jvmti)->GetStackTrace(jvmti, *thread, 0, maxFrameCount, frames, &count);
    if (errorCode != JVMTI_ERROR_NONE ) {
        jvmtiThreadInfo threadInfo;
        (*jvmti)->GetThreadInfo(jvmti, *thread, &threadInfo);
        logError("Error getting the stack trace of thread [%s]", threadInfo.name);
    }
    
    for (int i = 0; i < count; i++) {
        errorCode = (*jvmti)->GetMethodName(jvmti, frames[i].method, &methodName, NULL, NULL);
        if (errorCode == JVMTI_ERROR_NONE) {
            errorCode = (*jvmti)->GetMethodDeclaringClass(jvmti, frames[i].method, &declaringClass);
            errorCode = (*jvmti)->GetClassSignature(jvmti, declaringClass, &declaringClassName, NULL);
            if (errorCode == JVMTI_ERROR_NONE) {
                logMessage("%s::%s()", declaringClassName, methodName);
            }
        }
    }
    
    if (methodName != NULL) {
        (*jvmti)->Deallocate(jvmti, (void*) methodName);
    }

    if (declaringClassName != NULL) {
        (*jvmti)->Deallocate(jvmti, (void*) declaringClassName);
    }
    
    free(frames);
}

void preProcess(char* jniFunctionName)
{
    jvmtiThreadInfo threadInfo;
    
    jthread* currentThread = getCurrentThread();
    (*jvmti)->GetThreadInfo(jvmti, *currentThread, &threadInfo);
    logMessage("--- [Thread: %s] %s ---", threadInfo.name, jniFunctionName);
    printStackTrace(currentThread);
    logMessage("\n");
    free(currentThread);
}

void postProcess(char* jniFunctionName)
{
}

//////////////////////////////////////////////////////////////////////

void* newGetPrimitiveArrayCritical(JNIEnv *env, jarray array, jboolean *isCopy)
{
    preProcess("GetPrimitiveArrayCritical");
    void* result = originalJniFunctions->GetPrimitiveArrayCritical(env, array, isCopy);
    postProcess("GetPrimitiveArrayCritical");
    return result;
}

void newReleasePrimitiveArrayCritical(JNIEnv *env, jarray array, void* carray, jint mode)
{
    preProcess("ReleasePrimitiveArrayCritical");
    originalJniFunctions->ReleasePrimitiveArrayCritical(env, array, carray, mode);
    postProcess("ReleasePrimitiveArrayCritical");
}

//////////////////////////////////////////////////////////////////////

static void JNICALL gc_start(jvmtiEnv* jvmti_env)
{
    logMessage("GC start");
}

static void JNICALL gc_finish(jvmtiEnv* jvmti_env)
{
    logMessage("GC end");
}

//////////////////////////////////////////////////////////////////////

static void JNICALL vm_init(jvmtiEnv *jvmti, JNIEnv *env, jthread thread)
{
    jvmtiError errorCode = (*jvmti)->GetJNIFunctionTable(jvmti, &originalJniFunctions);
    if (errorCode != JVMTI_ERROR_NONE) {
        logError("Error getting JNI function table [%d]", errorCode);
        exit(1);
    }
    
    errorCode = (*jvmti)->GetJNIFunctionTable(jvmti, &replacedJniFunctions);
    if (errorCode != JVMTI_ERROR_NONE) {
        logError("Error getting JNI function table [%d]", errorCode);
        exit(1);
    }
    
    replacedJniFunctions->GetPrimitiveArrayCritical = newGetPrimitiveArrayCritical;
    replacedJniFunctions->ReleasePrimitiveArrayCritical = newReleasePrimitiveArrayCritical;
    
    errorCode = (*jvmti)->SetJNIFunctionTable(jvmti, replacedJniFunctions);
    if (errorCode != JVMTI_ERROR_NONE) {
        logError("Error setting JNI function table, err [%d]", errorCode);
        exit(1);
    }
    logMessage("Registered custom GetPrimitiveArrayCritical implementation");
    logMessage("Registered custom ReleasePrimitiveArrayCritical implementation");
}

//////////////////////////////////////////////////////////////////////

JNIEXPORT jint JNICALL Agent_OnLoad(JavaVM *vm, char *options, void *reserved) {
    jvmtiError errorCode;
    jvmtiCapabilities   capabilities;
    jvmtiEventCallbacks callbacks;
    
    jint returnCode = (*vm)->GetEnv(vm, (void **)&jvmti, JVMTI_VERSION);
    if (returnCode != JNI_OK) {
        logError("Cannot get jvmti environment [%d]", returnCode);
        exit(1);
    }

    errorCode = (*jvmti)->GetCapabilities(jvmti, &capabilities);
    if (errorCode != JVMTI_ERROR_NONE) {
        logError("Error getting capabilities [%d]", errorCode);
        exit(1);
    }
    
    capabilities.can_generate_garbage_collection_events = 1;
    errorCode = (*jvmti)->AddCapabilities(jvmti, &capabilities);
    if (errorCode != JVMTI_ERROR_NONE) {
        logError("Error adding capabilities [%d]", errorCode);
        exit(1);
    }
    
    memset(&callbacks, 0, sizeof(callbacks));
    callbacks.VMInit                  = &vm_init;
    callbacks.GarbageCollectionStart  = &gc_start;
    callbacks.GarbageCollectionFinish = &gc_finish;
    (*jvmti)->SetEventCallbacks(jvmti, &callbacks, sizeof(callbacks));
    (*jvmti)->SetEventNotificationMode(jvmti, JVMTI_ENABLE, JVMTI_EVENT_VM_INIT, NULL);
    (*jvmti)->SetEventNotificationMode(jvmti, JVMTI_ENABLE, JVMTI_EVENT_GARBAGE_COLLECTION_START, NULL);
    (*jvmti)->SetEventNotificationMode(jvmti, JVMTI_ENABLE, JVMTI_EVENT_GARBAGE_COLLECTION_FINISH, NULL);
    logMessage("jni_stack agent is loaded");
    return JNI_OK;
}

JNIEXPORT void JNICALL Agent_OnUnload(JavaVM *vm)
{
    logMessage("Unloading jni_stack agent");
}

