@LIBX_AM@

$(call MSGDISPATCHER_GEN,distreboot.msgs,all,dispatch=protected messages=protected)
$(call OPTIONS_GEN,distreboot.opts.H,distreboot.opts.xml)
$(call OPTIONS_GEN,tst.opts.H,tst.opts.xml)

noinst_LIBRARIES=libdistreboot.a
libdistreboot_a_SOURCES=distreboot.C

noinst_PROGRAMS=distreboot connect.tst reboot.tst

distreboot_SOURCES=main.C
distreboot_LDADD=libdistreboot.a -lstasher-client -lx

connect_tst_SOURCES=connect.tst.C tst.C
connect_tst_LDADD=libdistreboot.a -lstasher-client -lx

reboot_tst_SOURCES=reboot.tst.C tst.C
reboot_tst_LDADD=libdistreboot.a -lstasher-client -lx

check-am:
	./connect.tst
	./reboot.tst
