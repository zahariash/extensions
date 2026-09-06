add_library(lbug_fts_normalize
        OBJECT
        fts_normalize.cpp)

add_library(lbug_fts_utils
        OBJECT
        fts_utils.cpp)

set(FTS_EXTENSION_OBJECT_FILES
        ${FTS_EXTENSION_OBJECT_FILES}
        $<TARGET_OBJECTS:lbug_fts_normalize>
        $<TARGET_OBJECTS:lbug_fts_utils>
        PARENT_SCOPE)
