#!/bin/sh

# Builds the agent for OSX and puts the artifacts under build/

rm -fR build
rm test/*.class
mkdir build

SRC='src'
BUILD='build'
JAVA_HOME=/Library/Java/JavaVirtualMachines/jdk1.8.0_111.jdk/Contents/Home/
echo "Using JAVA_HOME=${JAVA_HOME}"

gcc -c -g -fno-strict-aliasing -fPIC -fno-omit-frame-pointer -I. -I${JAVA_HOME}/include/ -I${JAVA_HOME}/include/darwin/ ${SRC}/print_stack.c -o ${BUILD}/print_stack.o
g++ -dynamiclib -undefined suppress -flat_namespace ${BUILD}/print_stack.o -o ${BUILD}/libjnistack_agent.dylib
