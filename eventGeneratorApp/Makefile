# Makefile at top of application tree
TOP = ..
include $(TOP)/configure/CONFIG

# Directories to be built, in any order.
# You can replace these wildcards with an explicit list

DIRS := $(DIRS) $(filter-out $(DIRS), $(wildcard *src*))
DIRS := $(DIRS) $(filter-out $(DIRS), $(wildcard *Src*))
DIRS := $(DIRS) $(filter-out $(DIRS), $(wildcard *db*))
DIRS := $(DIRS) $(filter-out $(DIRS), $(wildcard *Db*))
DIRS := $(DIRS) $(filter-out $(DIRS), $(wildcard *iocsh*))

# If the build order matters, add dependency rules like this,
# which specifies that xxxSrc must be built after src:
#xxxSrc_DEPEND_DIRS += src

include $(TOP)/configure/RULES_DIRS
