TEMPLATE = subdirs

qtHaveModule(widgets) {
    SUBDIRS = helloworld override
}
