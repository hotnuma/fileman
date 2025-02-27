#ifndef ETYPE_H
#define ETYPE_H

#include <glib-object.h>
#include <libmacros.h>

#define c_autounref GC_CLEANUP(_c_autounref_func)

GC_UNUSED static inline void _c_autounref_func(void *p)
{
    void **pp = (void**) p;

    if (*pp)
        g_object_unref(*pp);
}

#define E_DECLARE_FINAL_TYPE(ObjName, obj_name, OBJ_NAME, ParentName)       \
    GType obj_name##_get_type();                                            \
    G_GNUC_BEGIN_IGNORE_DEPRECATIONS                                        \
    typedef struct _##ObjName ObjName;                                      \
    typedef struct {ParentName##Class parent_class;} ObjName##Class;        \
                                                                            \
    _GLIB_DEFINE_AUTOPTR_CHAINUP(ObjName, ParentName)                       \
    G_DEFINE_AUTOPTR_CLEANUP_FUNC(ObjName##Class, g_type_class_unref)       \
                                                                            \
    G_GNUC_UNUSED static inline ObjName* OBJ_NAME(gpointer ptr)             \
    {                                                                       \
    return G_TYPE_CHECK_INSTANCE_CAST(ptr, obj_name##_get_type(), ObjName); \
    }                                                                       \
    G_GNUC_UNUSED static inline gboolean IS_##OBJ_NAME(gpointer ptr)        \
    {                                                                       \
    return G_TYPE_CHECK_INSTANCE_TYPE(ptr, obj_name##_get_type());          \
    }                                                                       \
    G_GNUC_END_IGNORE_DEPRECATIONS

#endif // ETYPE_H


