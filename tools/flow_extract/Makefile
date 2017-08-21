##
# Copyright(c) 2010-2015 Intel Corporation.
# Copyright(c) 2016-2017 Viosoft Corporation.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
#   * Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
#   * Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in
#     the documentation and/or other materials provided with the
#     distribution.
#   * Neither the name of Intel Corporation nor the names of its
#     contributors may be used to endorse or promote products derived
#     from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
##

SOURCES = main.cpp
SOURCES += streamextract.cpp
SOURCES += pcapreader.cpp
SOURCES += pcapwriter.cpp
SOURCES += timestamp.cpp
SOURCES += pcappkt.cpp
SOURCES += netsocket.cpp
SOURCES += stream3.cpp
SOURCES += stream2.cpp
SOURCES += stream.cpp
SOURCES += path.cpp
SOURCES += allocator.cpp
SOURCES += halfstream.cpp
SOURCES += bundle.cpp
SOURCES += progress.cpp
SOURCES += mappedfile.cpp
SOURCES += streamsorter.cpp
SOURCES += memreader.cpp
SOURCES += programconfig.cpp

BUILD_DIR = build
OBJECTS = $(SOURCES:%.cpp=$(BUILD_DIR)/%.o)
PROG = flowextract

CXXFLAGS += -D__STDC_LIMIT_MACROS -g -O2 -Wall -ansi -pedantic -Wno-unused -msse4.2
LDFLAGS = -lpcap

$(BUILD_DIR)/$(PROG): $(OBJECTS)
	@echo -e "LD\t$<"
	@$(CXX) $(CXXFLAGS) $(LDFLAGS) $(OBJECTS) -o $@

-include $(SOURCES:%.cpp=$(BUILD_DIR)/%.d)

$(BUILD_DIR)/%.o: %.cpp
	@mkdir -p $(BUILD_DIR)
	@echo -e "CXX\t $<"
	@$(CXX) -c $(CXXFLAGS) $*.cpp -o $@
	@$(CXX) -MM $(CXXFLAGS) $*.cpp -MT $(BUILD_DIR)/$*.o > $(BUILD_DIR)/$*.d
	@cp -f $(BUILD_DIR)/$*.d $(BUILD_DIR)/$*.d.tmp
	@sed -e 's/.*://' -e 's/\\$$//' < $(BUILD_DIR)/$*.d.tmp | fmt -1 | sed -e 's/^ *//' -e 's/$$/:/' >> $(BUILD_DIR)/$*.d
	@rm -f $(BUILD_DIR)/$*.d.tmp
clean:
	@rm -f $(BUILD_DIR)/$(PROG) $(BUILD_DIR)/*.o $(BUILD_DIR)/*.d
