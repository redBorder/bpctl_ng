################################################################################
#
# Intel(R) 10GbE PCI Express Linux Network Driver
# Copyright(c) 1999 - 2017 Intel Corporation.
#
# This program is free software; you can redistribute it and/or modify it
# under the terms and conditions of the GNU General Public License,
# version 2, as published by the Free Software Foundation.
#
# This program is distributed in the hope it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
# FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
# more details.
#
# The full GNU General Public License is included in this distribution in
# the file called "COPYING".
#
# Contact Information:
# Linux NICS <linux.nics@intel.com>
# e1000-devel Mailing List <e1000-devel@lists.sourceforge.net>
# Intel Corporation, 5200 N.E. Elam Young Parkway, Hillsboro, OR 97124-6497
# Silicom, Ltd
################################################################################

MOD_VER=5.2.0.42-rb

INSTALL_MOD_PATH?=/usr
PRDPATH_UTIL?=/bin
TARGET_UTIL=bpctl_util
OBJS_UTIL=bp_util.o

#ifeq (,$(wildcard common.mk))
#  $(error Cannot find common.mk build rules)
#else
#  include common.mk
#endif


###############
# Build rules #
###############

# Standard compilation, with regular output
default: $(TARGET_UTIL)

# Clean the module subdirectories
clean:
	@-rm -rf *.ko
	rm -rf $(OBJS_UTIL) $(TARGET_UTIL)

rpm: clean
	$(MAKE) -C packaging/rpm

rpmtest:
	$(MAKE) LATEST=`git stash create` -C packaging/rpm

# Install the modules
install: default
	install $(TARGET_UTIL) $(INSTALL_MOD_PATH)$(PRDPATH_UTIL)
	install bpctl_start $(INSTALL_MOD_PATH)$(PRDPATH_UTIL)
	install bpctl_stop $(INSTALL_MOD_PATH)$(PRDPATH_UTIL)	
	
$(TARGET_UTIL): $(OBJS_UTIL)
	$(CC) $(OBJS_UTIL) -g -Wall -o $(TARGET_UTIL) 

uninstall:
	if [ -e $(INSTALL_MOD_PATH)$(PRDPATH_UTIL)/$(TARGET_UTIL) ] ; then \
	    rm -f $(INSTALL_MOD_PATH)$(PRDPATH_UTIL)/$(TARGET_UTIL) ; \
	fi
	if [ -e $(INSTALL_MOD_PATH)$(PRDPATH_UTIL)/bpctl_start ] ; then \
	    rm -f $(INSTALL_MOD_PATH)$(PRDPATH_UTIL)/bpctl_start ; \
	fi
	if [ -e $(INSTALL_MOD_PATH)$(PRDPATH_UTIL)/bpctl_stop ] ; then \
	    rm -f $(INSTALL_MOD_PATH)$(PRDPATH_UTIL)/bpctl_stop ; \
	fi	

########
# Help #
########
help:
	@echo 'Cleaning targets:'
	@echo '  clean               - Clean files'
	@echo 'Build targets:'
	@echo '  default             - Build utilities'
	@echo 'Other targets:'
	@echo '  install             - Build then install the module(s)'
	@echo '  uninstall           - Uninstall the module(s)'
	@echo '  help                - Display this help message'
	@echo 'Variables:'
	@echo '  LINUX_VERSION       - Debug tool to force kernel LINUX_VERSION_CODE. Use at your own risk.'
	@echo '  W=N                 - Kernel variable for setting warning levels'
	@echo '  V=N                 - Kernel variable for setting output verbosity'
	@echo '  INSTALL_MOD_PATH    - Add prefix for the module installation path'

.PHONY: default clean install uninstall help

