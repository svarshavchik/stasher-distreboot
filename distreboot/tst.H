#ifndef tst_H
#define tst_H

// Stuff used by regression tests

#include "tst.opts.H"
#include "distreboot.H"
#include <x/threads/run.H>
#include <x/logger.H>
#include <unistd.h>
#include <array>

// A regression test instance

class tst_instance {

	LOG_CLASS_SCOPE;

public:
	distreboot inst;

	typedef decltype(x::run(distreboot::create(),
				getuid(),
				distreboot::base::argsptr::create())
			 ) thread_ret_t;

	thread_ret_t thread_ret;
	tst_instance(const distreboot &instArg);
	tst_instance();
	~tst_instance();

	void run(const distreboot::base::args &args);
	void stop();
	void wait();
};

template<typename tst_type>
class tst_instance_type : public tst_instance {

public:
	tst_instance_type(const distreboot &instArg) : tst_instance(instArg)
	{
	}

	tst_instance_type() : tst_instance(tst_type::create())
	{
	}
};

// Returns the directory node specified.
// Makes a client connection, cleans thing up, before returning.

std::string tst_get_node(const test_options &opts);

// Dummy node name for regression test instance #n

std::string tst_name(size_t n);

template<size_t n, typename tst_type=distreboot> class tst_nodes {

	LOG_CLASS_SCOPE;

public:
	std::array<tst_instance_type<tst_type>, n> instances;

	void start(const test_options &opts)
	{
		alarm(120);
		std::string node=tst_get_node(opts);

		size_t i=0;

		for (auto &inst:instances)
		{
			auto args=distreboot::base::args::create();

			args->start=true;
			args->forcenodename=tst_name(i++);
			inst.run(args);
		}
	}

	void stop()
	{
		for (auto &inst:instances)
			inst.stop();

		for (auto &inst:instances)
		{
			try {
				inst.wait();
			} catch (const x::exception &e)
			{
				LOG_ERROR(e);
				LOG_TRACE(e->backtrace);
			}
		}
	}

	~tst_nodes()
	{
		stop();
	}
};

template<size_t n, typename tst_type> LOG_CLASS_INIT(tst_nodes<n, tst_type>);

#endif
