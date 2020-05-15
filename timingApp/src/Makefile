TOP=../..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

#=============================
# Build the IOC application

PROD_IOC = timing
# timing.dbd will be created and installed
DBD += timing.dbd

# timing.dbd will be made up from these files:
timing_DBD += base.dbd

# Include dbd files from all support applications:
timing_DBD += timingSequence.dbd
timing_DBD += eventGeneratorSup.dbd
timing_DBD += drvAsynIPPort.dbd
timing_DBD += asyn.dbd
timing_DBD += asSupport.dbd
timing_DBD += iocExit.dbd

# Add all the support libraries needed by this IOC
timing_LIBS += eventGeneratorSup
timing_LIBS += asyn
timing_LIBS += autosave
timing_LIBS += iocExit
timing_LIBS += seq

# timing_registerRecordDeviceDriver.cpp derives from timing.dbd
timing_SRCS += timingSequence.st
timing_SRCS += tuningModes.c
timing_SRCS += timing_registerRecordDeviceDriver.cpp

# Build the main IOC entry point on workstation OSs.
timing_SRCS_DEFAULT += timingMain.cpp

# Finally link to the EPICS Base libraries
timing_LIBS += $(EPICS_BASE_IOC_LIBS)

#===========================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

