#ifndef KANAGLOBAL_H
#define KANAGLOBAL_H

#ifdef _MSC_VER
#  define CPP_KANA_DECL_EXPORT __declspec(dllexport)
#  define CPP_KANA_DECL_IMPORT __declspec(dllimport)
#else
#  define CPP_KANA_DECL_EXPORT __attribute__((visibility("default")))
#  define CPP_KANA_DECL_IMPORT __attribute__((visibility("default")))
#endif

#ifndef CPP_KANA_EXPORT
#  ifdef CPP_KANA_STATIC
#    define CPP_KANA_EXPORT
#  else
#    ifdef CPP_KANA_LIBRARY
#      define CPP_KANA_EXPORT CPP_KANA_DECL_EXPORT
#    else
#      define CPP_KANA_EXPORT CPP_KANA_DECL_IMPORT
#    endif
#  endif
#endif

#endif //KANAGLOBAL_H
