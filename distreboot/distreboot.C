#include "distreboot_config.h"
#include <iostream>
#include "distreboot.H"

distrebootObj::distrebootObj()
{
}

distrebootObj::~distrebootObj() noexcept
{
}

distrebootObj::ret distrebootObj::run(uid_t uid, argsptr &args)
{
	if (args->stop)
		return ret::create("The daemon is not running", 0);

	args=argsptr();
	try {
		while (1)
			msgqueue->pop()->dispatch();
	} catch (const x::stopexception &e)
	{
	}

	ret retv=ret::create();

	retv->message="Goodbye";
	return retv;
}

void distrebootObj::dispatch(const instance_msg &msg)
{
	msg.flag->processed();

	if (msg.command->start)
	{
		msg.retArg->exitcode=1;
		msg.retArg->message="The daemon is already running";
		return;
	}

	if (msg.command->stop)
		stop();
}
