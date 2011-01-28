
dist_pkgdata_DATA = base.yl ext.yl init.yl

init.yl: Makefile
	echo "(load-cnf '$(pkglibdir)/libylbase.so)" > init.yl
if COND_PCRE
	echo "(set 'pcrelib-path '/usr/local/lib/libpcre.so)" >> init.yl
	echo "(load-cnf '$(pkglibdir)/libylext.so)" >> init.yl
	echo "(unset 'pcrelib-path)" >> init.yl
else
	echo "(load-cnf '$(pkglibdir)/libylext.so)" >> init.yl
endif
	echo "(interpret-file '$(pkgdatadir)/base.yl)" >> init.yl
	echo "(interpret-file '$(pkgdatadir)/ext.yl)" >> init.yl

clean_local:
	rm -f init.yl