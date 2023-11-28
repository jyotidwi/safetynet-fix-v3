if $BOOTMODE; then
    ui_print "- Installing from Magisk / KernelSU app"
else
    ui_print "*********************************************************"
    ui_print "! Install from recovery is NOT supported"
    ui_print "! Recovery sucks"
    ui_print "! Please install from Magisk / KernelSU app"
    abort    "*********************************************************"
fi

# Error on < Android 8
if [ "$API" -lt 26 ]; then
    abort "!!! You can't use this module on Android < 8.0."
fi

# PlayIntegrityFix module is incompatible
if [ -d "/data/adb/modules/playintegrityfix" ]; then
    touch "/data/adb/modules/PlayIntegrityFix/remove"
	ui_print "! PlayIntegrityFix module will be removed in next reboot."
fi