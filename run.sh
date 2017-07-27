#!/bin/sh

#  run.sh
#  jvmti-tools
#
#  Created by Nezih Yigitbasi on 7/27/17.
#  Copyright © 2017 Nezih Yigitbasi. All rights reserved.

pushd .
cd test
javac DeflateTest.java
popd
LD_LIBRARY_PATH=build/ java -agentlib:jnistack_agent -cp test/ DeflateTest
