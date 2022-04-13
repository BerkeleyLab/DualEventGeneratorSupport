#!../../bin/linux-x86_64/eventGenerator

###############################################################################
# Set up environment
epicsEnvSet "P" "$(P=EVG)"
epicsEnvSet "R" "$(R=:)"
epicsEnvSet "EVG_ADDRESS" "$(EVG_ADDRESS=131.243.93.169)"
epicsEnvSet "OLD_EVG_SYS" "$(OLD_EVG_SYS=LI11)"
epicsEnvSet "OLD_EVG_T" "$(OLD_EVG_T=)"
epicsEnvSet "FPGA_SIMM_DISABLE" "$(FPGA_SIMM_DISABLE=#)"
epicsEnvSet "TRACK_OLD_EVG_DISABLE" "$(TRACK_OLD_EVG_DISABLE=#)"
< envPaths
epicsEnvSet "EPICS_CA_MAX_ARRAY_BYTES" "150000"
epicsEnvSet "IOCSH_PS1" "$(IOC)> "

###############################################################################
# Register all support components
cd "$(TOP)"
dbLoadDatabase "dbd/eventGenerator.dbd"
eventGenerator_registerRecordDeviceDriver pdbbase
#pid_check("/vxboot/run/$(IOC).pid")

###############################################################################
# Set up ASYN port
# Port name, IP address, priority
eventGeneratorConfigure("EVG01", "$(EVG_ADDRESS)", 0)
asynSetTraceIOMask("EVG01_CMD",-1,0x4)
asynSetTraceMask("EVG01_CMD",-1,0x1)
asynSetTraceIOMask("EVG01_SEQ",-1,0x4)
asynSetTraceMask("EVG01_SEQ",-1,0x1)

###############################################################################
# Load record instances
dbLoadRecords("db/eventGenerator.db","P=$(P),R=$(R),PORT=EVG01")

##############################################################################
# Load additional records
dbLoadRecords("db/iocExit.db","IOC=$(IOC)")
dbLoadRecords("db/asynRecord.db","P=$(IOC),R=:asyn,PORT=EVG01_CMD,ADDR=0,OMAX=0,IMAX=0")

###############################################################################
# Start IOC
cd "${TOP}/iocBoot/${IOC}"
iocInit

###############################################################################
# Update IOC data
dbl >"/vxboot/PVnames/$(IOC)"
epicsEnvShow >"/vxboot/PVenv/$(IOC).softioc"
