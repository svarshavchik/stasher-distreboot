/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include "distreboot_config.h"
#include <x/options.H>
#include <x/property_value.H>
#include <x/globlock.H>
#include <x/forkexec.H>
#include <x/sysexception.H>
#include "distreboot.H"
#include "distreboot.opts.H"

x::property::value<std::string> reboot_cmd(L"rebootcmd", REBOOTCMD);

class rebootimplObj : public distrebootObj {

public:
	rebootimplObj() {}
	~rebootimplObj() noexcept {}

	void do_reboot() override;
};

void rebootimplObj::do_reboot()
{
	std::string cmd=reboot_cmd.getValue();

	LOG_INFO("Executing " << cmd);

	const char *shell=getenv("SHELL");

	if (!shell || !*shell)
		shell="/bin/sh";

	try {
		x::forkexec(shell, "-c", cmd).spawn_detached();
	} catch (const x::exception &e)
	{
		LOG_FATAL(cmd << ": " << e);
	}
	_exit(1);
}

int main(int argc, char **argv)
{
	distreboot_options opts;

	auto parser=opts.parse(argc, argv);

	if (getuid() != 0)
	{
		std::cerr << "Only root can run \""
			  << reboot_cmd.getValue() << "\"" << std::endl;
		exit(1);
	}

	distreboot::base::args args=distreboot::base::args::create(opts);

	auto ret=x::singletonapp
		::managed([] { return x::ref<rebootimplObj>::create(); }, args);

	if (ret.first->message.size() > 0)
		std::cout << ret.first->message << std::endl;
	exit(ret.first->exitcode);
}
