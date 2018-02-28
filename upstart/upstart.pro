include(../config.pri)

TEMPLATE = aux

OTHER_FILES += MaliitServer.conf.in MaliitServer-launcher.conf.in

outputFiles(MaliitServer.conf)
outputFiles(MaliitServer-launcher.conf)
