@LIBX_AM@

AUTOMAKE_OPTIONS=foreign dist-bzip2 -Wno-portability

$(call MSGDISPATCHER_GEN,distreboot.msgs,all,dispatch=protected messages=protected)
$(call OPTIONS_GEN,distreboot.opts.H,distreboot.opts.xml)
$(call OPTIONS_GEN,tst.opts.H,tst.opts.xml)

EXTRA_DIST=packaging/fedora/stasher-distreboot.spec \
	packaging/fedora/stasher-distreboot.spec.in \
        packaging/fedora/stasher-distreboot.service.in

noinst_LIBRARIES=libdistreboot.a
libdistreboot_a_SOURCES=distreboot.C distreboot.H

sbin_PROGRAMS=distreboot
noinst_PROGRAMS=connect.tst reboot.tst

distreboot_SOURCES=main.C
distreboot_LDADD=libdistreboot.a -lstasher-client -lx

connect_tst_SOURCES=connect.tst.C tst.C tst.H
connect_tst_LDADD=libdistreboot.a -lstasher-client -lx

reboot_tst_SOURCES=reboot.tst.C tst.C tst.H
reboot_tst_LDADD=libdistreboot.a -lstasher-client -lx

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
