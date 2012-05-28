#include "distreboot_config.h"
#include "tst.H"
#include <stasher/client.H>
#include <sstream>

LOG_CLASS_INIT(tst_instance);

tst_instance::tst_instance()
	: inst(distreboot::create()),
	  thread_ret(thread_ret_t::create())
{
}

tst_instance::tst_instance(const distreboot &instArg)
	: inst(instArg), thread_ret(thread_ret_t::create())
{
}

tst_instance::~tst_instance()
{
	try {
		stop();
		wait();
	} catch (const x::exception &e)
	{
		LOG_ERROR(e);
		LOG_TRACE(e->backtrace);
	}
}

void tst_instance::run(const distreboot::base::args &args)
{
	thread_ret=x::run(inst, getuid(), distreboot::base::argsptr(args));
}

void tst_instance::stop()
{
	inst->stop();
}

void tst_instance::wait()
{
	if (thread_ret->terminated())
		return;

	thread_ret->get();
}

std::string tst_get_node(const test_options &opts)
{
	std::string s=opts.node->value;

	if (s.empty())
	{
		std::set<std::string> nodes;

		stasher::client::base::defaultnodes(nodes);

		if (nodes.size() != 1)
			throw EXCEPTION("--repo option is required");
		s=*nodes.begin();
	}
	return s;
}

std::string tst_name(size_t n)
{
	std::ostringstream o;

	o << "node" << n;

	return o.str();
}
