# New ports collection makefile for: stasher-distreboot
# Date created:		September, 2012
# Whom:                 Sam Varshavchik <mrsam@courier-mta.com>
# $FreeBSD$

PORTNAME=		stasher-distreboot
PORTVERSION=		0.04
CATEGORIES=		databases

MAINTAINER=		mrsam@courier-mta.com
COMMENT=		Stasher object repository - distributed reboot service

LIB_DEPENDS=		stasher-client:${PORTSDIR}/databases/stasher
USE_BZIP2=1
USE_GMAKE=yes
GNU_CONFIGURE=1
USE_PKGCONFIG=build

MAN8=			distreboot.8

MAN8COMPRESSED=		yes

USE_RC_SUBR=		distreboot

.include <bsd.port.mk>
