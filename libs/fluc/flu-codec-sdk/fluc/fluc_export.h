#ifndef _FLUC_EXPORT_H_
#define _FLUC_EXPORT_H_

#ifdef FLUC_EXPORT
#undef FLUC_EXPORT
#endif

#ifdef _WIN32
#define FLUC_EXPORT __declspec(dllexport)
#else
#ifdef __GNUC__
#if __GNUC__ >= 4
#define FLUC_EXPORT __attribute__ ((visibility ("default")))
#else
#define FLUC_EXPORT
#endif
#else
#define FLUC_EXPORT
#endif
#endif

#endif /*  _FLUC_EXPORT_H_  */
