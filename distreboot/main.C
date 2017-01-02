/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include "distreboot_config.h"
#include <x/options.H>
#include <x/globlock.H>
#include <x/forkexec.H>
#include <x/sysexception.H>
#include "distreboot.H"
#include "distreboot.opts.H"

class rebootimplObj : public distrebootObj {

public:
	rebootimplObj() {}
	~rebootimplObj() {}

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

	if (opts.daemon->value)
	{
		auto pipe=x::fd::base::pipe();

		pid_t p=fork();

		if (p < 0)
		{
			std::cerr << "Fork failed" << std::endl;
			exit(1);
		}

		if (p)
		{
			// Pipe needed for the parent to wait until the child
			// process executes setsid(). This makes sure that
			// the child process is running in a new session before
			// the parent exit. It's one syscall, hey, but it's
			// a bone-fide race condition.

			char dummy;

			pipe.second->close();
			pipe.first->read(&dummy, 1);
			exit(0);
		}
		setsid();
		// Child closes both pipe file descriptors
	}

	distreboot::base::args args=distreboot::base::args::create(opts);

	auto ret=x::singletonapp
		::managed([] { return x::ref<rebootimplObj>::create(); }, args);

	if (ret.first->message.size() > 0)
		std::cout << ret.first->message << std::endl;
	exit(ret.first->exitcode);
}
