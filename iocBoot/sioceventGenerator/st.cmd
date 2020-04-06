#!../../bin/linux-x86_64/eventGenerator

###############################################################################
# Set up environment
epicsEnvSet "P" "$(P=EVG)"
epicsEnvSet "R" "$(R=:)"
epicsEnvSet "EVG_ADDRESS" "$(EVG_ADDRESS=192.168.1.129)"
epicsEnvSet "OLD_EVG_SYS" "$(OLD_EVG_SYS=LI11)"
epicsEnvSet "OLD_EVG_T" "$(OLD_EVG_T=)"
< envPaths
epicsEnvSet "EPICS_CA_MAX_ARRAY_BYTES" "150000"
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

###############################################################################
# Load record instances
dbLoadRecords("db/eventGenerator.db","P=$(P),R=$(R),PORT=EVG01")

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
# Start shadowing old event generator
seq shadowEVG "P=$(P),R=$(R),SYS=$(OLD_EVG_SYS),T=$(OLD_EVG_T)"

