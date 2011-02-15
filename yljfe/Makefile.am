
# Automake's java support is minimal...
# So, do it manually 
# To build java...
top="$(CURDIR)"/
JTARGET=yljfe.jar
JSRCDIR=jsrc/
JOUTDIR=$(JSRCDIR)bin/
JC=javac
JSOURCES=$(JSRCDIR)YLJEditArea.java $(JSRCDIR)Main.java
JFLAGS=-Xlint:unchecked -d $(JOUTDIR)

AM_CPPFLAGS+= -I$(JAVAINC)

bin_PROGRAMS = yljfe
yljfe_SOURCES = main.c
yljfe_LDADD = ../ylisp/libylisp.a

if COND_STATIC
    yljfe_LDADD+= ../ylbase/libylbase.la ../ylext/libylext.la
endif

yljfe_DEPENDENCIES = $(JTARGET)
dist_pkgdata_DATA = $(JTARGET)


$(JOUTDIR):
	mkdir $(JOUTDIR) > /dev/null 2>&1

$(JTARGET): $(JSOURCES) $(JOUTDIR)
	@echo '$(JC) $(JFLAGS) $(JSOURCES)'; \
	$(JC) $(JFLAGS) $(JSOURCES)
	cd $(JOUTDIR); jar cf $@ *.class; mv $@ $(top)

clean-local:
	rm -rf $(JOUTDIR)
	rm -f $(JTARGET)