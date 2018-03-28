#include <jni.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <android/log.h>

#if 0
typedef struct {
    char *name;          /* Java����õĺ����� */
    char *signature;    /* JNI�ֶ�������, ������ʾJava����õĺ����Ĳ����ͷ���ֵ���� */
    void *fnPtr;          /* C����ʵ�ֵı��غ��� */
} JNINativeMethod;
#endif

static jint fd;

jint ledOpen(JNIEnv *env, jobject cls)
{
	fd= open("/dev/led", O_RDWR);
	if(fd < 0)
		return 0;
	else
		return -1;
}

jint ledClose(JNIEnv *env, jobject cls)
{
	close(fd);
}

jint ledCtrl(JNIEnv *env, jobject cls, jint which , jint status)
{
	int ret = ioctl(fd, status, which);
	return ret;
}

static const JNINativeMethod methods[] = {
	{"ledOpen", "()I", (void *)ledOpen},
	{"ledClose", "()I", (void *)ledClose},
	{"ledCtrl", "(II)I", (void *)ledCtrl},
};

JNIEXPORT jint JNICALL 
JNI_OnLoad(JavaVM *jvm, void *reserved)
{
	JNIEnv *env;
	jclass cls;

	if((*jvm)->GetEnv(jvm, (void **)&env, JNI_VERSION_1_4)) {
		return JNI_ERR;
	}

	cls = (*env)->FindClass(env, "com/thisway/hardlibrary/HardControl");
	if(cls == NULL){
		return JNI_ERR;
	}

	if((*env)->RegisterNatives(env, cls, methods, sizeof(methods)/sizeof(methods[0])) < 0)
		return JNI_ERR;

	return JNI_VERSION_1_4;	
}