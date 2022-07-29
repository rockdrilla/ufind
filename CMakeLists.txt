cmake_minimum_required( VERSION 3.0 )

project( ufind LANGUAGES C )

set( PROJECT_SOURCES
    ufind.c
    popcnt.h
    ulist.h
    uhash.h
    uhash-common.h
    uhash-type0.h
    uhash-type1.h
    uhash-type2.h
    uhash-type3.h
)

add_executable( ufind ${PROJECT_SOURCES} )
