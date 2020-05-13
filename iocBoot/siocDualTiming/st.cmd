#!../../bin/linux-x86_64/timing

###############################################################################
# Set up environment
epicsEnvSet "FPGA_SIMM_DISABLE" "$(FPGA_SIMM_DISABLE=#)"
epicsEnvSet "P" "$(P=EVG)"
epicsEnvSet "R" "$(R=:)"
epicsEnvSet "T" "$(T=Test)"
epicsEnvSet "EVG_ADDRESS" "$(EVG_ADDRESS=192.168.1.129)"
< envPaths
epicsEnvSet "IOCSH_PS1" "$(IOC)> "
epicsEnvSet "AUTOSAVE_PATH" "$(AUTOSAVE_PATH=/vxboot/ioc_data/$(IOC)/autosave)"

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
dbLoadRecords("db/timing.db","P=$(P),R=$(R),T=$(T)")
dbLoadRecords("db/alsGblPVs.db","T=$(T)")

##############################################################################
# Load additional records
dbLoadRecords("db/iocExit.db","IOC=$(IOC)")
dbLoadRecords("db/asynRecord.db","P=$(IOC),R=:asyn,PORT=EVG01_CMD,ADDR=0,OMAX=0,IMAX=0")

#############################################################################
# Autosave/restore
#var save_restoreDebug 6
set_savefile_path("$(AUTOSAVE_PATH)")
set_requestfile_path("$(AUTOSAVE_PATH)")
set_pass0_restoreFile("autosave.sav")
set_pass1_restoreFile("autosave.sav")
save_restoreSet_status_prefix("$(IOC):")
dbLoadRecords("db/save_restoreStatus.db", "P=$(IOC):")

###############################################################################
# Start IOC
cd "${TOP}/iocBoot/${IOC}"
iocInit

###############################################################################
# Autosave/restore
makeAutosaveFileFromDbInfo("$(AUTOSAVE_PATH)/autosave.req", "autosaveFields_pass0")
create_monitor_set("autosave.req", 300, "")

###############################################################################
# Update IOC data
dbl >"/vxboot/PVnames/$(IOC)"
epicsEnvShow >"/vxboot/PVenv/$(IOC).softioc"

###############################################################################
# Put FPGA I/O records into simulation mode?
$(FPGA_SIMM_DISABLE) <st.simm

###############################################################################
# Start timing sequence program
seq timingSequence "P=$(P),R=$(R),T=$(T)"
