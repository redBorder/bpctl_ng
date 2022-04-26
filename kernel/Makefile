
PACKAGE_NAME?=	bpctl
VERSION?=	$(shell git describe --abbrev=6 --tags HEAD --always | sed 's/-/_/g')

obj-m := bpctl_mod.o
bpctl_mod-objs := bp_mod.o

DRIVER := bpctl_mod

ifeq (0,$(shell [ ${KVER_CODE} -lt $(call get_kvercode,2,6,0) ]; echo "$?"))
  $(warning *** Aborting the build.)
  $(error This driver is not supported on kernel versions older than 2.6.0)
endif

# assigned by Makefile.dkms
GIT_REV:=

ifndef GIT_REV
 ifneq (, $(shell which git))
  ifneq (, $(shell git rev-parse --is-inside-work-tree 2> /dev/null | grep true))
   ifeq (, $(shell echo ${SUBDIRS}))
    GIT_BRANCH=$(shell git branch --no-color|cut -d ' ' -f 2)
    GIT_HASH=$(shell git rev-parse HEAD)
    ifneq ($(strip $(GIT_BRANCH)),)
     GIT_REV:=${GIT_BRANCH}:${GIT_HASH}
    endif
   endif
  endif
 endif
endif

ifneq ($(strip $(GIT_REV)),)
 GITDEF:=-DGIT_REV="\"${GIT_REV}\""
endif

ifeq (,$(BUILD_KERNEL))
 ifneq (,$(KERNELRELEASE))
  BUILD_KERNEL=$(KERNELRELEASE)
 else
  BUILD_KERNEL=$(shell uname -r)
 endif
endif

PWD ?= $(shell pwd)
EXTRA_CFLAGS += -I${PWD} ${GITDEF}

ifeq (,$(wildcard ${PWD}/common.mk))
  $(error Cannot find common.mk build rules)
else
  include ${PWD}/common.mk
endif

# Set no-pie for compilers that enable pie by default
test_no_nopie = $(shell echo "" | $(CC) -fsyntax-only -no-pie -xc - 2>&1 | grep unrecognized | wc -l)
ifneq (1,$(test_no_nopie))
  EXTRA_CFLAGS += -no-pie
endif
test_no_fnopie = $(shell echo "" | $(CC) -fsyntax-only -fno-pie -xc - 2>&1 | grep unrecognized | wc -l)
ifneq (1,$(test_no_fnopie))
  EXTRA_CFLAGS += -fno-pie
endif

EXTRA_CFLAGS += -Wno-implicit-fallthrough

HERE=${PWD}

devkernelbuild = $(call kernelbuild,$(if $(filter -DNO_PTP_SUPPORT,${EXTRA_CFLAGS}),CONFIG_PTP_1588_CLOCK=n) ${2},${1})

all: 
	@+$(call devkernelbuild,modules)	

dkms-rpm:
	sudo $(MAKE) PACKAGE_NAME=$(PACKAGE_NAME) VERSION=$(VERSION) -f Makefile.dkms rpm

clean:
	@+$(call devkernelbuild,clean)
	@-rm -rf *.ko

install:
	@echo "Installing modules..."
	@+$(call devkernelbuild,modules_install)
	@echo "Running depmod..."
	@$(call cmd_depmod)
	@echo "*****NOTE:";
	@echo "bpctl_mod.ko kernel module installed in ${DESTDIR}";
	@echo "*****";

