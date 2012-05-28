#ifndef distreboot_H
#define distreboot_H

#include <x/ref.H>
#include <x/fd.H>
#include <x/eventqueuemsgdispatcher.H>
#include <x/managedsingletonapp.H>
#include <x/logger.H>

#include <stasher/client.H>

// The main class. It's an application singleton.

class distrebootObj : public x::eventqueuemsgdispatcherObj {

	LOG_CLASS_SCOPE;

	stasher::client *client; // On the executing thread's stack

	class serverStatusCallbackObj;

	stasher::req_stat_t	connection_status;
	stasher::clusterstate	connection_state;
	stasher::userhelo	connection_info;

	std::string nodename;
	bool connection_state_received;
	bool connection_info_received;
public:

	// Arguments to the singleton's invocation

	class argsObj : virtual public x::obj {

	public:
		bool start; // --start command line option
		bool stop;  // --stop
		std::string node;  // --repo

		argsObj() {}
		argsObj(bool startArg, bool stopArg,
			const std::string &nodeArg)
			: start(startArg), stop(stopArg),
			  node(nodeArg)
		{
		}

		~argsObj() noexcept 
		{
		}

		template<typename iter_type> void serialize(iter_type &iter)
		{
			iter(start);
			iter(stop);
			iter(node);
		}
	};

	typedef x::ref<argsObj> args;
	typedef x::ptr<argsObj> argsptr;

	// Return value from the singleton.

	class retObj : virtual public x::obj {

	public:
		retObj() : exitcode(0) {}
		~retObj() noexcept {}

		std::string message;
		int exitcode;

		retObj(const std::string &messageArg,
		       int exitcodeArg) : message(messageArg),
					  exitcode(exitcodeArg)
		{
		}

		template<typename iter_type> void serialize(iter_type &iter)
		{
			iter(message);
			iter(exitcode);
		}
	};

	typedef x::ref<retObj> ret;

	distrebootObj();
	~distrebootObj() noexcept;

	// The singleton thread.
	ret run(uid_t uid, argsptr &args);

#include "distreboot.msgs.all.H"

};

// distreboot::base provides some convenience typedefs.

class distrebootBase : public x::ptrrefBase {

public:

	typedef distrebootObj::args args;

	typedef distrebootObj::argsptr argsptr;

	typedef distrebootObj::ret ret;
};

typedef x::ref<distrebootObj, distrebootBase> distreboot;

#endif
