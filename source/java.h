/*
 * Earth And Legend (PS Vita) - FalsoJNI values that need to be created at
 * runtime (strings, field values).
 */

#ifndef EAL_JAVA_H
#define EAL_JAVA_H

#ifdef __cplusplus
extern "C" {
#endif

/** Create the Java strings/fields the engine reads on startup. */
void java_init_values(void);

/** Update the java.io.FileDescriptor.descriptor field used for the data pack. */
void java_set_pack_fd(int fd);

#ifdef __cplusplus
}
#endif

#endif // EAL_JAVA_H
