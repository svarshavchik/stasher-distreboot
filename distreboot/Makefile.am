@LIBCXX_AM@

AUTOMAKE_OPTIONS=foreign dist-bzip2 -Wno-portability

man8_MANS=distreboot.8

$(call THREADMSGDISPATCHER_GEN,distreboot.msgs.H,distreboot.msgs.xml)

$(call OPTIONS_GEN,distreboot.opts.H,distreboot.opts.xml)
$(call OPTIONS_GEN,tst.opts.H,tst.opts.xml)

EXTRA_DIST=COPYING COPYING.GPL INSTALL distreboot.8\
	packaging/fedora/stasher-distreboot.spec \
	packaging/fedora/stasher-distreboot.spec.in \
        packaging/fedora/stasher-distreboot.service.in \
	packaging/freebsd9/databases/stasher-distreboot/Makefile \
	packaging/freebsd9/databases/stasher-distreboot/pkg-descr \
	packaging/freebsd9/databases/stasher-distreboot/pkg-plist \
	packaging/freebsd9/databases/stasher-distreboot/files/distreboot.in

noinst_LIBRARIES=libdistreboot.a
libdistreboot_a_SOURCES=distreboot.C distreboot.H

sbin_PROGRAMS=distreboot
noinst_PROGRAMS=connect.tst reboot.tst

distreboot_SOURCES=main.C
distreboot_LDADD=libdistreboot.a -lstasher-client -lcxx

connect_tst_SOURCES=connect.tst.C tst.C tst.H
connect_tst_LDADD=libdistreboot.a -lstasher-client -lcxx

reboot_tst_SOURCES=reboot.tst.C tst.C tst.H
reboot_tst_LDADD=libdistreboot.a -lstasher-client -lcxx

check-am:
	./connect.tst
	./reboot.tst

APPDIR=$(DESTDIR)@localstatedir@/stasher/apps
APPFILE=$(APPDIR)/distreboot.stasher.courier-mta.com

install-data-hook:
	mkdir -p $(APPDIR)
	echo 'PATH @sbindir@/distreboot' >$(APPFILE)
	echo 'ROOT apps/courier-mta.com/stasher' >>$(APPFILE)

install-exec-hook:
	mkdir -p $(DESTDIR)@sysconfdir@
	echo 'distrebootObj::@log::level=info' >$(DESTDIR)@sysconfdir@/distreboot.properties
	echo 'x::logger::log::handler::default=syslog' >>$(DESTDIR)@sysconfdir@/distreboot.properties
	echo 'x::logger::log::handler::default::format=syslog' >>$(DESTDIR)@sysconfdir@/distreboot.properties
	properties --set=@sysconfdir@/distreboot.properties --nocheckset $(DESTDIR)@sbindir@/distreboot

uninstall-hook:
	rm -f '$(APPFILE)'
	rm -f $(DESTDIR)@sysconfdir@/distreboot.properties

.PHONY: docs

if HAVE_DOCBOOK
docs: html/book.css html/.htaccess

distreboot.8: docbook/distreboot.xml
	rm -rf manpages
	mkdir -p manpages
	xsltproc --nonet -o manpages/ http://docbook.sourceforge.net/release/xsl/current/manpages/docbook.xsl $(srcdir)/docbook/book.xml
	mv -f manpages/* .
	rm -rf manpages

INSTALL: html/book.css
	touch INSTALL

html/.htaccess: docbook/.htaccess html/book.css
	mkdir -p html
	$(INSTALL) -m 0644 $(srcdir)/docbook/.htaccess html/.htaccess
	touch html/.htaccess

html/book.css: $(wildcard docbook/*.xml)
	rm -rf html.tmp
	mkdir html.tmp
	xsltproc --nonet -o html.tmp/ --xinclude \
		--param local.l10n.xml 'document("'`pwd`/docbook/l10.xml'")' \
		$(srcdir)/docbook/docbook.xsl \
		$(srcdir)/docbook/book.xml
	-rm -f html/* 2>/dev/null >/dev/null || :
	mkdir -p html
	mv html.tmp/* html
	rm -rf html.tmp
	links --dump html/INSTALL.html >INSTALL.tmp
	mv -f INSTALL.tmp INSTALL
	$(INSTALL) -m 0644 $(srcdir)/docbook/book.css html
endif

.PHONY: rpm

rpm:
	make dist
	rm -rf rpm/SRPMS/*
	rm -rf rpm/RPMS/*
	rm -rf rpm/BUILD/*
	rm -rf rpm/BUILDROOT/*
	rpmbuild -ta --clean \
		--define "_topdir `pwd`/rpm" \
		--define '_rpmdir %{_topdir}/RPMS' \
		--define '_srcrpmdir %{_topdir}/SRPMS' \
		--define '_sourcedir %{_topdir}/SOURCES' \
		--define '_specdir %{_topdir}/SPECS' \
		--define '_builddir %{_topdir}/BUILD' \
		--define '_build_name_fmt %%{ARCH}/%%{NAME}-%%{VERSION}-%%{RELEASE}.%%{ARCH}.rpm' \
		--define '_tmppath %{_var}/tmp' \
		--define '__spec_prep_pre %{___build_pre}' \
		--define "extrarelease .`date '+%Y%m%d%H%M%S'`" @PACKAGE@-@VERSION@.tar.bz2
	sh $$HOME/repos.sh

www: html
	rsync -a --delete-after html/. ~/www/www.courier-mta.org/stasher-distreboot/.

tag:
	git tag -s -a @PACKAGE@/@VERSION@/`date +%Y%m%d%H%M%S` -m 'Tag version @VERSION@'

scp:
	rm -f @PACKAGE@-@VERSION@.tar.bz2.sig; gpg --detach-sign --default-key mrsam@courier-mta.com @PACKAGE@-@VERSION@.tar.bz2; test -f @PACKAGE@-@VERSION@.tar.bz2.sig || exit 1
	scp @PACKAGE@-@VERSION@.tar.bz2 @PACKAGE@-@VERSION@.tar.bz2.sig fedorahosted.org:libcxx
