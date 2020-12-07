TOP=../..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

#=============================
# Build the IOC application

PROD_IOC = eventGenerator
# eventGenerator.dbd will be created and installed
DBD += eventGenerator.dbd

# eventGenerator.dbd will be made up from these files:
eventGenerator_DBD += base.dbd

# Include dbd files from all support applications:
eventGenerator_DBD += eventGeneratorSup.dbd
eventGenerator_DBD += drvAsynIPPort.dbd
eventGenerator_DBD += asyn.dbd
eventGenerator_DBD += asSupport.dbd
eventGenerator_DBD += iocExit.dbd

# Add all the support libraries needed by this IOC
eventGenerator_LIBS += eventGeneratorSup
eventGenerator_LIBS += asyn
eventGenerator_LIBS += autosave
eventGenerator_LIBS += iocExit
eventGenerator_LIBS += seq

# eventGenerator_registerRecordDeviceDriver.cpp derives from eventGenerator.dbd
eventGenerator_SRCS += eventGenerator_registerRecordDeviceDriver.cpp

# Build the main IOC entry point on workstation OSs.
eventGenerator_SRCS_DEFAULT += eventGeneratorMain.cpp
eventGenerator_SRCS_vxWorks += -nil-

# Add support from base/src/vxWorks if needed
#eventGenerator_OBJS_vxWorks += $(EPICS_BASE_BIN)/vxComLibrary

# Finally link to the EPICS Base libraries
eventGenerator_LIBS += $(EPICS_BASE_IOC_LIBS)

#===========================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

