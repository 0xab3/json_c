#define LOG_DEBUG(...)                                                         \
  do {                                                                         \
    fprintf(stderr, "DEBUG: %s(): ", __func__);                                \
    fprintf(stderr, __VA_ARGS__);                                              \
  } while (0)
#define LOG_WARN(...)                                                          \
  do {                                                                         \
    fprintf(stderr, "WARN: %s(): ", __func__);                                 \
    fprintf(stderr, __VA_ARGS__);                                              \
  } while (0)
#define LOG_ERROR(...)                                                         \
  do {                                                                         \
    fprintf(stderr, "ERROR: %s(): ", __func__);                                \
    fprintf(stderr, __VA_ARGS__);                                              \
  } while (0)
