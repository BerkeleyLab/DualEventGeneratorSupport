TOP=../..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

#==================================================
# build a support library

LIBRARY_IOC += eventGeneratorSup

# install eventGeneratorSup.dbd into <top>/dbd
DBD += eventGeneratorSup.dbd

# specify all source files to be compiled and added to the library
eventGeneratorSup_SRCS += eventGenerator.c

eventGeneratorSup_LIBS += $(EPICS_BASE_IOC_LIBS)

#===========================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE
