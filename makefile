SOURCES = p4_plugin.cpp
INCLUDES = -Iutils
OBJECTS = ${SOURCES:.cc=.o}
BINARY = p4_plugin
RM = /bin/rm -f

C = c
CFLAGS = -c -g -D_GNU_SOURCE
LINK = c
LINKFLAGS =

.cpp.o :
   ${C} ${CFLAGS} $< ${INCLUDES}

${BINARY} : ${OBJECTS}
   ${LINK} -o ${BINARY} ${OBJECTS} ${LIBRARIES}

clean :
   - ${RM} ${OBJECTS} ${BINARY}