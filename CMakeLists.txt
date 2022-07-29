cmake_minimum_required( VERSION 3.0 )

project( ufind LANGUAGES C )

set( PROJECT_SOURCES
    ufind.c
)

add_executable( ufind ${PROJECT_SOURCES} )
