#ifndef WSPRD_EMBEDDED_H
#define WSPRD_EMBEDDED_H

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*wsprd_output_callback_t) (void * context, char const * text, int is_error);

void wsprd_set_output_callback (wsprd_output_callback_t callback, void * context);
int wsprd_run (int argc, char * argv[]);

#ifdef __cplusplus
}
#endif

#endif
