/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

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
	thread_ret=x::start_threadmsgdispatcher(inst, getuid(),
				   distreboot::base::argsptr(args));
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

// Delete any stale object in my sandbox

void tst_clean(const stasher::client &client)
{
	std::set<std::string> set;

	set.insert(distrebootObj::heartbeat_object);
	set.insert(distrebootObj::rebootlist_object);

	auto contents=client->get(set)->objects;

	if (!contents->succeeded)
		throw EXCEPTION(contents->errmsg);

	auto transaction=stasher::client::base::transaction::create();
	bool deleted=false;

	for (auto &dropit:*contents)
	{
		deleted=true;
		transaction->delobj(dropit.first, dropit.second.uuid);
		std::cout << "Removing object: " << dropit.first
			  << std::endl;
	}

	if (deleted)
	{
		auto res=client->put(transaction);

		if (res->status != stasher::req_processed_stat)
			throw EXCEPTION(x::tostring(res->status));
	}
}

std::string tst_status(const distreboot &instance)
{
	auto ret=distrebootObj::ret::create();

	{
		x::destroy_callback::base::guard guard;

		auto status=distrebootObj::args::create();

		x::ref<x::obj> mcguffin=x::ref<x::obj>::create();
		guard(mcguffin); // Exit this scope only when it's done.

		instance->instance(0, status, ret,
				   x::singletonapp::processed::create(),
				   mcguffin);
	}

	return ret->message;
}
