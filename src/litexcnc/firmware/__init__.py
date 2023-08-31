# When a change of is pushed to the develop or master branch the version 
# must be updated. The version is in the format `major.minor.patch`. Guidelines
# for modifications:
# - bugfixes and improvements which do not change the communication protocol
#   i.e. the number and order of fields in `mmio.py` is not changed AND the
#   change does not require a change in the driver, it is sufficient to bump
#   the patch number. 
#   The minor part of the version can be bumped at the discretion of the developer
#   if the change is large enough. Bear in mind that the end-user must then 
#   also upgrade their driver.
# - new components and changes which do affect `mmio.py` or the communication 
#   and THUS lead to modifications of the driver MUST have at least the minor 
#   part of the version being bumped.
# 
# In all cases, the version must also be modified in the header-file `litexcnc.h`
# of the driver. 
__version__ = "1.1.0"
